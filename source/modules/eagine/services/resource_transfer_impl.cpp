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
import eagine.core.math;
import eagine.core.main_ctx;
import eagine.msgbus.core;
import <fstream>;
import <random>;
import <tuple>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// single_byte_blob_io
//------------------------------------------------------------------------------
class single_byte_blob_io final : public blob_io {
public:
    single_byte_blob_io(const span_size_t size, const byte value) noexcept
      : _size{size}
      , _value{value} {}

    auto total_size() noexcept -> span_size_t final {
        return _size;
    }

    auto fetch_fragment(const span_size_t offs, memory::block dst) noexcept
      -> span_size_t final {
        return fill(head(dst, _size - offs), _value).size();
    }

private:
    span_size_t _size;
    byte _value;
};
//------------------------------------------------------------------------------
// random_byte_blob_io
//------------------------------------------------------------------------------
class random_byte_blob_io final : public blob_io {
public:
    random_byte_blob_io(span_size_t size) noexcept
      : _size{size}
      , _re{std::random_device{}()} {}

    auto total_size() noexcept -> span_size_t final {
        return _size;
    }

    auto fetch_fragment(const span_size_t offs, memory::block dst) noexcept
      -> span_size_t final {
        return fill_with_random_bytes(head(dst, _size - offs), _re).size();
    }

private:
    span_size_t _size;
    std::default_random_engine _re;
};
//------------------------------------------------------------------------------
// file_blob_io
//------------------------------------------------------------------------------
class file_blob_io final : public blob_io {
public:
    file_blob_io(
      std::fstream file,
      std::optional<span_size_t> offs,
      std::optional<span_size_t> size) noexcept
      : _file{std::move(file)} {
        _file.seekg(0, std::ios::end);
        _size = limit_cast<span_size_t>(_file.tellg());
        if(size) {
            _size = _size ? math::minimum(_size, extract(size)) : extract(size);
        }
        if(offs) {
            _offs = math::minimum(_size, extract(offs));
        }
    }

    auto is_at_eod(const span_size_t offs) noexcept -> bool final {
        return offs >= total_size();
    }

    auto total_size() noexcept -> span_size_t final {
        return _size - _offs;
    }

    auto fetch_fragment(const span_size_t offs, memory::block dst) noexcept
      -> span_size_t final {
        _file.seekg(_offs + offs, std::ios::beg);
        return limit_cast<span_size_t>(
          read_from_stream(_file, head(dst, _size - _offs - offs)).gcount());
    }

    auto store_fragment(const span_size_t offs, memory::const_block src) noexcept
      -> bool final {
        _file.seekg(_offs + offs, std::ios::beg);
        return write_to_stream(_file, head(src, _size - _offs - offs)).good();
    }

    auto check_stored(const span_size_t, memory::const_block) noexcept
      -> bool final {
        return true;
    }

    void handle_finished(
      const message_id,
      const message_age,
      const message_info&) noexcept final {
        _file.close();
    }

    void handle_cancelled() noexcept final {
        _file.close();
    }

private:
    std::fstream _file;
    span_size_t _offs{0};
    span_size_t _size{0};
};
//------------------------------------------------------------------------------
// resource_server_impl
//------------------------------------------------------------------------------
class resource_server_impl : public resource_server_intf {
public:
    resource_server_impl(subscriber& sub, resource_server_driver& drvr) noexcept;

    void add_methods() noexcept final {
        base.add_method(
          this,
          message_map<
            "eagiRsrces",
            "qryResurce",
            &resource_server_impl::_handle_has_resource_query>{});
        base.add_method(
          this,
          message_map<
            "eagiRsrces",
            "getContent",
            &resource_server_impl::_handle_resource_content_request>{});
        base.add_method(
          this,
          message_map<
            "eagiRsrces",
            "fragResend",
            &resource_server_impl::_handle_resource_resend_request>{});
    }
    auto update() noexcept -> work_done final {
        {
            auto& bus = base.bus_node();
            some_true something_done{_blobs.update(bus.post_callable())};
            const auto opt_max_size = bus.max_data_size();
            if(opt_max_size) [[likely]] {
                something_done(_blobs.process_outgoing(
                  bus.post_callable(), extract(opt_max_size)));
            }

            return something_done;
        }
    }

    void set_file_root(const std::filesystem::path& root_path) noexcept final {
        _root_path = std::filesystem::canonical(root_path);
    }

    auto is_contained(const std::filesystem::path& file_path) const noexcept
      -> bool;

    auto get_file_path(const url& locator) const noexcept
      -> std::filesystem::path;

    auto has_resource(const message_context&, const url& locator) noexcept
      -> bool;

    auto get_resource(
      const message_context& ctx,
      const url& locator,
      const identifier_t endpoint_id,
      const message_priority priority) -> std::
      tuple<std::unique_ptr<blob_io>, std::chrono::seconds, message_priority>;

private:
    auto _handle_has_resource_query(
      const message_context& ctx,
      const stored_message& message) noexcept -> bool;

    auto _handle_resource_content_request(
      const message_context& ctx,
      const stored_message& message) noexcept -> bool;

    auto _handle_resource_resend_request(
      const message_context& ctx,
      const stored_message& message) noexcept -> bool;

    subscriber& base;
    resource_server_driver& driver;
    blob_manipulator _blobs;
    std::filesystem::path _root_path{};
};
//------------------------------------------------------------------------------
auto make_resource_server_impl(subscriber& sub, resource_server_driver& drvr)
  -> std::unique_ptr<resource_server_intf> {
    return std::make_unique<resource_server_impl>(sub, drvr);
}
//------------------------------------------------------------------------------
resource_server_impl::resource_server_impl(
  subscriber& sub,
  resource_server_driver& drvr) noexcept
  : base{sub}
  , driver{drvr}
  , _blobs{
      base,
      message_id{"eagiRsrces", "fragment"},
      message_id{"eagiRsrces", "fragResend"}} {}
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
  const message_context&,
  const url& locator) noexcept -> bool {
    if(const auto has_res{driver.has_resource(locator)}) {
        return true;
    } else if(has_res.is(indeterminate)) {
        if(locator.has_scheme("eagires")) {
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
    }
    return false;
}
//------------------------------------------------------------------------------
auto resource_server_impl::get_resource(
  const message_context& ctx,
  const url& locator,
  const identifier_t endpoint_id,
  const message_priority priority)
  -> std::tuple<std::unique_ptr<blob_io>, std::chrono::seconds, message_priority> {
    auto read_io = driver.get_resource_io(endpoint_id, locator);
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
      read_io ? driver.get_blob_timeout(endpoint_id, read_io->total_size())
              : std::chrono::seconds{};

    return {
      std::move(read_io),
      max_time,
      driver.get_blob_priority(endpoint_id, priority)};
}
//------------------------------------------------------------------------------
auto resource_server_impl::_handle_has_resource_query(
  const message_context& ctx,
  const stored_message& message) noexcept -> bool {
    std::string url_str;
    if(default_deserialize(url_str, message.content())) [[likely]] {
        const url locator{std::move(url_str)};
        if(has_resource(ctx, locator)) {
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
auto resource_server_impl::_handle_resource_content_request(
  const message_context& ctx,
  const stored_message& message) noexcept -> bool {
    std::string url_str;
    if(default_deserialize(url_str, message.content())) [[likely]] {
        const url locator{std::move(url_str)};
        ctx.bus_node()
          .log_info("received content request for ${url}")
          .tag("rsrcCntReq")
          .arg("url", "URL", locator.str());

        auto [read_io, max_time, priority] =
          get_resource(ctx, locator, message.source_id, message.priority);
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
auto resource_server_impl::_handle_resource_resend_request(
  const message_context&,
  const stored_message& message) noexcept -> bool {
    _blobs.process_resend(message);
    return true;
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
