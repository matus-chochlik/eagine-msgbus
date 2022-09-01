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
import eagine.core.main_ctx;
import eagine.msgbus.core;
import :discovery;
import :host_info;
import <filesystem>;
import <optional>;
import <tuple>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export struct resource_server_intf : interface<resource_server_intf> {
    virtual auto has_resource(const url&) noexcept -> tribool {
        return indeterminate;
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
};
//------------------------------------------------------------------------------
export class resource_server_impl {
public:
    resource_server_impl(main_ctx_parent parent) noexcept;

    auto update(endpoint& bus) noexcept -> work_done;

    void set_file_root(const std::filesystem::path& root_path) noexcept {
        _root_path = std::filesystem::canonical(root_path);
    }

    auto is_contained(const std::filesystem::path& file_path) const noexcept
      -> bool;

    auto get_file_path(const url& locator) const noexcept
      -> std::filesystem::path;

    auto has_resource(
      resource_server_intf& impl,
      const message_context&,
      const url& locator) noexcept -> bool;

    auto get_resource(
      resource_server_intf& impl,
      const message_context& ctx,
      const url& locator,
      const identifier_t endpoint_id,
      const message_priority priority) -> std::
      tuple<std::unique_ptr<blob_io>, std::chrono::seconds, message_priority>;

    auto handle_has_resource_query(
      resource_server_intf& impl,
      const message_context& ctx,
      const stored_message& message) noexcept -> bool;

    auto handle_resource_content_request(
      resource_server_intf& impl,
      const message_context& ctx,
      const stored_message& message) noexcept -> bool;

    auto handle_resource_resend_request(
      resource_server_intf& impl,
      const message_context&,
      const stored_message& message) noexcept -> bool;

private:
    blob_manipulator _blobs;
    std::filesystem::path _root_path{};
};
//------------------------------------------------------------------------------
/// @brief Service providing access to files and/or blobs over the message bus.
/// @ingroup msgbus
/// @see service_composition
/// @see resource_manipulator
export template <typename Base = subscriber>
class resource_server
  : public Base
  , public resource_server_intf {
    using This = resource_server;

public:
    void set_file_root(const std::filesystem::path& root_path) noexcept {
        _impl.set_file_root(root_path);
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        Base::add_method(
          this,
          message_map<
            "eagiRsrces",
            "qryResurce",
            &This::_handle_has_resource_query>{});
        Base::add_method(
          this,
          message_map<
            "eagiRsrces",
            "getContent",
            &This::_handle_resource_content_request>{});
        Base::add_method(
          this,
          message_map<
            "eagiRsrces",
            "fragResend",
            &This::_handle_resource_resend_request>{});
    }

    auto update() noexcept -> work_done {
        some_true something_done{Base::update()};
        something_done(_impl.update(this->bus_node()));

        return something_done;
    }

private:
    auto _handle_has_resource_query(
      const message_context& ctx,
      const stored_message& message) noexcept -> bool {
        return _impl.handle_has_resource_query(*this, ctx, message);
    }

    auto _handle_resource_content_request(
      const message_context& ctx,
      const stored_message& message) noexcept -> bool {
        return _impl.handle_resource_content_request(*this, ctx, message);
    }

    auto _handle_resource_resend_request(
      const message_context& ctx,
      const stored_message& message) noexcept -> bool {
        return _impl.handle_resource_resend_request(*this, ctx, message);
    }

    resource_server_impl _impl{*this};
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
             default_serialize(locator.str(), cover(buffer))}) [[likely]] {
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
          message_map<"eagiRsrces", "hasResurce", &This::_handle_has_resource>{});
        base::add_method(
          this,
          message_map<
            "eagiRsrces",
            "hasNotRsrc",
            &This::_handle_has_not_resource>{});

        base::add_method(
          this,
          message_map<
            "eagiRsrces",
            "fragment",
            &This::_handle_resource_fragment>{});
        base::add_method(
          this,
          message_map<
            "eagiRsrces",
            "notFound",
            &This::_handle_resource_not_found>{});
        base::add_method(
          this,
          message_map<
            "eagiRsrces",
            "fragResend",
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

