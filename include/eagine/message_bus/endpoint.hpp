/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MESSAGE_BUS_ENDPOINT_HPP
#define EAGINE_MESSAGE_BUS_ENDPOINT_HPP

#include "blobs.hpp"
#include "connection.hpp"
#include "context_fwd.hpp"
#include "serialize.hpp"
#include "signal.hpp"
#include <eagine/application_config.hpp>
#include <eagine/flat_map.hpp>
#include <eagine/main_ctx_object.hpp>
#include <eagine/timeout.hpp>
#include <tuple>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
class friend_of_endpoint;
//------------------------------------------------------------------------------
/// @brief Message bus client endpoint that can send and receive messages.
/// @ingroup msgbus
/// @see static_subscriber
/// @see subscriber
class endpoint
  : public connection_user
  , public main_ctx_object {
public:
    static constexpr auto invalid_id() noexcept -> identifier_t {
        return 0U;
    }

    /// @brief Tests if the specified id is a valid endpoint id.
    static constexpr auto is_valid_id(identifier_t id) noexcept -> bool {
        return id != invalid_id();
    }

    /// @brief Alias for message fetch handler callable reference.
    using fetch_handler = connection::fetch_handler;

    /// @brief Triggered when the id is confirmed or assigned to this endpoint.
    signal<void(identifier_t)> id_assigned;

    /// @brief Triggered when this endpoint's connection is established.
    signal<void(bool)> connection_established;

    /// @brief Triggered when this endpoint's connection is lost.
    signal<void()> connection_lost;

    /// @brief Construction with a reference to parent main context object.
    endpoint(main_ctx_object obj) noexcept
      : main_ctx_object{std::move(obj)} {}

    /// @brief Construction with an enpoint id and parent main context object.
    endpoint(identifier id, main_ctx_parent parent) noexcept
      : main_ctx_object{id, parent} {}

    /// @brief Not copy constructible.
    endpoint(const endpoint&) = delete;
    /// @brief Not move assignable.
    auto operator=(endpoint&&) = delete;
    /// @brief Not copy assignable.
    auto operator=(const endpoint&) = delete;

    /// @brief Returns a reference to the message bus context.
    /// @see msgbus::context
    auto ctx() noexcept -> context& {
        EAGINE_ASSERT(_context);
        return *_context;
    }

    ~endpoint() noexcept override = default;

    /// @brief Assigns the unique id of this endpoint.
    /// @see preconfigure_id
    /// @see has_id
    /// @see get_id
    /// @note Do not set manually, use preconfigure_id instead.
    auto set_id(identifier id) -> auto& {
        _endpoint_id = id.value();
        return *this;
    }

    /// @brief Preconfigures the unique id of this endpoint.
    /// @see set_id
    /// @see has_preconfigured_id
    /// @see get_preconfigured_id
    auto preconfigure_id(identifier_t id) -> auto& {
        _preconfd_id = id;
        return *this;
    }

    /// @brief Indicates if this endpoint has a preconfigured id (or should request one).
    /// @see preconfigure_id
    /// @see get_preconfigured_id
    /// @see is_valid_id
    auto has_preconfigured_id() const noexcept -> bool {
        return is_valid_id(_preconfd_id);
    }

    /// @brief Indicates if this endpoint has valid id (set manually or from the bus).
    /// @see set_id
    /// @see get_id
    /// @see is_valid_id
    auto has_id() const noexcept -> bool {
        return is_valid_id(_endpoint_id);
    }

    /// @brief Returns the preconfigured id of this endpoint.
    /// @see preconfigure_id
    /// @see has_preconfigured_id
    /// @see is_valid_id
    auto get_preconfigured_id() const noexcept {
        return _preconfd_id;
    }

    /// @brief Returns the unique id of this endpoint.
    /// @see set_id
    /// @see has_id
    /// @see is_valid_id
    auto get_id() const noexcept {
        return _endpoint_id;
    }

    /// @brief Adds endpoint certificate in a PEM-encoded memory block.
    /// @see add_ca_certificate_pem
    void add_certificate_pem(memory::const_block blk);

    /// @brief Adds CA certificate in a PEM-encoded memory block.
    /// @see add_certificate_pem
    void add_ca_certificate_pem(memory::const_block blk);

    /// @brief Adds a connection for communication with a message bus router.
    auto add_connection(std::unique_ptr<connection> conn) -> bool final;

    /// @brief Tests if this has all prerequisites for sending and receiving messages.
    auto is_usable() const -> bool;

    /// @brief Returns the maximum data block size that the endpoint can send.
    auto max_data_size() const -> valid_if_positive<span_size_t>;

    /// @brief Sends any pending outgoing messages if possible.
    void flush_outbox();

    /// @brief Updates the internal state, sends and receives pending messages.
    auto update() -> work_done;

    /// @brief Says to the message bus that this endpoint is disconnecting.
    void finish() {
        say_bye();
        flush_outbox();
    }

    /// @brief Subscribes to messages with the specified id/type.
    void subscribe(message_id);

    /// @brief Unsubscribes from messages with the specified id/type.
    void unsubscribe(message_id);

    auto set_next_sequence_id(message_id, message_info&) -> bool;

    /// @brief Enqueues a message with the specified id/type for sending.
    /// @see post_signed
    /// @see post_value
    /// @see post_blob
    /// @see post_callable
    auto post(message_id msg_id, const message_view& message) -> bool {
        if(EAGINE_LIKELY(has_id())) {
            return _do_send(msg_id, message);
        }
        _outgoing.push(msg_id, message);
        return true;
    }

    /// @brief Creates a callable that calls post on this enpoint.
    /// @see post
    auto post_callable() noexcept
      -> callable_ref<bool(message_id, const message_view&)> {
        return EAGINE_THIS_MEM_FUNC_REF(post);
    }

    /// @brief Signs and enqueues a message with the specified id/type for sending.
    /// @see post
    /// @see post_value
    auto post_signed(message_id, message_view message) -> bool;

    /// @brief Serializes the specified value and enqueues it for sending in message.
    /// @see post
    /// @see post_signed
    /// @see default_serialize
    template <typename T>
    auto post_value(message_id msg_id, T& value, const message_info& info = {})
      -> bool {
        if(const auto opt_size = max_data_size()) {
            const auto max_size = extract(opt_size);
            return _outgoing.push_if(
              [this, msg_id, &info, &value, max_size](
                message_id& dst_msg_id, stored_message& message) {
                  if(message.store_value(value, max_size)) {
                      message.assign(info);
                      dst_msg_id = msg_id;
                      return true;
                  }
                  return false;
              },
              max_size);
        }
        return false;
    }

    /// @brief Enqueues a BLOB that is larger than max_data_size for sending.
    /// @see post
    /// @see post_signed
    /// @see post_value
    /// @see max_data_size
    auto post_blob(
      message_id msg_id,
      identifier_t target_id,
      blob_id_t target_blob_id,
      memory::const_block blob,
      std::chrono::seconds max_time,
      message_priority priority) -> message_sequence_t {
        return _blobs.push_outgoing(
          msg_id,
          _endpoint_id,
          target_id,
          target_blob_id,
          blob,
          max_time,
          priority);
    }

    /// @brief Enqueues a BLOB that is larger than max_data_size for broadcast.
    /// @see post
    /// @see post_signed
    /// @see post_value
    /// @see max_data_size
    auto broadcast_blob(
      message_id msg_id,
      memory::const_block blob,
      std::chrono::seconds max_time,
      message_priority priority) -> bool {
        return post_blob(
          msg_id, broadcast_endpoint_id(), 0, blob, max_time, priority);
    }

    /// @brief Enqueues a BLOB that is larger than max_data_size for broadcast.
    /// @see post
    /// @see post_signed
    /// @see post_value
    /// @see max_data_size
    auto broadcast_blob(
      message_id msg_id,
      memory::const_block blob,
      std::chrono::seconds max_time) -> bool {
        return broadcast_blob(msg_id, blob, max_time, message_priority::normal);
    }

    /// @brief Posts the certificate of this enpoint to the specified remote.
    auto post_certificate(identifier_t target_id, blob_id_t) -> bool;

    /// @brief Broadcasts the certificate of this enpoint to the whole bus.
    auto broadcast_certificate() -> bool;

    auto broadcast(message_id msg_id) -> bool {
        return post(msg_id, {});
    }

    /// @brief Posts a message saying that this is not a router bus node.
    /// @see post
    auto say_not_a_router() -> bool;

    /// @brief Posts a message saying that this endpoint is alive.
    /// @see post
    /// @see say_bye
    auto say_still_alive() -> bool;

    /// @brief Posts a message saying that this endpoint is about to disconnect.
    /// @see post
    /// @see say_still_alive
    auto say_bye() -> bool;

    /// @brief Post a message with another message type as its content.
    /// @see post
    /// @see post_meta_message_to
    /// @see default_serialize
    void post_meta_message(message_id meta_msg_id, message_id msg_id);

    /// @brief Post a message with another message type as its content to target.
    /// @see post
    /// @see post_meta_message
    /// @see default_serialize
    void post_meta_message_to(
      identifier_t target_id,
      message_id meta_msg_id,
      message_id msg_id);

    /// @brief Broadcasts a message that this subscribes to message with given id.
    /// @see post_meta_message
    /// @see say_unsubscribes_from
    /// @see say_not_subscribed_to
    void say_subscribes_to(message_id);

    /// @brief Posts a message that this subscribes to message with given id.
    /// @see post_meta_message_to
    /// @see say_unsubscribes_from
    /// @see say_not_subscribed_to
    void say_subscribes_to(identifier_t target_id, message_id);

    /// @brief Broadcasts a message that this unsubscribes from message with given type.
    /// @see post_meta_message
    /// @see say_subscribes_to
    /// @see say_not_subscribed_to
    void say_unsubscribes_from(message_id);

    /// @brief Posts a message that this is not subscribed to message with given type.
    /// @see post_meta_message
    /// @see say_subscribes_to
    /// @see say_not_subscribed_to
    void say_not_subscribed_to(identifier_t target_id, message_id);

    /// @brief Posts a message requesting all subscriptions of a target node.
    /// @see query_subscribers_of
    /// @see say_subscribes_to
    void query_subscriptions_of(identifier_t target_id);

    /// @brief Posts a message requesting all subscribers of a given message type.
    /// @see query_subscribers_of
    /// @see say_subscribes_to
    void query_subscribers_of(message_id);

    /// @brief Sends a message to router to clear its block filter for this endpoint.
    /// @see block_message_type
    /// @see clear_allow_list
    void clear_block_list();

    /// @brief Sends a message to router to start blocking message type for this endpoint.
    /// @see clear_block_list
    /// @see allow_message_type
    void block_message_type(message_id);

    /// @brief Sends a message to router to clear its allow filter for this endpoint.
    /// @see allow_message_type
    /// @see clear_block_list
    void clear_allow_list();

    /// @brief Sends a message to router to start blocking message type for this endpoint.
    /// @see clear_allow_list
    /// @see block_message_type
    void allow_message_type(message_id);

    /// @brief Sends a message requesting remote endpoint certificate.
    void query_certificate_of(identifier_t endpoint_id);

    /// @brief Posts a message as a response to another received message.
    /// @see message_info::setup_response
    auto respond_to(
      const message_info& info,
      message_id msg_id,
      message_view message) -> bool {
        message.setup_response(info);
        return post(msg_id, message);
    }

    /// @brief Posts a message as a response to another received message.
    auto respond_to(const message_info& info, message_id msg_id) -> bool {
        return respond_to(info, msg_id, {});
    }

    /// @brief Alias for callable handling received messages.
    /// @see process_one
    /// @see process_all
    using method_handler =
      callable_ref<bool(const message_context&, stored_message&)>;

    /// @brief Processes a single received message of specified type with a handler.
    /// @see process_all
    /// @see process_everything
    auto process_one(message_id msg_id, method_handler handler) -> bool;

    /// @brief Processes a single received message of specified type with a method.
    /// @see process_all
    /// @see process_everything
    template <
      typename Class,
      bool (Class::*MemFnPtr)(const message_context&, stored_message&)>
    auto process_one(
      message_id msg_id,
      member_function_constant<
        bool (Class::*)(const message_context&, stored_message&),
        MemFnPtr> method,
      Class* instance) -> bool {
        return process_one(msg_id, {instance, method});
    }

    /// @brief Processes all received messages of specified type with a handler.
    /// @see process_one
    /// @see process_everything
    auto process_all(message_id msg_id, method_handler handler) -> span_size_t;

    /// @brief Processes all received messages regardles of type with a handler.
    auto process_everything(method_handler handler) -> span_size_t;

    auto ensure_queue(message_id msg_id) noexcept -> message_priority_queue& {
        return _ensure_incoming(msg_id).queue;
    }

    /// @brief Returns the average message age in the connected router.
    auto flow_average_message_age() const noexcept
      -> std::chrono::microseconds {
        return std::chrono::microseconds{_flow_info.avg_msg_age_ms * 1000};
    }

private:
    friend class friend_of_endpoint;

    shared_context _context{make_context(*this)};

    identifier_t _preconfd_id{invalid_id()};
    identifier_t _endpoint_id{invalid_id()};
    const process_instance_id_t _instance_id{process_instance_id()};

    std::chrono::steady_clock::time_point _startup_time{
      std::chrono::steady_clock::now()};

    endpoint_statistics _stats{};
    message_flow_info _flow_info{};

    auto _uptime_seconds() -> std::int64_t;

    timeout _no_id_timeout{
      cfg_init(
        "msg_bus.endpoint.no_id_timeout",
        adjusted_duration(std::chrono::seconds{3})),
      nothing};

    resetting_timeout _should_notify_alive{
      cfg_init("msg_bus.endpoint.alive_notify_period", std::chrono::seconds{30}),
      nothing};

    std::unique_ptr<connection> _connection{};
    bool _had_working_connection{false};

    message_storage _outgoing{};

    struct incoming_state {
        span_size_t subscription_count{0};
        message_priority_queue queue{};
    };

    flat_map<message_id, std::unique_ptr<incoming_state>> _incoming{};

    auto _ensure_incoming(message_id msg_id) -> incoming_state& {
        auto pos = _incoming.find(msg_id);
        if(pos == _incoming.end()) {
            pos = _incoming.emplace(msg_id, std::make_unique<incoming_state>())
                    .first;
        }
        EAGINE_ASSERT(pos->second);
        return *pos->second;
    }

    auto _find_incoming(message_id msg_id) const noexcept -> incoming_state* {
        const auto pos = _incoming.find(msg_id);
        return (pos != _incoming.end()) ? pos->second.get() : nullptr;
    }

    auto _get_incoming(message_id msg_id) const noexcept -> incoming_state& {
        const auto pos = _incoming.find(msg_id);
        EAGINE_ASSERT(pos != _incoming.end());
        EAGINE_ASSERT(pos->second);
        return *pos->second;
    }

    blob_manipulator _blobs{
      *this,
      EAGINE_MSGBUS_ID(blobFrgmnt),
      EAGINE_MSGBUS_ID(blobResend)};

    auto _process_blobs() -> work_done;

    auto _default_store_handler() noexcept -> fetch_handler {
        return EAGINE_THIS_MEM_FUNC_REF(_store_message);
    }

    fetch_handler _store_handler{_default_store_handler()};

    auto _do_send(message_id msg_id, message_view) -> bool;

    auto
    _handle_send(message_id msg_id, message_age, const message_view& message)
      -> bool {
        // TODO: use message age
        return _do_send(msg_id, message);
    }

    auto _handle_special(message_id msg_id, const message_view&) noexcept
      -> bool;

    auto _store_message(message_id msg_id, message_age, const message_view&)
      -> bool;

    auto _accept_message(message_id msg_id, const message_view&) -> bool;

    explicit endpoint(main_ctx_object obj, fetch_handler store_message) noexcept
      : main_ctx_object{std::move(obj)}
      , _store_handler{std::move(store_message)} {}

    endpoint(endpoint&& temp) noexcept
      : main_ctx_object{static_cast<main_ctx_object&&>(temp)}
      , _context{std::move(temp._context)}
      , _preconfd_id{std::exchange(temp._preconfd_id, invalid_id())}
      , _endpoint_id{std::exchange(temp._endpoint_id, invalid_id())}
      , _connection{std::move(temp._connection)}
      , _outgoing{std::move(temp._outgoing)}
      , _incoming{std::move(temp._incoming)}
      , _blobs{std::move(temp._blobs)} {}

    endpoint(endpoint&& temp, fetch_handler store_message) noexcept
      : main_ctx_object{static_cast<main_ctx_object&&>(temp)}
      , _context{std::move(temp._context)}
      , _preconfd_id{std::exchange(temp._preconfd_id, invalid_id())}
      , _endpoint_id{std::exchange(temp._endpoint_id, invalid_id())}
      , _connection{std::move(temp._connection)}
      , _outgoing{std::move(temp._outgoing)}
      , _incoming{std::move(temp._incoming)}
      , _blobs{std::move(temp._blobs)}
      , _store_handler{std::move(store_message)} {}
};
//------------------------------------------------------------------------------
/// @brief Base for classes that need access to enpoint internal functionality
/// @ingroup msgbus
class friend_of_endpoint {
protected:
    static auto _make_endpoint(
      main_ctx_object obj,
      endpoint::fetch_handler store_message) noexcept {
        return endpoint{std::move(obj), store_message};
    }

    static auto _move_endpoint(
      endpoint&& bus,
      endpoint::fetch_handler store_message) noexcept {
        return endpoint{std::move(bus), store_message};
    }

    inline auto _accept_message(
      endpoint& ep,
      message_id msg_id,
      const message_view& message) -> bool {
        return ep._accept_message(msg_id, message);
    }
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#if !EAGINE_MSGBUS_LIBRARY || defined(EAGINE_IMPLEMENTING_LIBRARY)
#include <eagine/message_bus/endpoint.inl>
#endif

#endif // EAGINE_MESSAGE_BUS_ENDPOINT_HPP
