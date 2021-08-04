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
#include <eagine/flat_set.hpp>
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
constexpr auto data_member_mapping(
  type_identity<stream_info>,
  Selector) noexcept {
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
    signal<void(const identifier_t relay_id)> stream_relay_assigned;

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
                  EAGINE_MSG_ID(eagiStream, startFrwrd));
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
        if(msg_id == EAGINE_MSG_ID(eagiStream, startFrwrd)) {
            if(!has_stream_relay() || (_stream_relay_hops > sub_info.hop_count)) {
                set_stream_relay(sub_info.endpoint_id, sub_info.hop_count);
            }
        }
    }

    void _handle_stream_relay_unsubscribed(
      const subscriber_info& sub_info,
      message_id msg_id) {
        if(msg_id == EAGINE_MSG_ID(eagiStream, startFrwrd)) {
            if(_stream_relay_id == sub_info.endpoint_id) {
                reset_stream_relay();
            }
        }
    }

    identifier_t _stream_relay_id{invalid_endpoint_id()};
    timeout _stream_relay_timeout{endpoint_alive_notify_period() * 2, nothing};
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
    /// @brief Adds the information about a new stream. Returns the stream id.
    /// @see remove_stream
    /// @see send_stream_data
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
        auto& stream = _streams[info.id];
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
            _retract_stream(this->stream_relay(), stream_id);
        }
        return _streams.erase(stream_id) > 0;
    }

    /// @brief Sends a fragment of encoded stream data.
    /// @see add_stream
    auto send_stream_data(identifier_t stream_id, memory::const_block data)
      -> bool {
        if(this->has_stream_relay()) {
            const auto pos = _streams.find(stream_id);
            if(pos != _streams.end()) {
                if(pos->second.send_data) {
                    // TODO
                    EAGINE_MAYBE_UNUSED(data);
                }
            }
        }
        return false;
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
          EAGINE_MSG_MAP(eagiStream, startSend, This, _handle_start_send_data));

        base::add_method(
          this,
          EAGINE_MSG_MAP(eagiStream, stopSend, This, _handle_stop_send_data));
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

    auto _handle_start_send_data(
      const message_context&,
      const stored_message& message) -> bool {
        identifier_t stream_id{0};
        if(default_deserialize(stream_id, message.content())) {
            const auto pos = _streams.find(stream_id);
            if(pos != _streams.end()) {
                auto& stream = pos->second;
                stream.sequence = 0U;
                stream.send_data = true;
            }
        }
        return true;
    }

    auto _handle_stop_send_data(
      const message_context&,
      const stored_message& message) -> bool {
        identifier_t stream_id{0};
        if(default_deserialize(stream_id, message.content())) {
            const auto pos = _streams.find(stream_id);
            if(pos != _streams.end()) {
                auto& stream = pos->second;
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
    using stream_key_t = std::tuple<identifier_t, identifier_t>;

public:
    /// @brief Triggered when a data stream has appeared at the given provider.
    /// @see stream_disappeared
    signal<void(
      const identifier_t provider_id,
      const stream_info&,
      const verification_bits verified)>
      stream_appeared;

    /// @brief Triggered when a data stream has been lost at the given provider.
    /// @see stream_appeared
    signal<void(
      const identifier_t provider_id,
      const stream_info&,
      const verification_bits verified)>
      stream_disappeared;

    /// @brief Subscribes to the data from the specified stream.
    /// @see unsubscribe_from_stream
    void subscribe_to_stream(identifier_t provider_id, identifier_t stream_id) {
        const stream_key_t key{provider_id, stream_id};
        auto pos = _streams.find(key);
        if(pos == _streams.end()) {
            pos = _streams.emplace(key, stream_status{}).first;
        }
        if(pos->second.stream_timeout) {
            _do_subscribe(key);
        }
    }

    /// @brief Unsubscribes from the specified stream.
    /// @seei subscribe_to_stream
    void unsubscribe_from_stream(
      identifier_t provider_id,
      identifier_t stream_id) {
        const stream_key_t key{provider_id, stream_id};
        auto pos = _streams.find(key);
        if(pos != _streams.end()) {
            _do_unsubscribe(key);
            _streams.erase(pos);
        }
    }

protected:
    using base::base;

    void add_methods() {
        base::add_methods();
        base::add_method(
          this,
          EAGINE_MSG_MAP(eagiStream, appeared, This, _handle_stream_appeared));
        base::add_method(
          this,
          EAGINE_MSG_MAP(
            eagiStream, disapeared, This, _handle_stream_disappeared));
    }

    auto update() -> work_done {
        some_true something_done{base::update()};
        // TODO
        return something_done;
    }

private:
    void _do_subscribe(const stream_key_t& key) {
        auto buffer = default_serialize_buffer_for(key);
        auto serialized{default_serialize(key, cover(buffer))};
        EAGINE_ASSERT(serialized);
        message_view message{extract(serialized)};
        message.set_target_id(this->stream_relay());
        this->bus_node().post(EAGINE_MSG_ID(eagiStream, startFrwrd), message);
    }

    void _do_unsubscribe(const stream_key_t& key) {
        auto buffer = default_serialize_buffer_for(key);
        auto serialized{default_serialize(key, cover(buffer))};
        EAGINE_ASSERT(serialized);
        message_view message{extract(serialized)};
        message.set_target_id(this->stream_relay());
        this->bus_node().post(EAGINE_MSG_ID(eagiStream, stopFrwrd), message);
    }

    auto _handle_stream_appeared(
      const message_context&,
      const stored_message& message) -> bool {
        stream_info info{};
        if(default_deserialize(info, message.content())) {
            stream_appeared(
              message.source_id, info, this->verify_bits(message));
        }
        return true;
    }

    auto _handle_stream_disappeared(
      const message_context&,
      const stored_message& message) -> bool {
        stream_info info{};
        if(default_deserialize(info, message.content())) {
            stream_disappeared(
              message.source_id, info, this->verify_bits(message));
        }
        return true;
    }

    struct stream_status {
        stream_info info{};
        timeout stream_timeout{std::chrono::seconds{3}, nothing};
    };
    std::map<identifier_t, stream_status> _streams;
};
//------------------------------------------------------------------------------
/// @brief Service relaying stream data between providers and consumers.
/// @ingroup msgbus
/// @see service_composition
/// @see stream_provider
/// @see stream_consumer
template <typename Base = subscriber>
class stream_relay
  : public require_services<Base, subscriber_discovery, pingable> {
    using This = stream_relay;
    using base = require_services<Base, subscriber_discovery, pingable>;
    using stream_key_t = std::tuple<identifier_t, identifier_t>;

public:
    /// @brief Triggered when a data stream was announced by the given provider.
    /// @see stream_retracted
    signal<void(
      const identifier_t provider_id,
      const stream_info&,
      const verification_bits verified)>
      stream_announced;

    /// @brief Triggered when a data stream was retracted by the given provider.
    /// @see stream_announced
    signal<void(
      const identifier_t provider_id,
      const stream_info&,
      const verification_bits verified)>
      stream_retracted;

protected:
    using base::base;

    void add_methods() {
        base::add_methods();

        base::add_method(
          this,
          EAGINE_MSG_MAP(eagiStream, announce, This, _handle_stream_announce));
        base::add_method(
          this,
          EAGINE_MSG_MAP(eagiStream, retract, This, _handle_stream_retract));
        base::add_method(
          this,
          EAGINE_MSG_MAP(eagiStream, startFrwrd, This, _handle_start_forward));
        base::add_method(
          this,
          EAGINE_MSG_MAP(eagiStream, stopFrwrd, This, _handle_stop_forward));
    }

    auto update() -> work_done {
        some_true something_done{base::update()};
        // TODO
        return something_done;
    }

private:
    struct provider_status {
        timeout provider_timeout;
    };

    struct consumer_status {
        timeout consumer_timeout;
    };

    struct relay_status {
        timeout relay_timeout;
    };

    struct stream_status {
        stream_info info{};
        timeout stream_timeout{std::chrono::seconds{5}};
        flat_set<identifier_t> forward_set{};
    };

    auto _handle_stream_announce(
      const message_context&,
      const stored_message& message) -> bool {
        stream_info info{};
        if(default_deserialize(info, message.content())) {
            const stream_key_t key{message.source_id, info.id};
            auto pos = _streams.find(key);
            bool added = false;
            if(pos == _streams.end()) {
                pos = _streams.emplace(key, stream_status{}).first;
                added = true;
            }
            auto& stream = pos->second;
            const bool changed = (stream.info.kind || info.kind) ||
                                 (stream.info.encoding || info.encoding) ||
                                 (stream.info.description != info.description);
            if(added || changed) {
                if(changed) {
                    if(!added) {
                        _forward_stream_retract(
                          message.source_id,
                          stream,
                          this->verify_bits(message),
                          message);
                    }
                    stream.info = info;
                }
                _forward_stream_announce(
                  message.source_id,
                  stream,
                  this->verify_bits(message),
                  message);
            }
            stream.stream_timeout.reset();
        }
        return true;
    }

    void _forward_stream_announce(
      identifier_t provider_id,
      const stream_status& stream,
      verification_bits verified,
      message_view message) {
        const auto msg_id{EAGINE_MSG_ID(eagiStream, appeared)};
        for(const auto consumer_id : stream.forward_set) {
            message.set_target_id(consumer_id);
            this->bus_node().post(msg_id, message);
        }
        stream_announced(provider_id, stream.info, verified);
    }

    auto _handle_stream_retract(
      const message_context&,
      const stored_message& message) -> bool {
        identifier_t stream_id{0};
        if(default_deserialize(stream_id, message.content())) {
            const auto pos = _streams.find({message.source_id, stream_id});
            if(pos != _streams.end()) {
                _forward_stream_retract(
                  message.source_id,
                  pos->second,
                  this->verify_bits(message),
                  message);
                _streams.erase(pos);
            }
        }
        return true;
    }

    void _forward_stream_retract(
      identifier_t provider_id,
      const stream_status& stream,
      verification_bits verified,
      message_view message) {
        const auto msg_id{EAGINE_MSG_ID(eagiStream, disapeared)};
        for(const auto consumer_id : stream.forward_set) {
            message.set_target_id(consumer_id);
            this->bus_node().post(msg_id, message);
        }
        stream_retracted(provider_id, stream.info, verified);
    }

    auto _handle_start_forward(const message_context&, const stored_message&)
      -> bool {
        return true;
    }

    auto _handle_stop_forward(const message_context&, const stored_message&)
      -> bool {
        return true;
    }

    void _handle_stream_relay_alive(const subscriber_info& sub_info) {
        auto ppos = _providers.find(sub_info.endpoint_id);
        if(ppos != _providers.end()) {
            ppos->second.provider_timeout.reset();
        }

        auto cpos = _consumers.find(sub_info.endpoint_id);
        if(cpos != _consumers.end()) {
            cpos->second.consumer_timeout.reset();
        }

        auto rpos = _relays.find(sub_info.endpoint_id);
        if(rpos != _relays.end()) {
            rpos->second.relay_timeout.reset();
        }
    }

    void _handle_stream_relay_subscribed(
      const subscriber_info& sub_info,
      message_id msg_id) {
        if(msg_id == EAGINE_MSG_ID(eagiStream, startFrwrd)) {
            auto pos = _relays.find(sub_info.endpoint_id);
            if(pos == _relays.end()) {
                pos =
                  _relays.emplace(sub_info.endpoint_id, relay_status{}).first;
            }
            pos->second.relay_timeout.reset();
        }
    }

    void _handle_stream_relay_unsubscribed(
      const subscriber_info& sub_info,
      message_id msg_id) {
        if(msg_id == EAGINE_MSG_ID(eagiStream, startFrwrd)) {
            auto pos = _relays.find(sub_info.endpoint_id);
            if(pos != _relays.end()) {
                _relays.erase(pos);
            }
        }
    }

    std::map<stream_key_t, stream_status> _streams;
    std::map<identifier_t, provider_status> _providers;
    std::map<identifier_t, consumer_status> _consumers;
    std::map<identifier_t, relay_status> _relays;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#endif // EAGINE_MSGBUS_SERVICE_STREAM_HPP
