/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module eagine.msgbus.services;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.string;
import eagine.core.identifier;
import eagine.core.valid_if;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.logging;
import eagine.core.main_ctx;
import eagine.msgbus.core;
import <fstream>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
resource_server_impl::resource_server_impl(main_ctx_parent parent) noexcept
  : _blobs{
      parent,
      message_id{"eagiRsrces", "fragment"},
      message_id{"eagiRsrces", "fragResend"}} {}
//------------------------------------------------------------------------------
auto resource_server_impl::update(endpoint& bus) noexcept -> work_done {
    some_true something_done{_blobs.update(bus.post_callable())};
    const auto opt_max_size = bus.max_data_size();
    if(opt_max_size) [[likely]] {
        something_done(
          _blobs.process_outgoing(bus.post_callable(), extract(opt_max_size)));
    }

    return something_done;
}
//------------------------------------------------------------------------------
auto resource_server_impl::is_contained(
  const std::filesystem::path& file_path) const noexcept -> bool {
    return starts_with(string_view(file_path), string_view(_root_path));
}
//------------------------------------------------------------------------------
auto resource_server_impl::get_file_path(const url& locator) const noexcept
  -> std::filesystem::path {
    try {
        if(const auto loc_path_str{locator.path_str()}) {
            std::filesystem::path loc_path{
              std::string_view{extract(loc_path_str)}};
            if(_root_path.empty()) {
                if(loc_path.is_absolute()) {
                    return loc_path;
                }
                return std::filesystem::current_path().root_path() / loc_path;
            }
            if(loc_path.is_absolute()) {
                return std::filesystem::canonical(
                  _root_path / loc_path.relative_path());
            }
            return std::filesystem::canonical(_root_path / loc_path);
        }
    } catch(const std::exception&) {
    }
    return {};
}
//------------------------------------------------------------------------------
auto resource_server_impl::has_resource(
  resource_server_intf& impl,
  const message_context&,
  const url& locator) noexcept -> bool {
    if(impl.has_resource(locator)) {
        return true;
    } else if(locator.has_scheme("eagires")) {
        return locator.has_path("/zeroes") || locator.has_path("/ones") ||
               locator.has_path("/random");
    } else if(locator.has_scheme("file")) {
        const auto file_path = get_file_path(locator);
        if(is_contained(file_path)) {
            try {
                const auto stat = std::filesystem::status(file_path);
                return exists(stat) && !is_directory(stat);
            } catch(...) {
            }
        }
    }
    return false;
}
//------------------------------------------------------------------------------
auto resource_server_impl::get_resource(
  resource_server_intf& impl,
  const message_context& ctx,
  const url& locator,
  const identifier_t endpoint_id,
  const message_priority priority)
  -> std::tuple<std::unique_ptr<blob_io>, std::chrono::seconds, message_priority> {
    auto read_io = impl.get_resource_io(endpoint_id, locator);
    if(!read_io) {
        if(locator.has_scheme("eagires")) {
            if(const auto count{locator.argument("count")}) {
                if(const auto bytes{from_string<span_size_t>(extract(count))}) {
                    if(locator.has_path("/random")) {
                        read_io =
                          std::make_unique<random_byte_blob_io>(extract(bytes));
                    } else if(locator.has_path("/zeroes")) {
                        read_io = std::make_unique<single_byte_blob_io>(
                          extract(bytes), 0x0U);
                    } else if(locator.has_path("/ones")) {
                        read_io = std::make_unique<single_byte_blob_io>(
                          extract(bytes), 0x1U);
                    }
                }
            }
        } else if(locator.has_scheme("file")) {
            const auto file_path = get_file_path(locator);
            if(is_contained(file_path)) {
                std::fstream file{file_path, std::ios::in | std::ios::binary};
                if(file.is_open()) {
                    ctx.bus_node()
                      .log_info("sending file ${filePath} to ${target}")
                      .arg("target", endpoint_id)
                      .arg("filePath", "FsPath", file_path);
                    read_io = std::make_unique<file_blob_io>(
                      std::move(file),
                      from_string<span_size_t>(
                        extract_or(locator.argument("offs"), string_view{})),
                      from_string<span_size_t>(
                        extract_or(locator.argument("size"), string_view{})));
                }
            }
        }
    }

    const auto max_time =
      read_io ? impl.get_blob_timeout(endpoint_id, read_io->total_size())
              : std::chrono::seconds{};

    return {
      std::move(read_io),
      max_time,
      impl.get_blob_priority(endpoint_id, priority)};
}
//------------------------------------------------------------------------------
auto resource_server_impl::handle_has_resource_query(
  resource_server_intf& impl,
  const message_context& ctx,
  const stored_message& message) noexcept -> bool {
    std::string url_str;
    if(default_deserialize(url_str, message.content())) [[likely]] {
        const url locator{std::move(url_str)};
        if(has_resource(impl, ctx, locator)) {
            message_view response{message.content()};
            response.setup_response(message);
            ctx.bus_node().post(
              message_id{"eagiRsrces", "hasResurce"}, response);
        } else {
            message_view response{message.content()};
            response.setup_response(message);
            ctx.bus_node().post(
              message_id{"eagiRsrces", "hasNotRsrc"}, response);
        }
    }
    return true;
}
//------------------------------------------------------------------------------
auto resource_server_impl::handle_resource_content_request(
  resource_server_intf& impl,
  const message_context& ctx,
  const stored_message& message) noexcept -> bool {
    std::string url_str;
    if(default_deserialize(url_str, message.content())) [[likely]] {
        const url locator{std::move(url_str)};
        ctx.bus_node()
          .log_info("received content request for ${url}")
          .arg("url", "URL", locator.str());

        auto [read_io, max_time, priority] =
          get_resource(impl, ctx, locator, message.source_id, message.priority);
        if(read_io) {
            _blobs.push_outgoing(
              message_id{"eagiRsrces", "content"},
              message.target_id,
              message.source_id,
              message.sequence_no,
              std::move(read_io),
              max_time,
              priority);
        } else {
            message_view response{};
            response.setup_response(message);
            ctx.bus_node().post(message_id{"eagiRsrces", "notFound"}, response);
            ctx.bus_node()
              .log_info("failed to get I/O object for content request")
              .arg("url", "URL", locator.str());
        }
    } else {
        ctx.bus_node()
          .log_error("failed to deserialize resource content request")
          .arg("content", message.const_content());
    }
    return true;
}
//------------------------------------------------------------------------------
auto resource_server_impl::handle_resource_resend_request(
  resource_server_intf&,
  const message_context&,
  const stored_message& message) noexcept -> bool {
    _blobs.process_resend(message);
    return true;
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus