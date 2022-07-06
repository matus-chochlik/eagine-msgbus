/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus:message;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.reflection;
import eagine.core.serialization;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.main_ctx;
import :types;
import <chrono>;
import <cstdint>;
import <limits>;
import <vector>;

namespace eagine {

export template <identifier_t Sid, typename Selector>
struct get_serialize_buffer_size<Sid, message_id, Selector>
  : get_serialize_buffer_size<Sid, message_id::base, Selector> {};
//------------------------------------------------------------------------------
namespace msgbus {
//------------------------------------------------------------------------------
/// @brief Alias for default serialization backend for bus messages.
/// @ingroup msgbus
/// @see default_deserializer_backend
export using default_serializer_backend = portable_serializer_backend;

/// @brief Alias for default deserialization backend for bus messages.
/// @ingroup msgbus
/// @see default_serializer_backend
export using default_deserializer_backend = portable_deserializer_backend;

/// @brief Returns count of bytes required for serialization of the specified object.
/// @ingroup msgbus
export template <typename T>
auto default_serialize_buffer_size_for(const T& inst) noexcept {
    return serialize_buffer_size_for<default_serializer_backend::id_value>(
      inst, default_selector_t{});
}

/// @brief Returns a vector for the serialization of the specified object.
/// @ingroup msgbus
export template <typename T>
auto default_serialize_vector_for(const T& inst) noexcept {
    return get_serialize_vector_for<default_serializer_backend::id_value>(
      inst, default_selector_t{});
}

/// @brief Returns a suitable buffer for the serialization of the specified object.
/// @ingroup msgbus
export template <typename T>
auto default_serialize_buffer_for(const T& inst) noexcept {
    return serialize_buffer_for<default_serializer_backend::id_value>(inst);
}
//------------------------------------------------------------------------------
// TODO
// #define EAGINE_MSGBUS_ID(METHOD) EAGINE_MSG_ID(eagiMsgBus, METHOD)
//------------------------------------------------------------------------------
/// @brief Indicates if the specified message id denotes a special message bus message.
/// @ingroup msgbus
export constexpr auto is_special_message(const message_id msg_id) noexcept {
    return msg_id.has_class(identifier{"eagiMsgBus"});
}
//------------------------------------------------------------------------------
/// @brief Returns the special broadcast message bus endpoint id.
/// @ingroup msgbus
export constexpr auto broadcast_endpoint_id() noexcept -> identifier_t {
    return 0U;
}
//------------------------------------------------------------------------------
/// @brief Returns the special invalid message bus endpoint id.
/// @ingroup msgbus
/// @see is_valid_id
export constexpr auto invalid_endpoint_id() noexcept -> identifier_t {
    return 0U;
}
//------------------------------------------------------------------------------
/// @brief Indicates if the specified endpoint id is valid.
/// @ingroup msgbus
/// @see invalid_endpoint_id
export constexpr auto is_valid_endpoint_id(const identifier_t id) noexcept
  -> bool {
    return id != 0U;
}
//------------------------------------------------------------------------------
/// @brief Alias for message timestamp type.
/// @ingroup msgbus
/// @see message_age
export using message_timestamp = std::chrono::steady_clock::time_point;
//------------------------------------------------------------------------------
/// @brief Alias for message age type.
/// @ingroup msgbus
/// @see message_timestamp
export using message_age = std::chrono::duration<float>;
//------------------------------------------------------------------------------
/// @brief Message priority enumeration.
/// @ingroup msgbus
export enum class message_priority : std::uint8_t {
    /// @brief Idle, sent only when no messages with higher priority are enqueued.
    idle,
    /// @brief Low message priority.
    low,
    /// @brief Normal, default message priority.
    normal,
    /// @brief High, sent before messages with lower priority.
    high,
    /// @brief Critical, sent as soon as possible.
    critical
};
//------------------------------------------------------------------------------
/// @brief Message priority ordering.
/// @ingroup msgbus
/// @relates message_priority
export auto operator<(
  const message_priority l,
  const message_priority r) noexcept -> bool {
    using U = std::underlying_type_t<message_priority>;
    return U(l) < U(r);
}
//------------------------------------------------------------------------------
export template <typename Selector>
constexpr auto enumerator_mapping(
  const std::type_identity<message_priority>,
  const Selector) noexcept {
    return enumerator_map_type<message_priority, 5>{
      {{"critical", message_priority::critical},
       {"high", message_priority::high},
       {"normal", message_priority::normal},
       {"low", message_priority::low},
       {"idle", message_priority::idle}}};
}
//------------------------------------------------------------------------------
/// @brief Message cryptography-related flag bits enumeration.
/// @ingroup msgbus
/// @see message_crypto_flags
export enum class message_crypto_flag : std::uint8_t {
    /// @brief Assymetric cipher is used (symmetric otherwise).
    asymmetric = 1U << 0U,
    /// @brief The message header is signed.
    signed_header = 1U << 1U,
    /// @brief The message content is signed.
    signed_content = 1U << 2U
};
/// @brief  Alias for message crypto flags bitfield.
/// @ingroup msgbus
export using message_crypto_flags = bitfield<message_crypto_flag>;
//------------------------------------------------------------------------------
export template <typename Selector>
constexpr auto enumerator_mapping(
  const std::type_identity<message_crypto_flag>,
  const Selector) noexcept {
    return enumerator_map_type<message_crypto_flag, 3>{
      {{"asymmetric", message_crypto_flag::asymmetric},
       {"signed_header", message_crypto_flag::signed_header},
       {"signed_content", message_crypto_flag::signed_content}}};
}
//------------------------------------------------------------------------------
/// @brief Structure storing information about a sigle message bus message
/// @ingroup msgbus
/// @see message_view
/// @see stored_message
export struct message_info {
    static constexpr auto invalid_id() noexcept -> identifier_t {
        return 0U;
    }

    /// @brief Returns the source endpoint identifier.
    /// @see target_id
    /// @see set_source_id
    identifier_t source_id{broadcast_endpoint_id()};

    /// @brief Returns the target endpoint identifier.
    /// @see source_id
    /// @see set_target_id
    identifier_t target_id{broadcast_endpoint_id()};

    /// @brief Returns the identifier of the used serializer.
    /// @see set_serializer_id
    /// @see has_serializer_id
    identifier_t serializer_id{invalid_id()};

    /// @brief Alias for the sequence number type.
    /// @see set_sequence_no
    using sequence_t = message_sequence_t;

    /// @brief The message sequence number.
    /// @see set_sequence_no
    sequence_t sequence_no{0U};

    /// @brief Alias for type used to store the message hop count.
    using hop_count_t = std::int8_t;

    /// @brief The message hop counter.
    /// @see add_hop
    /// @see too_many_hops
    /// Counts how many times the message passed through a router or a bridge.
    hop_count_t hop_count{0};

    /// @brief Alias for type used to store the message age in quarter seconds.
    using age_t = std::int8_t;

    /// @brief The message age in quarter seconds.
    /// @see age
    /// @see add_age
    /// @see too_old
    /// Accumulated time the message spent in various queues.
    age_t age_quarter_seconds{0};

    /// @brief The message priority.
    /// @see set_priority
    message_priority priority{message_priority::normal};

    /// @brief The message cryptography flags.
    message_crypto_flags crypto_flags{};

    auto assign(const message_info& that) noexcept -> auto& {
        return *this = that;
    }

    /// @brief Indicates that the message made too many hops.
    /// @see hop_count
    /// @see add_hop
    auto too_many_hops() const noexcept -> bool {
        return hop_count >= hop_count_t(64);
    }

    /// @brief Increments the hop counter.
    /// @see hop_count
    /// @see too_many_hops
    auto add_hop() noexcept -> auto& {
        assert(hop_count < std::numeric_limits<hop_count_t>::max());
        ++hop_count;
        return *this;
    }

    /// @brief Indicates that the message is too old.
    /// @see age
    /// @see add_age
    auto too_old() const noexcept -> bool {
        switch(priority) {
            case message_priority::idle:
                return age_quarter_seconds > 10 * 4;
            case message_priority::low:
                return age_quarter_seconds > 20 * 4;
            case message_priority::normal:
                return age_quarter_seconds > 30 * 4;
            case message_priority::high:
                return age_quarter_seconds == std::numeric_limits<age_t>::max();
            case message_priority::critical:
                break;
        }
        return false;
    }

    /// @brief Adds to the age seconds counter.
    /// @see age
    /// @see too_old
    auto add_age(const message_age age) noexcept -> auto& {
        const float added_quarter_seconds = (age.count() + 0.20F) * 4.F;
        if(const auto new_age{convert_if_fits<age_t>(
             int(age_quarter_seconds) + int(added_quarter_seconds))}) {
            age_quarter_seconds = extract(new_age);
        } else {
            age_quarter_seconds = std::numeric_limits<age_t>::max();
        }
        return *this;
    }

    /// @brief Returns the message age
    /// @see too_old
    /// @see add_age
    auto age() const noexcept -> message_age {
        return message_age{float(age_quarter_seconds) * 0.25F};
    }

    /// @brief Sets the priority of this message.
    auto set_priority(const message_priority new_priority) noexcept -> auto& {
        priority = new_priority;
        return *this;
    }

    /// @brief Sets the source endpoint identifier.
    auto set_source_id(const identifier_t id) noexcept -> auto& {
        source_id = id;
        return *this;
    }

    /// @brief Sets the target endpoint identifier.
    auto set_target_id(const identifier_t id) noexcept -> auto& {
        target_id = id;
        return *this;
    }

    /// @brief Tests if a data serializer with the specified id was used.
    /// @see serializer_id
    /// @see set_serializer_id
    auto has_serializer_id(const identifier id) const noexcept -> bool {
        return serializer_id == id.value();
    }

    /// @brief Sets the id of the used data content serializer.
    /// @see serializer_id
    /// @see has_serializer_id
    auto set_serializer_id(const identifier id) noexcept -> auto& {
        serializer_id = id.value();
        return *this;
    }

    /// @brief Sets the sequence number of this message (has message-type specific meaning).
    /// @see sequence_no
    auto set_sequence_no(const message_sequence_t no) noexcept -> auto& {
        sequence_no = no;
        return *this;
    }

    /// @brief Sets the target id to be the source id from info, copies sequence number.
    /// @see source_id
    /// @see target_id
    /// @see sequence_no
    auto setup_response(const message_info& info) noexcept -> auto& {
        target_id = info.source_id;
        sequence_no = info.sequence_no;
        age_quarter_seconds = info.age_quarter_seconds;
        priority = info.priority;
        return *this;
    }
};
//------------------------------------------------------------------------------
/// @brief Combines message information and a non-owning view to message content.
/// @ingroup msgbus
export class message_view : public message_info {
public:
    /// @brief Default constructor.
    constexpr message_view() noexcept = default;

    /// @brief Construction from a const memory block.
    constexpr message_view(const memory::const_block init) noexcept
      : _data{init} {}

    /// @brief Construction from a mutable memory block.
    constexpr message_view(memory::block init) noexcept
      : _data{init} {}

    /// @brief Construction from a string view.
    constexpr message_view(const string_view init) noexcept
      : _data{as_bytes(init)} {}

    /// @brief Construction from a message info and a const memory block.
    constexpr message_view(
      const message_info info,
      memory::const_block init) noexcept
      : message_info{info}
      , _data{init} {}

    /// @brief Indicates if the header or the content is signed.
    /// @see signature
    auto is_signed() const noexcept -> bool {
        return crypto_flags.has(message_crypto_flag::signed_content) ||
               crypto_flags.has(message_crypto_flag::signed_header);
    }

    /// @brief Returns a const view of the storage buffer.
    auto data() const noexcept -> memory::const_block {
        return _data;
    }

    /// @brief Returns the message signature.
    /// @see is_signed
    /// @see content
    auto signature() const noexcept -> memory::const_block {
        if(is_signed()) {
            return skip(data(), skip_data_with_size(data()));
        }
        return {};
    }

    /// @brief Returns a const view of the data content of the message.
    /// @see signature
    /// @see text_content
    /// @see const_content
    auto content() const noexcept -> memory::const_block {
        if(is_signed()) [[unlikely]] {
            return get_data_with_size(data());
        }
        return data();
    }

    /// @brief Returns the content as a const string view.
    /// @see content
    auto text_content() const noexcept {
        return as_chars(content());
    }

private:
    /// @brief View of the message data content.
    memory::const_block _data;
};
//------------------------------------------------------------------------------
/// @brief Serializes a bus message header with the specified serializer backend.
/// @ingroup msgbus
/// @see serialize_message
/// @see deserialize_message_header
export template <typename Backend>
auto serialize_message_header(
  const message_id msg_id,
  const message_view& msg,
  Backend& backend) noexcept -> serialization_errors
    requires(std::is_base_of_v<serializer_backend, Backend>)
{

    auto message_params = std::make_tuple(
      msg_id.class_(),
      msg_id.method(),
      msg.source_id,
      msg.target_id,
      msg.serializer_id,
      msg.sequence_no,
      msg.hop_count,
      msg.age_quarter_seconds,
      msg.priority,
      msg.crypto_flags);
    return serialize(message_params, backend);
}
//------------------------------------------------------------------------------
/// @brief Serializes a bus message with the specified serializer backend.
/// @ingroup msgbus
/// @see serialize_message_header
/// @see deserialize_message
/// @see default_serialize
export template <typename Backend>
auto serialize_message(
  const message_id msg_id,
  const message_view& msg,
  Backend& backend) noexcept -> serialization_errors
    requires(std::is_base_of_v<serializer_backend, Backend>)
{

    auto errors = serialize_message_header(msg_id, msg, backend);

    if(!errors) {
        if(auto sink{backend.sink()}) {
            errors |= extract(sink).write(msg.data());
        } else {
            errors |= serialization_error_code::backend_error;
        }
    }

    return errors;
}
//------------------------------------------------------------------------------
/// @brief Uses the default backend to serialize a value into a memory block.
/// @see default_serializer_backend
/// @see default_serialize_packed
/// @see default_deserialize
/// @see serialize
export template <typename T>
inline auto default_serialize(const T& value, memory::block blk) noexcept
  -> serialization_result<memory::const_block> {
    block_data_sink sink(blk);
    default_serializer_backend backend(sink);
    auto errors = serialize(value, backend);
    return {sink.done(), errors};
}
//------------------------------------------------------------------------------
/// @brief Uses backend and compressor to serialize and pack a value into a memory block.
/// @see default_serializer_backend
/// @see default_serialize
/// @see default_deserialize_packed
/// @see data_compressor
/// @see serialize
export template <typename T>
auto default_serialize_packed(
  T& value,
  memory::block blk,
  data_compressor compressor) noexcept
  -> serialization_result<memory::const_block> {
    packed_block_data_sink sink(std::move(compressor), blk);
    default_serializer_backend backend(sink);
    auto errors = serialize(value, backend);
    return {sink.done(), errors};
}
//------------------------------------------------------------------------------
/// @brief Default-serializes the specified message id into a memory block.
/// @ingroup msgbus
/// @see default_serializer_backend
/// @see default_serialize
/// @see message_id
export auto default_serialize_message_type(
  const message_id msg_id,
  memory::block blk) noexcept {
    const auto value{msg_id.id_tuple()};
    return default_serialize(value, blk);
}
//------------------------------------------------------------------------------
export class context;
/// @brief Combines message information and an owned message content buffer.
/// @ingroup msgbus
export class stored_message : public message_info {
public:
    /// @brief Default constructor.
    stored_message() noexcept = default;

    /// @brief Construction from a message view and storage buffer.
    /// Adopts the buffer and copies the content from the message view into it.
    stored_message(const message_view message, memory::buffer buf) noexcept
      : message_info{message}
      , _buffer{std::move(buf)} {
        memory::copy_into(view(message.data()), _buffer);
    }

    /// @brief Conversion to message view.
    operator message_view() const noexcept {
        return {*this, data()};
    }

    /// @brief Copies the remaining data from the specified serialization source.
    template <typename Source>
    void fetch_all_from(Source& source) noexcept {
        _buffer.clear();
        source.fetch_all(_buffer);
    }

    /// @brief Copies the content from the given block into the internal buffer.
    void store_content(memory::const_block blk) noexcept {
        memory::copy_into(blk, _buffer);
    }

    template <typename Backend, typename Value>
    auto do_store_value(const Value& value, span_size_t max_size) noexcept
      -> bool;

    /// @brief Serializes and stores the specified value (up to max_size).
    template <typename Value>
    auto store_value(const Value& value, span_size_t max_size) noexcept -> bool;

    template <typename Backend, typename Value>
    auto do_fetch_value(Value& value) noexcept -> bool;

    /// @brief Deserializes the stored content into the specified value.
    template <typename Value>
    auto fetch_value(Value& value) noexcept -> bool;

    /// @brief Returns a mutable view of the storage buffer.
    auto storage() noexcept -> memory::block {
        return cover(_buffer);
    }

    /// @brief Returns a const view of the storage buffer.
    auto data() const noexcept -> memory::const_block {
        return view(_buffer);
    }

    /// @brief Indicates if the header or the content is signed.
    /// @see signature
    auto is_signed() const noexcept -> bool {
        return crypto_flags.has(message_crypto_flag::signed_content) ||
               crypto_flags.has(message_crypto_flag::signed_header);
    }

    /// @brief Returns the message signature.
    /// @see is_signed
    /// @see content
    auto signature() const noexcept -> memory::const_block {
        if(is_signed()) {
            return skip(data(), skip_data_with_size(data()));
        }
        return {};
    }

    /// @brief Returns a mutable view of the data content of the message.
    /// @see signature
    auto content() noexcept -> memory::block {
        if(is_signed()) [[unlikely]] {
            return get_data_with_size(storage());
        }
        return storage();
    }

    /// @brief Returns a const view of the data content of the message.
    /// @see signature
    /// @see text_content
    /// @see const_content
    auto content() const noexcept -> memory::const_block {
        if(is_signed()) [[unlikely]] {
            return get_data_with_size(data());
        }
        return data();
    }

    /// @brief Returns a const view of the data content of the message.
    /// @see signature
    /// @see content
    /// @see text_content
    auto const_content() const noexcept -> memory::const_block {
        return content();
    }

    /// @brief Returns the content as a mutable string view.
    /// @see content
    auto text_content() noexcept {
        return as_chars(content());
    }

    /// @brief Returns the content as a const string view.
    /// @see content
    auto text_content() const noexcept {
        return as_chars(content());
    }

    /// @brief Returns the content as a const string view.
    /// @see content
    /// @see text_content
    /// @see const_content
    auto const_text_content() const noexcept {
        return as_chars(const_content());
    }

    /// @brief Clears the content of the storage buffer.
    void clear_data() noexcept {
        _buffer.clear();
    }

    /// @brief Releases and returns the storage buffer (without clearing it).
    auto release_buffer() noexcept -> memory::buffer {
        return std::move(_buffer);
    }

    /// @brief Stores the specified data and signs it.
    auto store_and_sign(
      const memory::const_block data,
      const span_size_t max_size,
      context&,
      main_ctx_object&) noexcept -> bool;

    /// @brief Verifies the signatures of this message.
    auto verify_bits(context&, main_ctx_object&) const noexcept
      -> verification_bits;

private:
    memory::buffer _buffer{};
};
//------------------------------------------------------------------------------
/// @brief Deserializes a bus message header with the specified deserializer backend.
/// @ingroup msgbus
/// @see deserialize_message
/// @see serialize_message_header
export template <typename Backend>
auto deserialize_message_header(
  identifier& class_id,
  identifier& method_id,
  stored_message& msg,
  Backend& backend) noexcept -> deserialization_errors
    requires(std::is_base_of_v<deserializer_backend, Backend>)
{

    auto message_params = std::tie(
      class_id,
      method_id,
      msg.source_id,
      msg.target_id,
      msg.serializer_id,
      msg.sequence_no,
      msg.hop_count,
      msg.age_quarter_seconds,
      msg.priority,
      msg.crypto_flags);
    return deserialize(message_params, backend);
}
//------------------------------------------------------------------------------
/// @brief Deserializes a bus message with the specified deserializer backend.
/// @ingroup msgbus
/// @see deserialize_message_header
/// @see serialize_message
/// @see default_deserialize
export template <typename Backend>
auto deserialize_message(
  identifier& class_id,
  identifier& method_id,
  stored_message& msg,
  Backend& backend) noexcept -> deserialization_errors
    requires(std::is_base_of_v<deserializer_backend, Backend>)
{

    auto errors = deserialize_message_header(class_id, method_id, msg, backend);

    if(!errors) {
        if(auto source{backend.source()}) {
            msg.fetch_all_from(extract(source));
        } else {
            errors |= deserialization_error_code::backend_error;
        }
    }

    return errors;
}
//------------------------------------------------------------------------------
/// @brief Deserializes a bus message with the specified deserializer backend.
/// @ingroup msgbus
/// @see deserialize_message_header
/// @see serialize_message
export template <typename Backend>
auto deserialize_message(
  message_id& msg_id,
  stored_message& msg,
  Backend& backend) noexcept -> deserialization_errors
    requires(std::is_base_of_v<deserializer_backend, Backend>)
{

    identifier class_id{};
    identifier method_id{};
    deserialization_errors errors =
      deserialize_message(class_id, method_id, msg, backend);
    if(!errors) {
        msg_id = {class_id, method_id};
    }
    return errors;
}
//------------------------------------------------------------------------------
// default_deserialize
//------------------------------------------------------------------------------
/// @brief Uses the default backend to deserialize a value from a memory block.
/// @see default_deserializer_backend
/// @see default_deserialize_packed
/// @see default_serialize
/// @see deserialize
export template <typename T>
auto default_deserialize(T& value, const memory::const_block blk) noexcept
  -> deserialization_result<memory::const_block> {
    block_data_source source(blk);
    default_deserializer_backend backend(source);
    auto errors = deserialize(value, backend);
    return {source.remaining(), errors};
}
//------------------------------------------------------------------------------
/// @brief Uses backend and compressor to deserialize and unpack a value from a block.
/// @see default_deserializer_backend
/// @see default_deserialize
/// @see default_serialize_packed
/// @see data_compressor
/// @see deserialize
export template <typename T>
auto default_deserialize_packed(
  T& value,
  const memory::const_block blk,
  data_compressor compressor) noexcept
  -> deserialization_result<memory::const_block> {
    packed_block_data_source source(std::move(compressor), blk);
    default_deserializer_backend backend(source);
    auto errors = deserialize(value, backend);
    return {source.remaining(), errors};
}
//------------------------------------------------------------------------------
/// @brief Default-deserializes the specified message id from a memory block.
/// @ingroup msgbus
/// @see default_deserializer_backend
/// @see default_deserialize
/// @see message_id
export auto default_deserialize_message_type(
  message_id& msg_id,
  const memory::const_block blk) noexcept {
    std::tuple<identifier, identifier> value{};
    auto result = default_deserialize(value, blk);
    if(result) {
        msg_id = {value};
    }
    return result;
}
//------------------------------------------------------------------------------
template <typename Backend, typename Value>
auto stored_message::do_store_value(
  const Value& value,
  const span_size_t max_size) noexcept -> bool {
    _buffer.resize(max_size);
    block_data_sink sink(cover(_buffer));
    Backend backend(sink);
    auto errors = serialize(value, backend);
    if(!errors) {
        set_serializer_id(backend.type_id());
        _buffer.resize(sink.done().size());
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
template <typename Value>
auto stored_message::store_value(
  const Value& value,
  const span_size_t max_size) noexcept -> bool {
    return do_store_value<default_serializer_backend>(value, max_size);
}
//------------------------------------------------------------------------------
template <typename Backend, typename Value>
auto stored_message::do_fetch_value(Value& value) noexcept -> bool {
    block_data_source source(view(_buffer));
    Backend backend(source);
    auto errors = deserialize(value, backend);
    return !errors;
}
//------------------------------------------------------------------------------
template <typename Value>
auto stored_message::fetch_value(Value& value) noexcept -> bool {
    return do_fetch_value<default_deserializer_backend>(value);
}
//------------------------------------------------------------------------------
/// @brief Class storing message bus messages.
/// @ingroup msgbus
/// @see serialized_message_storage
export class message_storage {
public:
    /// @brief Default constructor.
    message_storage() {
        _messages.reserve(64);
    }

    /// @brief Indicates if the storage is empty.
    auto empty() const noexcept -> bool {
        return _messages.empty();
    }

    /// @brief Returns the coung of messages in the storage.
    auto count() const noexcept -> span_size_t {
        return span_size(_messages.size());
    }

    /// @brief Pushes a message into this storage.
    void push(const message_id msg_id, const message_view& message) noexcept {
        _messages.emplace_back(
          msg_id,
          stored_message{message, _buffers.get(message.data().size())},
          _clock_t::now());
    }

    /// @brief Pushes a new message and lets a function to fill it.
    ///
    /// The function's Boolean return value indicates if the message should be kept.
    template <typename Function>
    auto push_if(Function function, const span_size_t req_size = 0) noexcept
      -> bool {
        _messages.emplace_back(
          message_id{},
          stored_message{{}, _buffers.get(req_size)},
          _clock_t::now());
        auto& [msg_id, message, insert_time] = _messages.back();
        (void)(insert_time);
        bool rollback = false;
        try {
            if(!function(msg_id, insert_time, message)) {
                rollback = true;
            }
        } catch(...) {
            rollback = true;
        }
        if(rollback) {
            _buffers.eat(message.release_buffer());
            _messages.pop_back();
            return false;
        }
        return true;
    }

    /// @brief Alias for the message fetch handler.
    /// @see fetch_all
    ///
    /// The return value indicates if the message is considered handled
    /// and should be removed.
    using fetch_handler = callable_ref<
      bool(const message_id, const message_age, const message_view&) noexcept>;

    /// @brief Fetches all currently stored messages and calls handler on them.
    auto fetch_all(const fetch_handler handler) noexcept -> bool;

    /// @brief Alias for message cleanup callable predicate.
    /// @see cleanup
    ///
    /// The return value indicates if a message should be removed.
    using cleanup_predicate = callable_ref<bool(const message_age) noexcept>;

    /// @brief Removes messages based on the result of the specified predicate.
    void cleanup(const cleanup_predicate predicate) noexcept;

    void log_stats(main_ctx_object&);

private:
    using _clock_t = std::chrono::steady_clock;
    memory::buffer_pool _buffers{};
    std::vector<std::tuple<message_id, stored_message, message_timestamp>>
      _messages;
};
//------------------------------------------------------------------------------
export class message_pack_info {
public:
    using bit_set = std::uint64_t;

    message_pack_info(const span_size_t total_size) noexcept
      : _total_size{limit_cast<std::uint16_t>(total_size)} {}

    operator bool() const noexcept {
        return !is_empty();
    }

    auto is_empty() const noexcept -> bool {
        return _packed_bits == 0U;
    }

    auto bits() const noexcept -> bit_set {
        return _packed_bits;
    }

    auto count() const noexcept -> span_size_t {
        span_size_t result = 0;
        auto bits = _packed_bits;
        while(bits) {
            ++result;
            bits &= (bits - 1U);
        }
        return result;
    }

    auto used() const noexcept -> span_size_t {
        return span_size(_packed_size);
    }

    auto total() const noexcept -> span_size_t {
        return span_size(_total_size);
    }

    auto usage() const noexcept {
        return float(used()) / float(total());
    }

    void add(const span_size_t msg_size, const bit_set current_bit) noexcept {
        _packed_size += limit_cast<std::uint16_t>(msg_size);
        _packed_bits |= current_bit;
    }

private:
    bit_set _packed_bits{0U};
    std::uint16_t _packed_size{0};
    const std::uint16_t _total_size{0};
};
//------------------------------------------------------------------------------
export class serialized_message_storage {
public:
    /// The return value indicates if the message is considered handled
    /// and should be removed.
    using fetch_handler = callable_ref<
      bool(const message_timestamp, const memory::const_block) noexcept>;

    serialized_message_storage() noexcept {
        _messages.reserve(32);
    }

    auto empty() const noexcept -> bool {
        return _messages.empty();
    }

    auto count() const noexcept -> span_size_t {
        return span_size(_messages.size());
    }

    auto top() const noexcept -> memory::const_block {
        if(!_messages.empty()) {
            return view(std::get<0>(_messages.front()));
        }
        return {};
    }

    void pop() noexcept {
        assert(!_messages.empty());
        _buffers.eat(std::move(std::get<0>(_messages.front())));
        _messages.erase(_messages.begin());
    }

    void push(const memory::const_block message) noexcept {
        assert(!message.empty());
        auto buf = _buffers.get(message.size());
        memory::copy_into(message, buf);
        _messages.emplace_back(std::move(buf), _clock_t::now());
    }

    auto fetch_all(const fetch_handler handler) noexcept -> bool;

    auto pack_into(memory::block dest) noexcept -> message_pack_info;

    void cleanup(const message_pack_info& to_be_removed) noexcept;

    void log_stats(main_ctx_object&);

private:
    using _clock_t = std::chrono::steady_clock;
    memory::buffer_pool _buffers{};
    std::vector<std::tuple<memory::buffer, message_timestamp>> _messages;
};
//------------------------------------------------------------------------------
export class endpoint;
//------------------------------------------------------------------------------
export class message_context {
public:
    message_context(endpoint& ep) noexcept
      : _bus{ep} {}

    constexpr message_context(endpoint& ep, message_id mi) noexcept
      : _bus{ep}
      , _msg_id{std::move(mi)} {}

    auto bus_node() const noexcept -> endpoint& {
        return _bus;
    }

    auto msg_id() const noexcept -> const message_id& {
        return _msg_id;
    }

    auto set_msg_id(message_id msg_id) noexcept -> message_context& {
        _msg_id = std::move(msg_id);
        return *this;
    }

private:
    endpoint& _bus;
    message_id _msg_id{};
};
//------------------------------------------------------------------------------
export class message_priority_queue {
public:
    using handler_type =
      callable_ref<bool(const message_context&, const stored_message&) noexcept>;

    message_priority_queue() noexcept {
        _messages.reserve(128);
    }

    auto size() const noexcept {
        return _messages.size();
    }

    auto push(const message_view& message) noexcept -> stored_message& {
        const auto pos = std::lower_bound(
          _messages.begin(),
          _messages.end(),
          message.priority,
          [](auto& msg, auto pri) { return msg.priority < pri; });

        return *_messages.emplace(
          pos, message, _buffers.get(message.data().size()));
    }

    auto process_one(
      const message_context& msg_ctx,
      const handler_type handler) noexcept -> bool {
        if(!_messages.empty()) {
            if(handler(msg_ctx, _messages.back())) {
                _buffers.eat(_messages.back().release_buffer());
                _messages.pop_back();
                return true;
            }
        }
        return false;
    }

    auto process_all(
      const message_context& msg_ctx,
      const handler_type handler) noexcept -> span_size_t {
        span_size_t count{0};
        std::size_t pos = 0;
        while(pos < _messages.size()) {
            if(handler(msg_ctx, _messages[pos])) {
                ++count;
                _buffers.eat(_messages[pos].release_buffer());
                _messages.erase(_messages.begin() + signedness_cast(pos));
            } else {
                ++pos;
            }
        }
        return count;
    }

private:
    memory::buffer_pool _buffers{};
    std::vector<stored_message> _messages;
};
//------------------------------------------------------------------------------
export class connection_outgoing_messages {
public:
    auto count() const noexcept -> span_size_t {
        return _serialized.count();
    }

    auto empty() const noexcept -> bool {
        return _serialized.empty();
    }

    auto enqueue(
      main_ctx_object& user,
      const message_id,
      const message_view&,
      memory::block) noexcept -> bool;

    auto pack_into(memory::block dest) noexcept -> message_pack_info {
        return _serialized.pack_into(dest);
    }

    void cleanup(const message_pack_info& packed) noexcept {
        _serialized.cleanup(packed);
    }

    void log_stats(main_ctx_object& user) {
        _serialized.log_stats(user);
    }

private:
    serialized_message_storage _serialized{};
};
//------------------------------------------------------------------------------
export class connection_incoming_messages {
public:
    using fetch_handler = callable_ref<
      bool(const message_id, const message_age, const message_view&) noexcept>;

    auto empty() const noexcept -> bool {
        return _packed.empty();
    }

    auto count() const noexcept -> span_size_t {
        return _packed.count();
    }

    void push(const memory::const_block data) noexcept {
        _packed.push(data);
    }

    auto fetch_messages(
      main_ctx_object& user,
      const fetch_handler handler) noexcept -> bool;

    void log_stats(main_ctx_object& user) {
        _packed.log_stats(user);
        _unpacked.log_stats(user);
    }

private:
    serialized_message_storage _packed{};
    message_storage _unpacked{};
};
//------------------------------------------------------------------------------
} // namespace msgbus
} // namespace eagine
