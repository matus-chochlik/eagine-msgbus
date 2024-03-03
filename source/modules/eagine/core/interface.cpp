/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:interface;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.valid_if;
import eagine.core.utility;
import :types;
import :message;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Basic interface for retrieving message bus connection information.
/// @ingroup msgbus
/// @see connection
export struct connection_info : interface<connection_info> {

    /// @brief Returns the connection kind.
    virtual auto kind() -> connection_kind = 0;

    /// @brief Returns the connection address kind.
    virtual auto addr_kind() -> connection_addr_kind = 0;

    /// @brief Returns a description identifier of the implementation.
    virtual auto type_id() -> identifier = 0;
};
//------------------------------------------------------------------------------
/// @brief Interface for message bus connections.
/// @ingroup msgbus
/// @see connection_user
/// @see acceptor
export struct connection : connection_info {

    /// @brief Alias for fetch handler callable reference type.
    using fetch_handler = callable_ref<
      bool(const message_id, const message_age, const message_view&) noexcept>;

    /// @brief Updates the internal state of the connection (called repeatedly).
    /// @see send
    /// @see fetch_messages
    virtual auto update() noexcept -> work_done {
        return {};
    }

    /// @brief Cleans up the connection before destroying it.
    virtual void cleanup() noexcept {}

    /// @brief Checks if the connection is in usable state.
    virtual auto is_usable() noexcept -> bool {
        return true;
    }

    /// @brief Returns the maximum data block size in bytes that can be sent.
    virtual auto max_data_size() noexcept -> valid_if_positive<span_size_t> {
        return {0};
    }

    /// @brief Sent a message with the specified id.
    /// @see fetch_messages
    /// @see update
    virtual auto send(const message_id msg_id, const message_view&) noexcept
      -> bool = 0;

    /// @brief Fetch all enqueued messages that have been received since last fetch.
    /// @see send
    /// @see update
    virtual auto fetch_messages(const fetch_handler handler) noexcept
      -> work_done = 0;

    /// @brief Fill in the available statistics information for this connection.
    virtual auto query_statistics(connection_statistics&) noexcept -> bool = 0;

    virtual auto routing_weight() noexcept -> float = 0;
};
//------------------------------------------------------------------------------
/// @brief Interface for classes that can use message bus connections.
/// @ingroup msgbus
/// @see connection
/// @see acceptor_user
struct connection_user : interface<connection_user> {

    /// @brief Adds the specified message bus connection.
    /// Result indicates if the connection was used or discarded.
    virtual auto add_connection(shared_holder<connection>) noexcept -> bool = 0;
};
//------------------------------------------------------------------------------
/// @brief Interface for message bus connection acceptors.
/// @ingroup msgbus
/// @see acceptor_user
/// @see connection
export struct acceptor : connection_info {

    /// @brief Alias for accepted connection handler callable reference type.
    using accept_handler =
      callable_ref<void(shared_holder<connection>) noexcept>;

    /// @brief Updates the internal state of the acceptor (called repeatedly).
    virtual auto update() noexcept -> work_done {
        return {};
    }

    /// @brief Lets the handler process the pending accepted connections.
    virtual auto process_accepted(const accept_handler handler) noexcept
      -> work_done = 0;
};
//------------------------------------------------------------------------------
/// @brief Interface for classes that can use message bus connection acceptors.
/// @ingroup msgbus
/// @see acceptor
/// @see connection_user
struct acceptor_user : interface<acceptor_user> {

    /// @brief Adds the specified message bus connection acceptor.
    /// Result indicates if the acceptor was used or discarded.
    virtual auto add_acceptor(shared_holder<acceptor> an_acceptor) noexcept
      -> bool = 0;
};
//------------------------------------------------------------------------------
/// @brief Interface for message bus connection and acceptor factories.
/// @ingroup msgbus
struct connection_factory : connection_info {

    /// @brief Make a new acceptor listening on the specified address.
    /// @see make_connector
    [[nodiscard]] virtual auto make_acceptor(const string_view address)
      -> shared_holder<acceptor> = 0;

    /// @brief Make a new connector connecting to the specified address.
    /// @see make_acceptor
    [[nodiscard]] virtual auto make_connector(const string_view address)
      -> shared_holder<connection> = 0;

    /// @brief Make a new acceptor listening on a default address.
    /// @see make_connector
    [[nodiscard]] auto make_acceptor() {
        return make_acceptor(string_view{});
    }

    /// @brief Make a new connector connecting to the specified address.
    /// @see make_acceptor
    [[nodiscard]] auto make_connector() {
        return make_connector(string_view{});
    }

    /// @brief Make a new acceptor listening on the specified address.
    /// @see make_connector
    [[nodiscard]] auto make_acceptor(const identifier id) {
        return make_acceptor(id.name().view());
    }

    /// @brief Make a new connector connecting to the specified address.
    /// @see make_acceptor
    [[nodiscard]] auto make_connector(const identifier id) {
        return make_connector(id.name().view());
    }
};
//------------------------------------------------------------------------------
/// @brief Interface for message bus services
/// @ingroup msgbus
struct service_interface : interface<service_interface> {

    /// @brief Indicates if the service endpoint has an assigned id.
    virtual auto has_id() const noexcept -> bool = 0;

    /// @brief Returns a view of message queues registered with this service.
    virtual auto process_queues() noexcept
      -> pointee_generator<const subscriber_message_queue*> = 0;

    /// @brief Does an iteration update of the service.
    virtual auto update_only() noexcept -> work_done = 0;

    /// @brief Does an iteration update and processes all received messages.
    virtual auto update_and_process_all() noexcept -> work_done = 0;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
