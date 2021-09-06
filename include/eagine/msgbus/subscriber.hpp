/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_SUBSCRIBER_HPP
#define EAGINE_MSGBUS_SUBSCRIBER_HPP

#include "endpoint.hpp"
#include "handler_map.hpp"
#include "verification.hpp"
#include <eagine/interface.hpp>
#include <eagine/maybe_unused.hpp>
#include <eagine/span.hpp>
#include <array>
#include <type_traits>
#include <vector>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Base class for message bus subscribers.
/// @ingroup msgbus
/// @see static_subscriber
/// @see subscriber
/// @see endpoint
class subscriber_base {
public:
    operator main_ctx_object_parent_info() noexcept {
        return {_endpoint};
    }

    /// @brief Returns a reference to the associated endpoint.
    auto bus_node() noexcept -> auto& {
        return _endpoint;
    }

    /// @brief Returns a const reference to the associated endpoint.
    auto bus_node() const noexcept -> auto& {
        return _endpoint;
    }

    /// @brief Returns a reference to the associated application config object.
    auto app_config() noexcept -> application_config& {
        return _endpoint.app_config();
    }

    /// @brief Updates the internal endpoint state (should be called repeatedly).
    auto update() const noexcept -> work_done {
        return _endpoint.update();
    }

    /// @brief Uses the associated endpoint to verify the specified message.
    auto verify_bits(const stored_message& message) noexcept
      -> verification_bits {
        return message.verify_bits(_endpoint.ctx(), _endpoint);
    }

    /// @brief Queries the subscriptions of the remote endpoint with the specified id.
    /// @see query_subscribers_of
    void query_subscriptions_of(const identifier_t target_id) {
        _endpoint.query_subscriptions_of(target_id);
    }

    /// @brief Queries remote nodes subscribing to the specified message.
    /// @see query_subscriptions_of
    void query_subscribers_of(const message_id sub_msg) {
        _endpoint.query_subscribers_of(sub_msg);
    }

    /// @brief Not copy assignable.
    subscriber_base(const subscriber_base&) = delete;

    /// @brief Not move assignable.
    auto operator=(subscriber_base&&) = delete;

    /// @brief Not copy constructible.
    auto operator=(const subscriber_base&) = delete;

protected:
    using method_handler = basic_callable_ref<
      bool(const message_context&, const stored_message&) noexcept,
      true>;

    struct handler_entry {
        message_id msg_id{};
        method_handler handler{};
        message_priority_queue* queue{nullptr};

        constexpr handler_entry() noexcept = default;

        constexpr handler_entry(message_id id, method_handler hndlr) noexcept
          : msg_id{std::move(id)}
          , handler{std::move(hndlr)} {}

        template <
          const identifier_t ClassId,
          const identifier_t MethodId,
          typename Class,
          bool (Class::*HandlerFunc)(
            const message_context&,
            const stored_message&) noexcept>
        handler_entry(
          Class* instance,
          static_message_handler_map<
            static_message_id<ClassId, MethodId>,
            member_function_constant<
              bool (
                Class::*)(const message_context&, const stored_message&) noexcept,
              HandlerFunc>> msg_map) noexcept
          : msg_id{ClassId, MethodId}
          , handler{instance, msg_map.method()} {}

        template <
          const identifier_t ClassId,
          const identifier_t MethodId,
          typename Class,
          bool (
            Class::*HandlerFunc)(const message_context&, const stored_message&)
            const noexcept>
        handler_entry(
          const Class* instance,
          static_message_handler_map<
            static_message_id<ClassId, MethodId>,
            member_function_constant<
              bool (Class::*)(const message_context&, const stored_message&)
                const noexcept,
              HandlerFunc>> msg_map) noexcept
          : msg_id{ClassId, MethodId}
          , handler{instance, msg_map.method()} {}
    };

    ~subscriber_base() noexcept = default;

    constexpr subscriber_base(endpoint& bus) noexcept
      : _endpoint{bus} {}

    subscriber_base(subscriber_base&& temp) noexcept
      : _endpoint{temp._endpoint} {}

    void _subscribe_to(const span<const handler_entry> msg_handlers) const {
        for(auto& entry : msg_handlers) {
            _endpoint.subscribe(entry.msg_id);
        }
    }

    void _unsubscribe_from(
      const span<const handler_entry> msg_handlers) const noexcept {
        for(auto& entry : msg_handlers) {
            try {
                _endpoint.unsubscribe(entry.msg_id);
            } catch(...) {
            }
        }
    }

    void _announce_subscriptions(
      const span<const handler_entry> msg_handlers) const {
        for(auto& entry : msg_handlers) {
            _endpoint.say_subscribes_to(entry.msg_id);
        }
    }

    void _allow_subscriptions(
      const span<const handler_entry> msg_handlers) const {
        for(auto& entry : msg_handlers) {
            _endpoint.allow_message_type(entry.msg_id);
        }
    }

    void _retract_subscriptions(
      const span<const handler_entry> msg_handlers) const noexcept {
        for(auto& entry : msg_handlers) {
            try {
                _endpoint.say_unsubscribes_from(entry.msg_id);
            } catch(...) {
            }
        }
    }

    void _respond_to_subscription_query(
      const identifier_t source_id,
      const span<const handler_entry> msg_handlers) const {
        for(auto& entry : msg_handlers) {
            _endpoint.say_subscribes_to(source_id, entry.msg_id);
        }
    }

    void _respond_to_subscription_query(
      const identifier_t source_id,
      const message_id sub_msg,
      const span<const handler_entry> msg_handlers) const {
        for(auto& entry : msg_handlers) {
            if(entry.msg_id == sub_msg) {
                _endpoint.say_subscribes_to(source_id, sub_msg);
                return;
            }
        }
        _endpoint.say_not_subscribed_to(source_id, sub_msg);
    }

    auto _process_one(const span<const handler_entry> msg_handlers) -> bool {
        for(auto& entry : msg_handlers) {
            EAGINE_ASSERT(entry.queue);
            const message_context msg_ctx{this->bus_node(), entry.msg_id};
            if(extract(entry.queue).process_all(msg_ctx, entry.handler)) {
                return true;
            }
        }
        return false;
    }

    auto _process_all(const span<const handler_entry> msg_handlers)
      -> span_size_t {
        span_size_t result{0};
        for(auto& entry : msg_handlers) {
            EAGINE_ASSERT(entry.queue);
            const message_context msg_ctx{this->bus_node(), entry.msg_id};
            result += extract(entry.queue).process_all(msg_ctx, entry.handler);
        }
        return result;
    }

    void _setup_queues(span<handler_entry> msg_handlers) noexcept {
        for(auto& entry : msg_handlers) {
            entry.queue = &this->bus_node().ensure_queue(entry.msg_id);
        }
    }

    void _finish() noexcept {
        try {
            _endpoint.finish();
        } catch(...) {
        }
    }

private:
    endpoint& _endpoint;
};
//------------------------------------------------------------------------------
/// @brief Template for subscribers with predefined count of handled message types.
/// @ingroup msgbus
/// @see subscriber
template <std::size_t N>
class static_subscriber : public subscriber_base {
public:
    /// @brief Alias for method/message handler callable reference.
    using method_handler = basic_callable_ref<
      bool(const message_context&, const stored_message&) noexcept,
      true>;

    using handler_entry = typename subscriber_base::handler_entry;

    template <
      typename... MsgHandlers,
      typename = std::enable_if_t<sizeof...(MsgHandlers) == N>>
    static_subscriber(endpoint& bus, MsgHandlers&&... msg_handlers)
      : subscriber_base{bus}
      , _msg_handlers{{std::forward<MsgHandlers>(msg_handlers)...}} {
        this->_setup_queues(cover(_msg_handlers));
        this->_subscribe_to(view(_msg_handlers));
    }

    /// @brief Construction from a reference to endpoint and some message maps.
    /// @see endpoint
    /// @see message_handler_map
    /// @see EAGINE_MSG_MAP
    template <
      typename Class,
      typename... MsgMaps,
      typename = std::enable_if_t<sizeof...(MsgMaps) == N>>
    static_subscriber(endpoint& bus, Class* instance, const MsgMaps... msg_maps)
      : static_subscriber(bus, handler_entry(instance, msg_maps)...) {}

    /// @brief Not move constructible.
    static_subscriber(static_subscriber&& temp) = delete;

    /// @brief Not copy constructible.
    static_subscriber(const static_subscriber&) = delete;

    /// @brief Not move assignable.
    auto operator=(static_subscriber&&) = delete;

    /// @brief Not copy assignable.
    auto operator=(const static_subscriber&) = delete;

    ~static_subscriber() noexcept {
        this->_unsubscribe_from(view(_msg_handlers));
    }

    /// @brief Processes one pending enqueued message.
    auto process_one() -> bool {
        return this->_process_one(view(_msg_handlers));
    }

    /// @brief Processes all pending enqueued messages.
    auto process_all() -> span_size_t {
        return this->_process_all(view(_msg_handlers));
    }

    /// @brief Sends messages to the bus saying which messages this can handle.
    /// @see retract_subscriptions
    /// @see respond_to_subscription_query
    /// @see allow_subscriptions
    void announce_subscriptions() const {
        this->_announce_subscriptions(view(_msg_handlers));
    }

    /// @brief Sends messages to the router saying which messages should be forwarded.
    /// @see announce_subscriptions
    /// @see retract_subscriptions
    /// @see respond_to_subscription_query
    void allow_subscriptions() const {
        this->_allow_subscriptions(view(_msg_handlers));
    }

    /// @brief Sends messages to the bus saying which messages this cannot handle.
    /// @see announce_subscriptions
    /// @see respond_to_subscription_query
    /// @see allow_subscriptions
    void retract_subscriptions() const noexcept {
        this->_retract_subscriptions(view(_msg_handlers));
    }

    /// @brief Sends messages responding to a subscription query.
    /// @see retract_subscriptions
    /// @see announce_subscriptions
    void respond_to_subscription_query(
      const identifier_t source_id) const noexcept {
        this->_respond_to_subscription_query(source_id, view(_msg_handlers));
    }

    /// @brief Sends messages responding to a subscription query.
    /// @see retract_subscriptions
    /// @see announce_subscriptions
    void respond_to_subscription_query(
      const identifier_t source_id,
      const message_id sub_msg) const noexcept {
        this->_respond_to_subscription_query(
          source_id, sub_msg, view(_msg_handlers));
    }

private:
    std::array<handler_entry, N> _msg_handlers;
};
//------------------------------------------------------------------------------
/// @brief Template for subscribers with variable count of handled message types.
/// @ingroup msgbus
/// @see static_subscriber
class subscriber
  : public interface<subscriber>
  , public subscriber_base {
public:
    using handler_entry = subscriber_base::handler_entry;

    /// @brief Alias for method/message handler callable reference.
    using method_handler = basic_callable_ref<
      bool(const message_context&, const stored_message&) noexcept,
      true>;

    /// @brief Construction from a reference to endpoint.
    subscriber(endpoint& bus) noexcept
      : subscriber_base{bus} {}

    /// @brief Adds a handler for messages with the specified message id.
    template <
      typename Class,
      bool (
        Class::*Method)(const message_context&, const stored_message&) noexcept>
    void add_method(
      Class* instance,
      const message_id msg_id,
      const member_function_constant<
        bool (Class::*)(const message_context&, const stored_message&) noexcept,
        Method> method) noexcept {
        _msg_handlers.emplace_back(msg_id, method_handler{instance, method});
    }

    /// @brief Adds a handler for messages with the specified message id.
    template <
      typename Class,
      bool (
        Class::*Method)(const message_context&, const stored_message&) noexcept>
    void add_method(
      Class* instance,
      message_handler_map<member_function_constant<
        bool (Class::*)(const message_context&, const stored_message&) noexcept,
        Method>> msg_map) noexcept {
        add_method(instance, msg_map.msg_id(), msg_map.method());
    }

    /// @brief Adds a handler for messages with the specified message id.
    template <
      typename Class,
      bool (
        Class::*Method)(const message_context&, const stored_message&) noexcept>
    void add_method(
      std::tuple<
        Class*,
        message_handler_map<member_function_constant<
          bool (Class::*)(const message_context&, const stored_message&) noexcept,
          Method>>> imm) noexcept {
        add_method(
          std::get<0>(imm),
          std::get<1>(imm).msg_id(),
          std::get<1>(imm).method());
    }

    /// @brief Adds a handler for messages with the specified message id.
    template <
      typename Class,
      bool (
        Class::*Method)(const message_context&, const stored_message&) noexcept,
      identifier_t ClassId,
      identifier_t MethodId>
    void add_method(
      Class* instance,
      const static_message_handler_map<
        static_message_id<ClassId, MethodId>,
        member_function_constant<
          bool (Class::*)(const message_context&, const stored_message&) noexcept,
          Method>> msg_map) noexcept {
        add_method(instance, msg_map.msg_id(), msg_map.method());
    }

    /// @brief Handles (and removes) one of pending received messages.
    /// @see process_all
    auto process_one() -> bool {
        return this->_process_one(view(_msg_handlers));
    }

    /// @brief Handles (and removes) all pending received messages.
    /// @see process_one
    auto process_all() -> span_size_t {
        return this->_process_all(view(_msg_handlers));
    }

    /// @brief Sends messages to the bus saying which messages this can handle.
    /// @see retract_subscriptions
    /// @see respond_to_subscription_query
    /// @see allow_subscriptions
    void announce_subscriptions() const {
        this->_announce_subscriptions(view(_msg_handlers));
    }

    /// @brief Sends messages to the router saying which messages should be forwarded.
    /// @see announce_subscriptions
    /// @see retract_subscriptions
    /// @see respond_to_subscription_query
    void allow_subscriptions() const {
        this->_allow_subscriptions(view(_msg_handlers));
    }

    /// @brief Sends messages to the bus saying which messages this cannot handle.
    /// @see announce_subscriptions
    /// @see respond_to_subscription_query
    /// @see allow_subscriptions
    void retract_subscriptions() const noexcept {
        this->_retract_subscriptions(view(_msg_handlers));
    }

    /// @brief Sends messages responding to a subscription query.
    /// @see retract_subscriptions
    /// @see announce_subscriptions
    void respond_to_subscription_query(
      const identifier_t source_id) const noexcept {
        this->_respond_to_subscription_query(source_id, view(_msg_handlers));
    }

    /// @brief Sends messages responding to a subscription query.
    /// @see retract_subscriptions
    /// @see announce_subscriptions
    void respond_to_subscription_query(
      const identifier_t source_id,
      const message_id sub_msg) const noexcept {
        this->_respond_to_subscription_query(
          source_id, sub_msg, view(_msg_handlers));
    }

protected:
    constexpr void add_methods() noexcept {}

    void init() {
        this->_setup_queues(cover(_msg_handlers));
        this->_subscribe_to(view(_msg_handlers));
    }

    void finish() noexcept {
        this->_unsubscribe_from(view(_msg_handlers));
        this->_finish();
    }

private:
    std::vector<handler_entry> _msg_handlers;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#endif // EAGINE_MSGBUS_SUBSCRIBER_HPP
