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
#include <map>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Structure holding information about a data stream.
/// @ingroup msgbus
struct stream_info {
    /// @brief The stream identifier unique in the scope of the provider.
    identifier_t id{invalid_endpoint_id()};

    /// @brief The stream kind identifier.
    identifier kind{};

    /// @brief The stream encoding identifier.
    identifier encoding{};

    /// @brief Human-readable description of the stream,
    std::string description{};
};

template <typename Selector>
constexpr auto
data_member_mapping(type_identity<stream_info>, Selector) noexcept {
    using S = stream_info;
    return make_data_member_mapping<
      S,
      identifier_t,
      identifier,
      identifier,
      std::string>(
      {"id", &S::id},
      {"kind", &S::kind},
      {"encoding", &S::encoding},
      {"description", &S::description});
}
//------------------------------------------------------------------------------
/// @brief Base class for stream provider and consumer services.
/// @ingroup msgbus
/// @see service_composition
/// @see stream_provider
/// @see stream_consumer
template <typename Base = subscriber>
class stream_endpoint : public require_services<Base, subscriber_discovery> {
    using base = require_services<Base, subscriber_discovery>;

public:
    /// @brief Indicates if this stream client has associated a relay node.
    auto has_stream_relay() const noexcept -> bool {
        return is_valid_endpoint_id(_stream_relay_id);
    }

    /// @brief Returns the id of the assigned stream relay node.
    auto stream_relay() const noexcept -> identifier_t {
        return _stream_relay_id;
    }

    ///@brief Resets the assigned relay node.
    void reset_stream_relay() noexcept {
        _stream_relay_id = invalid_endpoint_id();
        _stream_relay_hops = subscriber_info::max_hops();
        stream_relay_reset();
    }

    ///@brief Explicitly sets the id of the relay node.
    void set_stream_relay(
      identifier_t endpoint_id,
      subscriber_info::hop_count_t hop_count =
        subscriber_info::max_hops()) noexcept {
        if(EAGINE_LIKELY(is_valid_endpoint_id(endpoint_id))) {
            _stream_relay_id = endpoint_id;
            _stream_relay_timeout.reset();
            _stream_relay_hops = hop_count;
            stream_relay_assigned(_stream_relay_id);
        } else {
            reset_stream_relay();
        }
    }

    /// @brief Triggered when a new relay has been assigned.
    signal<void(identifier_t relay_id)> stream_relay_assigned;

    /// @brief Triggered when the current relay has been reset.
    signal<void()> stream_relay_reset;

protected:
    using base::base;

    void init() {
        base::init();

        this->reported_alive.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_stream_relay_alive));
        this->subscribed.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_stream_relay_subscribed));
        this->unsubscribed.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_stream_relay_unsubscribed));
        this->not_subscribed.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_stream_relay_unsubscribed));
    }

    auto update() -> work_done {
        some_true something_done{base::update()};

        if(_stream_relay_timeout) {
            if(has_stream_relay()) {
                reset_stream_relay();
            } else {
                this->bus_node().query_subscribers_of(
                  EAGINE_MSG_ID(eagiStream, reqestData));
                _stream_relay_timeout.reset();
            }
            something_done();
        }

        return something_done;
    }

private:
    void _handle_stream_relay_alive(const subscriber_info& sub_info) {
        if(sub_info.endpoint_id == _stream_relay_id) {
            _stream_relay_timeout.reset();
        }
    }

    void _handle_stream_relay_subscribed(
      const subscriber_info& sub_info,
      message_id msg_id) {
        if(msg_id == EAGINE_MSG_ID(eagiStream, reqestData)) {
            if(!has_stream_relay() || (_stream_relay_hops > sub_info.hop_count)) {
                set_stream_relay(sub_info.endpoint_id, sub_info.hop_count);
            }
        }
    }

    void _handle_stream_relay_unsubscribed(
      const subscriber_info& sub_info,
      message_id msg_id) {
        if(msg_id == EAGINE_MSG_ID(eagiStream, reqestData)) {
            if(_stream_relay_id == sub_info.endpoint_id) {
                reset_stream_relay();
            }
        }
    }

    identifier_t _stream_relay_id{invalid_endpoint_id()};
    timeout _stream_relay_timeout{std::chrono::seconds{5}};
    subscriber_info::hop_count_t _stream_relay_hops{
      subscriber_info::max_hops()};
};
//------------------------------------------------------------------------------
/// @brief Service providing encoded stream data.
/// @ingroup msgbus
/// @see service_composition
/// @see stream_consumer
/// @see stream_relay
template <typename Base = subscriber>
class stream_provider : public require_services<Base, stream_endpoint> {
    using This = stream_provider;
    using base = require_services<Base, stream_endpoint>;

public:
    /// @brief Adds the information about a new stream.
    /// @see remove_stream
    auto add_stream(stream_info info) -> identifier_t {
        if(info.id == 0) {
            if(_stream_id_seq == 0) {
                ++_stream_id_seq;
            }
            while(_streams.find(_stream_id_seq) != _streams.end()) {
                if(_stream_id_seq == 0) {
                    return 0;
                }
                ++_stream_id_seq;
            }
            info.id = _stream_id_seq;
        }
        const auto& stream = _streams[info.id];
        stream.info = std::move(info);
        if(this->has_stream_relay()) {
            _announce_stream(this->stream_relay(), stream.info);
        }
        return stream.info.id;
    }

    /// @brief Removes the information about the specified stream.
    /// @see add_stream
    auto remove_stream(identifier_t stream_id) -> bool {
        if(this->has_stream_relay()) {
            _retract_stream(this->stream_relay());
        }
        return _streams.erase(stream_id) > 0;
    }

protected:
    using base::base;

    void init() {
        base::init();

        this->stream_relay_assigned.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_stream_relay_assigned));
        this->stream_relay_reset.connect(
          EAGINE_THIS_MEM_FUNC_REF(_handle_stream_relay_reset));
    }

    void add_methods() {
        base::add_methods();

        base::add_method(
          this,
          EAGINE_MSG_MAP(eagiStream, reqestData, This, _handle_request_data));

        base::add_method(
          this, EAGINE_MSG_MAP(eagiStream, stopData, This, _handle_stop_data));
    }

private:
    void _announce_stream(identifier_t relay_id, const stream_info& info) {
        auto buffer = default_serialize_buffer_for(info);

        if(auto serialized{default_serialize(info, cover(buffer))}) {
            const auto msg_id{EAGINE_MSG_ID(eagiStream, announce)};
            message_view message{extract(serialized)};
            message.set_target_id(relay_id);
            this->bus_node().set_next_sequence_id(msg_id, message);
            this->bus_node().post(msg_id, message);
        }
    }

    void _retract_stream(identifier_t relay_id, identifier_t stream_id) {
        auto buffer = default_serialize_buffer_for(stream_id);
        auto serialized{default_serialize(stream_id, cover(buffer))};
        EAGINE_ASSERT(serialized);
        const auto msg_id{EAGINE_MSG_ID(eagiStream, retract)};
        message_view message{extract(serialized)};
        message.set_target_id(relay_id);
        this->bus_node().set_next_sequence_id(msg_id, message);
        this->bus_node().post(msg_id, message);
    }

    void _handle_stream_relay_assigned(identifier_t relay_id) {
        for(const auto& [stream_id, stream] : _streams) {
            EAGINE_ASSERT(stream_id == stream.info.id);
            EAGINE_MAYBE_UNUSED(stream_id);
            _announce_stream(relay_id, stream.info);
        }
    }

    void _handle_stream_relay_reset() {
        for(auto& [stream_id, stream] : _streams) {
            EAGINE_ASSERT(stream_id == stream.info.id);
            EAGINE_MAYBE_UNUSED(stream_id);
            stream.send_data = false;
        }
    }

    auto _handle_request_data(const message_context&, stored_message& message)
      -> bool {
        identifier_t stream_id{0};
        if(default_deserialize(stream_id, message.content())) {
            const auto pos = _streams.find(stream_id);
            if(pos != _streams.end()) {
                auto& stream = std::get<1>(*pos);
                stream.sequence = 0U;
                stream.send_data = true;
            }
        }
        return true;
    }

    auto _handle_stop_data(const message_context&, stored_message& message)
      -> bool {
        identifier_t stream_id{0};
        if(default_deserialize(stream_id, message.content())) {
            const auto pos = _streams.find(stream_id);
            if(pos != _streams.end()) {
                auto& stream = std::get<1>(*pos);
                stream.send_data = false;
            }
        }
        return true;
    }

    identifier_t _stream_id_seq{0};
    struct stream_status {
        stream_info info{};
        std::uint64_t sequence{0U};
        bool send_data{false};
    };
    std::map<identifier_t, stream_status> _streams;
};
//------------------------------------------------------------------------------
/// @brief Service consuming encoded stream data.
/// @ingroup msgbus
/// @see service_composition
/// @see stream_provider
/// @see stream_relay
template <typename Base = subscriber>
class stream_consumer : public require_services<Base, stream_endpoint> {
    using This = stream_consumer;
    using base = require_services<Base, stream_endpoint>;

public:
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

    void add_methods() {
        base::add_methods();
        base::add_method(
          this, EAGINE_MSG_MAP(eagiStream, appeared, This, _handle_appeared));
        base::add_method(
          this,
          EAGINE_MSG_MAP(eagiStream, disapeared, This, _handle_disappeared));
    }

    auto update() -> work_done {
        some_true something_done{};
        something_done(base::update());
        return something_done;
    }

private:
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

    auto update() -> work_done {
        some_true something_done{};
        something_done(base::update());
        return something_done;
    }

private:
    auto _handle_search(const message_context&, stored_message&) -> bool {
        return true;
    }
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#endif // EAGINE_MSGBUS_SERVICE_STREAM_HPP
