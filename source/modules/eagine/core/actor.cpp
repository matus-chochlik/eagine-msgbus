/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:actor;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.core.main_ctx;
import :types;
import :message;
import :interface;
import :handler_map;
import :endpoint;
import :subscriber;
import std;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Base class for message bus actors with fixed number of message handlers.
/// @ingroup msgbus
/// @see static_subscriber
/// @see subscriber
/// @see endpoint
export template <
  std::size_t N,
  template <std::size_t> class Subscriber = static_subscriber>
class actor
  : public connection_user
  , public friend_of_endpoint {
    using friend_of_endpoint::_accept_message;
    using friend_of_endpoint::_make_endpoint;
    using friend_of_endpoint::_move_endpoint;

public:
    /// @brief Not move constructible.
    actor(actor&&) = delete;

    /// @brief Not copy constructible.
    actor(const actor&) = delete;

    /// @brief Not moved assignable.
    auto operator=(actor&&) = delete;

    /// @brief Not copy assignable.
    auto operator=(const actor&) = delete;

    /// @brief Returns a reference to the associated endpoint.
    auto bus_node() noexcept -> endpoint& {
        return _endpoint;
    }

    /// @brief Adds a connection to the associated endpoint.
    auto add_connection(std::unique_ptr<connection> conn) noexcept
      -> bool final {
        return _endpoint.add_connection(std::move(conn));
    }

    void allow_subscriptions() noexcept {
        _subscriber.allow_subscriptions();
    }

    /// @brief Processes a single enqueued message for which there is a handler.
    /// @see process_all
    auto process_one() noexcept {
        _endpoint.update();
        return _subscriber.process_one();
    }

    /// @brief Processes all enqueued messages for which there are handlers.
    /// @see process_one
    auto process_all() noexcept {
        _endpoint.update();
        return _subscriber.process_all();
    }

protected:
    auto _process_message(
      const message_id msg_id,
      const message_age,
      const message_view& message) noexcept -> bool {
        // TODO: use message age
        if(not _accept_message(_endpoint, msg_id, message)) {
            if(not is_special_message(msg_id)) {
                _endpoint.block_message_type(msg_id);
            }
        }
        return true;
    }

    /// @brief Constructor usable from derived classes
    template <typename Class, typename... MsgMaps>
    actor(
      main_ctx_object obj, // NOLINT(performance-unnecessary-value-param)
      Class* instance,
      MsgMaps... msg_maps) noexcept
        requires(sizeof...(MsgMaps) == N)
      : _endpoint{_make_endpoint(
          std::move(obj),
          make_callable_ref<&actor::_process_message>(this))}
      , _subscriber{_endpoint, instance, msg_maps...} {
        _subscriber.announce_subscriptions();
    }

    /// @brief Constructor usable from derived classes
    template <typename Derived, typename Class, typename... MsgMaps>
    actor(Derived&& temp, Class* instance, const MsgMaps... msg_maps) noexcept
        requires((sizeof...(MsgMaps) == N) and
                 std::is_base_of_v<actor, Derived>)
      : _endpoint{_move_endpoint(
          std::move(temp._endpoint),
          make_callable_ref<&actor::_process_message>(this))}
      , _subscriber{_endpoint, instance, msg_maps...} {}

    ~actor() noexcept override {
        try {
            _subscriber.retract_subscriptions();
            _endpoint.finish();
        } catch(...) {
        }
    }

private:
    endpoint _endpoint;
    Subscriber<N> _subscriber;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

