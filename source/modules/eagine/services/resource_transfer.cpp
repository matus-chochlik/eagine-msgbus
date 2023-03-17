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
import eagine.core.utility;
import eagine.core.runtime;
import eagine.msgbus.core;
import :discovery;
import :host_info;
import std;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export struct resource_server_driver : interface<resource_server_driver> {
    virtual auto has_resource(const url&) noexcept -> tribool {
        return indeterminate;
    }

    virtual auto get_resource_io(const identifier_t, const url&)
      -> std::unique_ptr<source_blob_io> {
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
struct resource_server_intf : interface<resource_server_intf> {
    virtual void add_methods() noexcept = 0;
    virtual auto update() noexcept -> work_done = 0;

    virtual void average_message_age(
      const std::chrono::microseconds) noexcept = 0;

    virtual void set_file_root(
      const std::filesystem::path& root_path) noexcept = 0;

    virtual void notify_resource_available(
      const string_view locator) noexcept = 0;
};
//------------------------------------------------------------------------------
auto make_resource_server_impl(subscriber&, resource_server_driver&)
  -> std::unique_ptr<resource_server_intf>;
//------------------------------------------------------------------------------
/// @brief Service providing access to files and/or blobs over the message bus.
/// @ingroup msgbus
/// @see service_composition
/// @see resource_manipulator
export template <typename Base = subscriber>
class resource_server : public Base {
    using This = resource_server;

    resource_server_driver _default_driver;

public:
    void average_message_age(const std::chrono::microseconds age) noexcept {
        _impl->average_message_age(age);
    }

    void set_file_root(const std::filesystem::path& root_path) noexcept {
        _impl->set_file_root(root_path);
    }

    void notify_resource_available(const string_view locator) noexcept {
        _impl->notify_resource_available(locator);
    }

protected:
    resource_server(endpoint& bus, resource_server_driver& drvr) noexcept
      : Base{bus}
      , _impl{make_resource_server_impl(*this, drvr)} {}

    resource_server(endpoint& bus) noexcept
      : resource_server{bus, _default_driver} {}

    void add_methods() noexcept {
        Base::add_methods();
        _impl->add_methods();
    }

    auto update() noexcept -> work_done {
        some_true something_done{Base::update()};
        something_done(_impl->update());
        return something_done;
    }

private:
    const std::unique_ptr<resource_server_intf> _impl;
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the resource manipulator service.
/// @ingroup msgbus
/// @see resource_manipulator
export struct resource_manipulator_signals {
    /// @brief Triggered when a server responds that is has a resource.
    /// @see search_resource
    signal<void(const identifier_t, const url&) noexcept> server_has_resource;

    /// @brief Triggered when a server responds that is has not a resource.
    /// @see search_resource
    signal<void(const identifier_t, const url&) noexcept>
      server_has_not_resource;

    /// @brief Triggered when a resource becomes available.
    signal<void(const identifier_t, const url&) noexcept> resource_appeared;

    /// @brief Triggered when a resource server appears on the bus.
    signal<void(const identifier_t) noexcept> resource_server_appeared;

    /// @brief Triggered when a resource server dissapears from the bus.
    signal<void(const identifier_t) noexcept> resource_server_lost;
};
//------------------------------------------------------------------------------
struct resource_manipulator_intf : interface<resource_manipulator_intf> {
    virtual void init(
      subscriber_discovery_signals&,
      host_info_consumer_signals&) noexcept = 0;

    virtual void add_methods() noexcept = 0;
    virtual auto update() noexcept -> work_done = 0;

    virtual auto server_endpoint_id(const url& locator) noexcept
      -> identifier_t = 0;

    virtual auto search_resource(
      const identifier_t endpoint_id,
      const url& locator) noexcept -> std::optional<message_sequence_t> = 0;

    virtual auto query_resource_content(
      identifier_t endpoint_id,
      const url& locator,
      std::shared_ptr<target_blob_io> write_io,
      const message_priority priority,
      const std::chrono::seconds max_time)
      -> std::optional<message_sequence_t> = 0;
};
//------------------------------------------------------------------------------
auto make_resource_manipulator_impl(subscriber&, resource_manipulator_signals&)
  -> std::unique_ptr<resource_manipulator_intf>;
//------------------------------------------------------------------------------
/// @brief Service manipulating files over the message bus.
/// @ingroup msgbus
/// @see service_composition
/// @see resource_server
export template <typename Base = subscriber>
class resource_manipulator
  : public require_services<Base, host_info_consumer, subscriber_discovery>
  , public resource_manipulator_signals {

    using This = resource_manipulator;
    using base =
      require_services<Base, host_info_consumer, subscriber_discovery>;

public:
    /// @brief Returns the best-guess of server endpoint id for a URL.
    /// @see query_resource_content
    auto server_endpoint_id(const url& locator) noexcept -> identifier_t {
        return _impl->server_endpoint_id(locator);
    }

    /// @brief Sends a query to a server checking if it can provide resource.
    /// @see server_has_resource
    /// @see server_has_not_resource
    auto search_resource(
      const identifier_t endpoint_id,
      const url& locator) noexcept -> std::optional<message_sequence_t> {
        return _impl->search_resource(endpoint_id, locator);
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
      std::shared_ptr<target_blob_io> write_io,
      const message_priority priority,
      const std::chrono::seconds max_time)
      -> std::optional<message_sequence_t> {
        return _impl->query_resource_content(
          endpoint_id, locator, std::move(write_io), priority, max_time);
    }

    /// @brief Requests the contents of the file with the specified URL.
    auto query_resource_content(
      identifier_t endpoint_id,
      const url& locator,
      std::shared_ptr<target_blob_io> write_io,
      const message_priority priority,
      timeout& max_timeout) -> std::optional<message_sequence_t> {
        return _impl->query_resource_content(
          endpoint_id,
          locator,
          std::move(write_io),
          priority,
          std::chrono::ceil<std::chrono::seconds>(max_timeout.period()));
    }

    /// @brief Requests the contents of the file with the specified URL.
    /// @see server_endpoint_id
    auto query_resource_content(
      const url& locator,
      std::shared_ptr<target_blob_io> write_io,
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
        _impl->init(*this, *this);
    }

    void add_methods() noexcept {
        base::add_methods();
        _impl->add_methods();
    }

    auto update() noexcept -> work_done {
        some_true something_done{base::update()};
        something_done(_impl->update());
        return something_done;
    }

private:
    const std::unique_ptr<resource_manipulator_intf> _impl{
      make_resource_manipulator_impl(*this, *this)};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

