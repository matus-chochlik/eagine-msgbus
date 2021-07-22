/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_SERVICE_STREAM_HPP
#define EAGINE_MSGBUS_SERVICE_STREAM_HPP

#include "../serialize.hpp"
#include "../service_requirements.hpp"
#include "../signal.hpp"
#include "../subscriber.hpp"
#include "discovery.hpp"
#include "ping_pong.hpp"
#include <eagine/reflect/map_data_members.hpp>
#include <eagine/timeout.hpp>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Structure holding information about a data stream.
/// @ingroup msgbus
struct stream_info {
    /// @brief The stream identifier unique in the scope of the provider.
    identifier_t id{0};

    /// @brief The stream kind identifier.
    identifier kind{};

    /// @brief The stream encoding identifier.
    identifier encoding{};
};

template <typename Selector>
constexpr auto
data_member_mapping(type_identity<stream_info>, Selector) noexcept {
    using S = stream_info;
    return make_data_member_mapping<S, identifier_t, identifier, identifier>(
      {"id", &S::id}, {"kind", &S::kind}, {"encoding", &S::encoding});
}
//------------------------------------------------------------------------------
/// @brief Service providing encoded stream data.
/// @ingroup msgbus
/// @see service_composition
/// @see stream_consumer
/// @see stream_relay
template <typename Base = subscriber>
class stream_provider : public require_services<Base, subscriber_discovery> {
    using This = stream_provider;
    using base = require_services<Base, subscriber_discovery>;

public:
    /// @brief Indicates if this provider has associated a relay node.
    auto has_relay() const noexcept -> bool {
        return is_valid_endpoint_id(_relay_id);
    }

protected:
    using base::base;

    void init() {
        base::init();

        this->reported_alive.connect(EAGINE_THIS_MEM_FUNC_REF(_handle_alive));
        this->subscribed.connect(EAGINE_THIS_MEM_FUNC_REF(_handle_subscribed));
        this->unsubscribed.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_unsubscribed));
        this->not_subscribed.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_unsubscribed));
    }

    void add_methods() {
        base::add_methods();
    }

private:
    void _handle_alive(const subscriber_info& sub_info) {
        if(sub_info.endpoint_id == _relay_id) {
            _relay_timeout.reset();
        }
    }

    void _handle_subscribed(const subscriber_info& sub_info, message_id msg_id) {
        if(msg_id == EAGINE_MSG_ID(eagiStream, anceStream)) {
            if(!has_relay() || (_hop_count > sub_info.hop_count)) {
                _relay_id = invalid_endpoint_id();
                _relay_timeout.reset();
                _hop_count = sub_info.hop_count;
            }
        }
    }

    void
    _handle_unsubscribed(const subscriber_info& sub_info, message_id msg_id) {
        if(msg_id == EAGINE_MSG_ID(eagiStream, reqestData)) {
            if(_relay_id == sub_info.endpoint_id) {
                _relay_id = invalid_endpoint_id();
                _hop_count = subscriber_info::max_hops();
            }
        }
    }

    identifier_t _relay_id{invalid_endpoint_id()};
    timeout _relay_timeout{std::chrono::seconds{5}};
    subscriber_info::hop_count_t _hop_count{subscriber_info::max_hops()};
};
//------------------------------------------------------------------------------
/// @brief Service consuming encoded stream data.
/// @ingroup msgbus
/// @see service_composition
/// @see stream_provider
/// @see stream_relay
template <typename Base = subscriber>
class stream_consumer : public require_services<Base, subscriber_discovery> {
    using This = stream_consumer;
    using base = require_services<Base, subscriber_discovery>;

public:
    /// @brief Indicates if this consumer has associated a relay node.
    auto has_relay() const noexcept -> bool {
        return is_valid_endpoint_id(_relay_id);
    }

    /// @brief Triggered when a data stream has appeared at the given provider.
    signal<void(
      identifier_t provider_id,
      const stream_info&,
      verification_bits verified)>
      stream_appeared;

    /// @brief Triggered when a data stream has been lost at the given provider.
    signal<void(
      identifier_t provider_id,
      const stream_info&,
      verification_bits verified)>
      stream_disappeared;

protected:
    using base::base;

    void init() {
        base::init();

        this->reported_alive.connect(EAGINE_THIS_MEM_FUNC_REF(_handle_alive));
        this->subscribed.connect(EAGINE_THIS_MEM_FUNC_REF(_handle_subscribed));
        this->unsubscribed.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_unsubscribed));
        this->not_subscribed.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_unsubscribed));
    }

    void add_methods() {
        base::add_methods();
        base::add_method(
          this, EAGINE_MSG_MAP(eagiStream, appeared, This, _handle_appeared));
        base::add_method(
          this,
          EAGINE_MSG_MAP(eagiStream, appeared, This, _handle_disappeared));
    }

private:
    void _handle_alive(const subscriber_info& sub_info) {
        if(_relay_id == sub_info.endpoint_id) {
            _relay_timeout.reset();
        }
    }

    void _handle_subscribed(const subscriber_info& sub_info, message_id msg_id) {
        if(msg_id == EAGINE_MSG_ID(eagiStream, reqestData)) {
            if(!has_relay() || (_hop_count > sub_info.hop_count)) {
                _relay_id = invalid_endpoint_id();
                _relay_timeout.reset();
                _hop_count = sub_info.hop_count;
            }
        }
    }

    void
    _handle_unsubscribed(const subscriber_info& sub_info, message_id msg_id) {
        if(msg_id == EAGINE_MSG_ID(eagiStream, reqestData)) {
            if(_relay_id == sub_info.endpoint_id) {
                _relay_id = invalid_endpoint_id();
                _hop_count = subscriber_info::max_hops();
            }
        }
    }

    auto _handle_appeared(const message_context&, stored_message& message)
      -> bool {
        stream_info info{};
        if(default_deserialize(info, message.content())) {
            stream_appeared(
              message.source_id, info, this->verify_bits(message));
        }
        return true;
    }

    auto _handle_disappeared(const message_context&, stored_message& message)
      -> bool {
        stream_info info{};
        if(default_deserialize(info, message.content())) {
            stream_disappeared(
              message.source_id, info, this->verify_bits(message));
        }
        return true;
    }

    identifier_t _relay_id{invalid_endpoint_id()};
    timeout _relay_timeout{std::chrono::seconds{5}};
    subscriber_info::hop_count_t _hop_count{subscriber_info::max_hops()};
};
//------------------------------------------------------------------------------
/// @brief Service relaying stream data between providers and consumers.
/// @ingroup msgbus
/// @see service_composition
/// @see stream_provider
/// @see stream_consumer
template <typename Base = subscriber>
class stream_relay : public require_services<Base, pingable> {
    using This = stream_relay;
    using base = require_services<Base, pingable>;

public:
protected:
    using base::base;

    void add_methods() {
        base::add_methods();
        base::add_method(
          this, EAGINE_MSG_MAP(eagiStream, serchRelay, This, _handle_search));
    }

private:
    auto _handle_search(const message_context&, stored_message&) -> bool {
        return true;
    }
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#endif // EAGINE_MSGBUS_SERVICE_STREAM_HPP
