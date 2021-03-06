/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:resource_transfer;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.string;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.runtime;
import eagine.core.math;
import eagine.msgbus.core;
import :discovery;
import :host_info;
import <filesystem>;
import <fstream>;
import <optional>;
import <random>;
import <tuple>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export class single_byte_blob_io final : public blob_io {
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
export class random_byte_blob_io final : public blob_io {
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
export class file_blob_io final : public blob_io {
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
/// @brief Service providing access to files and/or blobs over the message bus.
/// @ingroup msgbus
/// @see service_composition
/// @see resource_manipulator
export template <typename Base = subscriber>
class resource_server : public Base {
    using This = resource_server;

public:
    void set_file_root(const std::filesystem::path& root_path) noexcept {
        _root_path = std::filesystem::canonical(root_path);
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(
          this,
          message_map<
            id_v("eagiRsrces"),
            id_v("qryResurce"),
            &This::_handle_has_resource_query>{});
        Base::add_method(
          this,
          message_map<
            id_v("eagiRsrces"),
            id_v("getContent"),
            &This::_handle_resource_content_request>{});
        Base::add_method(
          this,
          message_map<
            id_v("eagiRsrces"),
            id_v("fragResend"),
            &This::_handle_resource_resend_request>{});
    }

    auto update() noexcept -> work_done {
        some_true something_done{Base::update()};

        something_done(_blobs.update(this->bus_node().post_callable()));
        const auto opt_max_size = this->bus_node().max_data_size();
        if(opt_max_size) [[likely]] {
            something_done(_blobs.process_outgoing(
              this->bus_node().post_callable(), extract(opt_max_size)));
        }

        return something_done;
    }

    virtual auto get_resource_io(const identifier_t, const url&)
      -> std::unique_ptr<blob_io> {
        return {};
    }

    virtual auto get_blob_timeout(
      const identifier_t,
      const span_size_t size) noexcept -> std::chrono::seconds {
        return std::chrono::seconds{size / 1024};
    }

    virtual auto get_blob_priority(
      const identifier_t,
      const message_priority priority) noexcept -> message_priority {
        return priority;
    }

private:
    auto _get_file_path(const url& locator) const noexcept
      -> std::filesystem::path {
        try {
            if(const auto loc_path_str{locator.path_str()}) {
                std::filesystem::path loc_path{
                  std::string_view{extract(loc_path_str)}};
                if(_root_path.empty()) {
                    if(loc_path.is_absolute()) {
                        return loc_path;
                    }
                    return std::filesystem::current_path().root_path() /
                           loc_path;
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

    auto _has_resource(const message_context&, const url& locator) noexcept
      -> bool {
        if(locator.has_scheme("eagires")) {
            return locator.has_path("/zeroes") || locator.has_path("/ones") ||
                   locator.has_path("/random");
        } else if(locator.has_scheme("file")) {
            const auto file_path = _get_file_path(locator);
            const bool is_contained =
              starts_with(string_view(file_path), string_view(_root_path));
            if(is_contained) {
                try {
                    const auto stat = std::filesystem::status(file_path);
                    return exists(stat) && !is_directory(stat);
                } catch(...) {
                }
            }
        }
        return false;
    }

    auto _get_resource(
      const message_context& ctx,
      const url& locator,
      const identifier_t endpoint_id,
      const message_priority priority) -> std::
      tuple<std::unique_ptr<blob_io>, std::chrono::seconds, message_priority> {
        auto read_io = get_resource_io(endpoint_id, locator);
        if(!read_io) {
            if(locator.has_scheme("eagires")) {
                if(const auto count{locator.argument("count")}) {
                    if(const auto bytes{
                         from_string<span_size_t>(extract(count))}) {
                        if(locator.has_path("/random")) {
                            read_io = std::make_unique<random_byte_blob_io>(
                              extract(bytes));
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
                const auto file_path = _get_file_path(locator);
                const bool is_contained =
                  starts_with(string_view(file_path), string_view(_root_path));
                if(is_contained) {
                    std::fstream file{
                      file_path, std::ios::in | std::ios::binary};
                    if(file.is_open()) {
                        ctx.bus_node()
                          .log_info("sending file ${filePath} to ${target}")
                          .arg("target", endpoint_id)
                          .arg("filePath", "FsPath", file_path);
                        read_io = std::make_unique<file_blob_io>(
                          std::move(file),
                          from_string<span_size_t>(extract_or(
                            locator.argument("offs"), string_view{})),
                          from_string<span_size_t>(extract_or(
                            locator.argument("size"), string_view{})));
                    }
                }
            }
        }

        const auto max_time =
          read_io ? get_blob_timeout(endpoint_id, read_io->total_size())
                  : std::chrono::seconds{};

        return {
          std::move(read_io),
          max_time,
          get_blob_priority(endpoint_id, priority)};
    }

    auto _handle_has_resource_query(
      const message_context& ctx,
      const stored_message& message) noexcept -> bool {
        std::string url_str;
        if(default_deserialize(url_str, message.content())) [[likely]] {
            const url locator{std::move(url_str)};
            if(_has_resource(ctx, locator)) {
                message_view response{message.content()};
                response.setup_response(message);
                this->bus_node().post(
                  message_id{"eagiRsrces", "hasResurce"}, response);
            } else {
                message_view response{message.content()};
                response.setup_response(message);
                this->bus_node().post(
                  message_id{"eagiRsrces", "hasNotRsrc"}, response);
            }
        }
        return true;
    }

    auto _handle_resource_content_request(
      const message_context& ctx,
      const stored_message& message) noexcept -> bool {
        std::string url_str;
        if(default_deserialize(url_str, message.content())) [[likely]] {
            const url locator{std::move(url_str)};
            ctx.bus_node()
              .log_info("received content request for ${url}")
              .arg("url", "URL", locator.str());

            auto [read_io, max_time, priority] =
              _get_resource(ctx, locator, message.source_id, message.priority);
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
                this->bus_node().post(
                  message_id{"eagiRsrces", "notFound"}, response);
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

    auto _handle_resource_resend_request(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        _blobs.process_resend(message);
        return true;
    }

    blob_manipulator _blobs{
      *this,
      message_id{"eagiRsrces", "fragment"},
      message_id{"eagiRsrces", "fragResend"}};
    std::filesystem::path _root_path{};
};
//------------------------------------------------------------------------------
/// @brief Service manipulating files over the message bus.
/// @ingroup msgbus
/// @see service_composition
/// @see resource_manipulator
export template <typename Base = subscriber>
class resource_manipulator
  : public require_services<Base, host_info_consumer, subscriber_discovery> {

    using This = resource_manipulator;
    using base =
      require_services<Base, host_info_consumer, subscriber_discovery>;

public:
    /// @brief Triggered when a server responds that is has a resource.
    /// @see search_resource
    signal<void(const identifier_t, const url&) noexcept> server_has_resource;

    /// @brief Triggered when a server responds that is has not a resource.
    /// @see search_resource
    signal<void(const identifier_t, const url&) noexcept>
      server_has_not_resource;

    /// @brief Triggered when a resource server appears on the bus.
    signal<void(const identifier_t) noexcept> resource_server_appeared;

    /// @brief Triggered when a resource server dissapears from the bus.
    signal<void(const identifier_t) noexcept> resource_server_lost;

    /// @brief Returns the best-guess of server endpoint id for a URL.
    /// @see query_resource_content
    auto server_endpoint_id(const url& locator) noexcept -> identifier_t {
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

    /// @brief Sends a query to a server checking if it can provide resource.
    /// @see server_has_resource
    /// @see server_has_not_resource
    auto search_resource(
      const identifier_t endpoint_id,
      const url& locator) noexcept -> std::optional<message_sequence_t> {
        auto buffer = default_serialize_buffer_for(locator.str());

        if(const auto serialized{
             default_serialize(locator.str(), cover(buffer))}) {
            const auto msg_id{message_id{"eagiRsrces", "qryResurce"}};
            message_view message{extract(serialized)};
            message.set_target_id(endpoint_id);
            this->bus_node().set_next_sequence_id(msg_id, message);
            this->bus_node().post(msg_id, message);
            return {message.sequence_no};
        }
        return {};
    }

    /// @brief Sends a query to the bus checking if any server can provide resource.
    /// @see server_has_resource
    /// @see server_has_not_resource
    auto search_resource(const url& locator) noexcept
      -> std::optional<message_sequence_t> {
        return search_resource(broadcast_endpoint_id(), locator);
    }

    /// @brief Requests the contents of the file with the specified URL.
    auto query_resource_content(
      identifier_t endpoint_id,
      const url& locator,
      std::shared_ptr<blob_io> write_io,
      const message_priority priority,
      const std::chrono::seconds max_time)
      -> std::optional<message_sequence_t> {
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
            this->bus_node().set_next_sequence_id(msg_id, message);
            this->bus_node().post(msg_id, message);
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

    /// @brief Requests the contents of the file with the specified URL.
    /// @see server_endpoint_id
    auto query_resource_content(
      const url& locator,
      std::shared_ptr<blob_io> write_io,
      const message_priority priority,
      const std::chrono::seconds max_time)
      -> std::optional<message_sequence_t> {
        return query_resource_content(
          server_endpoint_id(locator),
          locator,
          std::move(write_io),
          priority,
          max_time);
    }

protected:
    using base::base;

    void init() noexcept {
        base::init();

        connect<&This::_handle_alive>(this, this->reported_alive);
        connect<&This::_handle_subscribed>(this, this->subscribed);
        connect<&This::_handle_unsubscribed>(this, this->unsubscribed);
        connect<&This::_handle_unsubscribed>(this, this->not_subscribed);
        connect<&This::_handle_host_id_received>(this, this->host_id_received);
        connect<&This::_handle_hostname_received>(
          this, this->hostname_received);
    }

    void add_methods() noexcept {
        base::add_methods();

        base::add_method(
          this,
          message_map<
            id_v("eagiRsrces"),
            id_v("hasResurce"),
            &This::_handle_has_resource>{});
        base::add_method(
          this,
          message_map<
            id_v("eagiRsrces"),
            id_v("hasNotRsrc"),
            &This::_handle_has_not_resource>{});

        base::add_method(
          this,
          message_map<
            id_v("eagiRsrces"),
            id_v("fragment"),
            &This::_handle_resource_fragment>{});
        base::add_method(
          this,
          message_map<
            id_v("eagiRsrces"),
            id_v("notFound"),
            &This::_handle_resource_not_found>{});
        base::add_method(
          this,
          message_map<
            id_v("eagiRsrces"),
            id_v("fragResend"),
            &This::_handle_resource_resend_request>{});
    }

    auto update() noexcept -> work_done {
        some_true something_done{base::update()};

        something_done(_blobs.handle_complete() > 0);

        if(_search_servers) {
            this->bus_node().query_subscribers_of(
              message_id{"eagiRsrces", "getContent"});
            something_done();
        }

        return something_done;
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
                resource_server_appeared(sub_info.endpoint_id);
            }
            auto& svr_info = std::get<1>(*spos);
            svr_info.last_report_time = std::chrono::steady_clock::now();
        }
    }

    void _remove_server(const identifier_t endpoint_id) noexcept {
        const auto spos = _server_endpoints.find(endpoint_id);
        if(spos != _server_endpoints.end()) {
            resource_server_lost(endpoint_id);
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
            server_has_resource(message.source_id, url{std::move(url_str)});
        }
        return true;
    }

    auto _handle_has_not_resource(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        std::string url_str;
        if(default_deserialize(url_str, message.content())) [[likely]] {
            server_has_not_resource(message.source_id, url{std::move(url_str)});
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

    blob_manipulator _blobs{
      *this,
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
} // namespace eagine::msgbus

