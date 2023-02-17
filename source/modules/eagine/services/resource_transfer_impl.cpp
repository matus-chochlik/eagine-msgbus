/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

module eagine.msgbus.services;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.string;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.valid_if;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.logging;
import eagine.core.math;
import eagine.core.main_ctx;
import eagine.msgbus.core;
import <fstream>;
import <random>;
import <vector>;
import <tuple>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// single_byte_blob_io
//------------------------------------------------------------------------------
class single_byte_blob_io final : public source_blob_io {
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
class random_byte_blob_io final : public source_blob_io {
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
class file_blob_io final
  : public source_blob_io
  , public target_blob_io {
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

    auto store_fragment(
      const span_size_t offs,
      const memory::const_block src,
      const blob_info&) noexcept -> bool final {
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
      const message_info&,
      const blob_info&) noexcept final {
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
        auto& bus = base.bus_node();
        some_true something_done{
          _blobs.update(bus.post_callable(), min_connection_data_size)};
        if(_should_send_outgoing) {
            something_done(_blobs.process_outgoing(
              bus.post_callable(), min_connection_data_size, 2));
            _should_send_outgoing.reset();
        }

        return something_done;
    }

    void average_message_age(
      const std::chrono::microseconds age) noexcept final {
        _should_send_outgoing.set_duration(std::min(
          std::chrono::microseconds{50} + age / 16,
          std::chrono::microseconds{50000}));
    }

    void set_file_root(const std::filesystem::path& root_path) noexcept final {
        _root_path = std::filesystem::canonical(root_path);
    }

    void notify_resource_available(const string_view locator) noexcept final;

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
      const message_priority priority)
      -> std::tuple<
        std::unique_ptr<source_blob_io>,
        std::chrono::seconds,
        message_priority>;

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
    timeout _should_send_outgoing{std::chrono::microseconds{1}};
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
void resource_server_impl::notify_resource_available(
  const string_view locator) noexcept {
    auto buffer = default_serialize_buffer_for(locator);

    if(const auto serialized{default_serialize(locator, cover(buffer))})
      [[likely]] {
        const auto msg_id{message_id{"eagiRsrces", "available"}};
        message_view message{extract(serialized)};
        message.set_target_id(broadcast_endpoint_id());
        base.bus_node().post(msg_id, message);
    }
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
  const message_context&,
  const url& locator) noexcept -> bool {
    if(const auto has_res{driver.has_resource(locator)}) {
        return true;
    } else if(has_res.is(indeterminate)) {
        if(locator.has_scheme("eagires")) {
            return locator.has_path("/zeroes") or locator.has_path("/ones") or
                   locator.has_path("/random");
        } else if(locator.has_scheme("file")) {
            const auto file_path = get_file_path(locator);
            if(is_contained(file_path)) {
                try {
                    const auto stat = std::filesystem::status(file_path);
                    return exists(stat) and not is_directory(stat);
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
  const message_priority priority) -> std::
  tuple<std::unique_ptr<source_blob_io>, std::chrono::seconds, message_priority> {
    auto read_io = driver.get_resource_io(endpoint_id, locator);
    if(not read_io) {
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
// resource_manipulator_impl
//------------------------------------------------------------------------------
class resource_manipulator_impl : public resource_manipulator_intf {
public:
    resource_manipulator_impl(
      subscriber& sub,
      resource_manipulator_signals& sigs) noexcept
      : base{sub}
      , signals{sigs} {}

    void init(
      subscriber_discovery_signals& discovery,
      host_info_consumer_signals& host_info) noexcept final {
        connect<&resource_manipulator_impl::_handle_alive>(
          this, discovery.reported_alive);
        connect<&resource_manipulator_impl::_handle_subscribed>(
          this, discovery.subscribed);
        connect<&resource_manipulator_impl::_handle_unsubscribed>(
          this, discovery.unsubscribed);
        connect<&resource_manipulator_impl::_handle_unsubscribed>(
          this, discovery.not_subscribed);
        connect<&resource_manipulator_impl::_handle_host_id_received>(
          this, host_info.host_id_received);
        connect<&resource_manipulator_impl::_handle_hostname_received>(
          this, host_info.hostname_received);
    }

    void add_methods() noexcept final {
        base.add_method(
          this,
          message_map<
            "eagiRsrces",
            "hasResurce",
            &resource_manipulator_impl::_handle_has_resource>{});
        base.add_method(
          this,
          message_map<
            "eagiRsrces",
            "hasNotRsrc",
            &resource_manipulator_impl::_handle_has_not_resource>{});

        base.add_method(
          this,
          message_map<
            "eagiRsrces",
            "fragment",
            &resource_manipulator_impl::_handle_resource_fragment>{});
        base.add_method(
          this,
          message_map<
            "eagiRsrces",
            "notFound",
            &resource_manipulator_impl::_handle_resource_not_found>{});
        base.add_method(
          this,
          message_map<
            "eagiRsrces",
            "fragResend",
            &resource_manipulator_impl::_handle_resource_resend_request>{});
        base.add_method(
          this,
          message_map<
            "eagiRsrces",
            "available",
            &resource_manipulator_impl::_handle_resource_available>{});
    }

    auto update() noexcept -> work_done final {
        some_true something_done{};
        something_done(_blobs.handle_complete() > 0);
        something_done(_blobs.update(
          base.bus_node().post_callable(), min_connection_data_size));

        if(_search_servers) {
            base.bus_node().query_subscribers_of(
              message_id{"eagiRsrces", "getContent"});
            something_done();
        }

        return something_done;
    }

    auto server_endpoint_id(const url& locator) noexcept -> identifier_t final {
        if(locator.has_scheme("eagimbe")) {
            if(const auto opt_id{from_string<identifier_t>(
                 extract_or(locator.host(), string_view{}))}) {
                const auto endpoint_id = extract(opt_id);
                const auto spos = _server_endpoints.find(endpoint_id);
                if(spos != _server_endpoints.end()) {
                    return endpoint_id;
                }
            }
        } else if(locator.has_scheme("eagimbh")) {
            if(const auto hostname{locator.host()}) {
                const auto hpos = _hostname_to_endpoint.find(extract(hostname));
                if(hpos != _hostname_to_endpoint.end()) {
                    for(const auto endpoint_id : std::get<1>(*hpos)) {
                        const auto spos = _server_endpoints.find(endpoint_id);
                        if(spos != _server_endpoints.end()) {
                            return endpoint_id;
                        }
                    }
                }
            }
        }
        return broadcast_endpoint_id();
    }

    auto search_resource(
      const identifier_t endpoint_id,
      const url& locator) noexcept -> std::optional<message_sequence_t> final {
        auto buffer = default_serialize_buffer_for(locator.str());

        if(const auto serialized{
             default_serialize(locator.str(), cover(buffer))}) [[likely]] {
            const auto msg_id{message_id{"eagiRsrces", "qryResurce"}};
            message_view message{extract(serialized)};
            message.set_target_id(endpoint_id);
            base.bus_node().set_next_sequence_id(msg_id, message);
            base.bus_node().post(msg_id, message);
            return {message.sequence_no};
        }
        return {};
    }

    auto query_resource_content(
      identifier_t endpoint_id,
      const url& locator,
      std::shared_ptr<target_blob_io> write_io,
      const message_priority priority,
      const std::chrono::seconds max_time)
      -> std::optional<message_sequence_t> final {
        auto buffer = default_serialize_buffer_for(locator.str());

        if(endpoint_id == broadcast_endpoint_id()) {
            endpoint_id = server_endpoint_id(locator);
        }

        if(const auto serialized{
             default_serialize(locator.str(), cover(buffer))}) {
            const auto msg_id{message_id{"eagiRsrces", "getContent"}};
            message_view message{extract(serialized)};
            message.set_target_id(endpoint_id);
            message.set_priority(priority);
            base.bus_node().set_next_sequence_id(msg_id, message);
            base.bus_node().post(msg_id, message);
            _blobs.expect_incoming(
              message_id{"eagiRsrces", "content"},
              endpoint_id,
              message.sequence_no,
              std::move(write_io),
              max_time);
            return {message.sequence_no};
        }
        return {};
    }

private:
    void _handle_alive(const subscriber_info& sub_info) noexcept {
        const auto pos = _server_endpoints.find(sub_info.endpoint_id);
        if(pos != _server_endpoints.end()) {
            auto& svr_info = std::get<1>(*pos);
            svr_info.last_report_time = std::chrono::steady_clock::now();
        }
    }

    void _handle_subscribed(
      const subscriber_info& sub_info,
      const message_id msg_id) noexcept {
        if(msg_id == message_id{"eagiRsrces", "getContent"}) {
            auto spos = _server_endpoints.find(sub_info.endpoint_id);
            if(spos == _server_endpoints.end()) {
                spos = _server_endpoints.emplace(sub_info.endpoint_id).first;
                signals.resource_server_appeared(sub_info.endpoint_id);
            }
            auto& svr_info = std::get<1>(*spos);
            svr_info.last_report_time = std::chrono::steady_clock::now();
        }
    }

    void _remove_server(const identifier_t endpoint_id) noexcept {
        const auto spos = _server_endpoints.find(endpoint_id);
        if(spos != _server_endpoints.end()) {
            signals.resource_server_lost(endpoint_id);
            _server_endpoints.erase(spos);
        }
        for(auto& entry : _host_id_to_endpoint) {
            std::get<1>(entry).erase(endpoint_id);
        }
        _host_id_to_endpoint.erase_if(
          [](const auto& entry) { return std::get<1>(entry).empty(); });

        for(auto& entry : _hostname_to_endpoint) {
            std::get<1>(entry).erase(endpoint_id);
        }
        _hostname_to_endpoint.erase_if(
          [](const auto& entry) { return std::get<1>(entry).empty(); });
    }

    void _handle_unsubscribed(
      const subscriber_info& sub_info,
      const message_id msg_id) noexcept {
        if(msg_id == message_id{"eagiRsrces", "getContent"}) {
            _remove_server(sub_info.endpoint_id);
        }
    }

    void _handle_host_id_received(
      const result_context& ctx,
      const valid_if_positive<host_id_t>& host_id) noexcept {
        if(host_id) {
            _host_id_to_endpoint[extract(host_id)].insert(ctx.source_id());
        }
    }

    void _handle_hostname_received(
      const result_context& ctx,
      const valid_if_not_empty<std::string>& hostname) noexcept {
        if(hostname) {
            _hostname_to_endpoint[extract(hostname)].insert(ctx.source_id());
        }
    }

    auto _handle_has_resource(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        std::string url_str;
        if(default_deserialize(url_str, message.content())) [[likely]] {
            const url locator{std::move(url_str)};
            signals.server_has_resource(message.source_id, locator);
        }
        return true;
    }

    auto _handle_has_not_resource(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        std::string url_str;
        if(default_deserialize(url_str, message.content())) [[likely]] {
            const url locator{std::move(url_str)};
            signals.server_has_not_resource(message.source_id, locator);
        }
        return true;
    }

    auto _handle_resource_fragment(
      [[maybe_unused]] const message_context& ctx,
      const stored_message& message) noexcept -> bool {
        _blobs.process_incoming(message);
        return true;
    }

    auto _handle_resource_not_found(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        _blobs.cancel_incoming(message.sequence_no);
        return true;
    }

    auto _handle_resource_resend_request(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        _blobs.process_resend(message);
        return true;
    }

    auto _handle_resource_available(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        std::string url_str;
        if(default_deserialize(url_str, message.content())) [[likely]] {
            const url locator{std::move(url_str)};
            base.bus_node()
              .log_info("resource ${locator} is available at ${source}")
              .arg("source", message.source_id)
              .arg("locator", locator.str());
            signals.resource_appeared(message.source_id, locator);
        }
        return true;
    }

    subscriber& base;
    resource_manipulator_signals& signals;
    blob_manipulator _blobs{
      base.bus_node(),
      message_id{"eagiRsrces", "fragment"},
      message_id{"eagiRsrces", "fragResend"}};

    resetting_timeout _search_servers{std::chrono::seconds{5}, nothing};

    flat_map<std::string, flat_set<identifier_t>, str_view_less>
      _hostname_to_endpoint;
    flat_map<identifier_t, flat_set<identifier_t>> _host_id_to_endpoint;

    struct _server_info {
        std::chrono::steady_clock::time_point last_report_time{};
    };

    flat_map<identifier_t, _server_info> _server_endpoints;
};
//------------------------------------------------------------------------------
auto make_resource_manipulator_impl(
  subscriber& base,
  resource_manipulator_signals& sigs)
  -> std::unique_ptr<resource_manipulator_intf> {
    return std::make_unique<resource_manipulator_impl>(base, sigs);
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
