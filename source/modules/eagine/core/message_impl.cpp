/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

module eagine.msgbus.core;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.reflection;
import eagine.core.serialization;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.main_ctx;
import eagine.core.c_api;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// message_info
//------------------------------------------------------------------------------
auto message_info::too_old() const noexcept -> bool {
    switch(priority) {
        case message_priority::idle:
            return age_quarter_seconds > 10 * 4;
        case message_priority::low:
            return age_quarter_seconds > 20 * 4;
        [[likely]] case message_priority::normal:
            return age_quarter_seconds > 30 * 4;
        case message_priority::high:
            return age_quarter_seconds == std::numeric_limits<age_t>::max();
        case message_priority::critical:
            break;
    }
    return false;
}
//------------------------------------------------------------------------------
auto message_info::add_age(const message_age age) noexcept -> message_info& {
    const auto added_quarter_seconds = (age.count() + 20) / 25;
    if(const auto new_age{convert_if_fits<age_t>(
         int(age_quarter_seconds) + int(added_quarter_seconds))}) [[likely]] {
        age_quarter_seconds = *new_age;
    } else {
        age_quarter_seconds = std::numeric_limits<age_t>::max();
    }
    return *this;
}
//------------------------------------------------------------------------------
// stored message
//------------------------------------------------------------------------------
auto stored_message::store_and_sign(
  const memory::const_block data,
  const span_size_t max_size,
  context& ctx,
  main_ctx_object& user) noexcept -> bool {

    if(const ok md_type{ctx.default_message_digest()}) {
        auto& ssl = ctx.ssl();
        _buffer.resize(max_size);
        const auto used{store_data_with_size(data, storage())};
        if(used) [[likely]] {
            if(ok md_ctx{ssl.new_message_digest()}) {
                const auto cleanup{ssl.delete_message_digest.raii(md_ctx)};

                if(ctx.message_digest_sign_init(md_ctx, md_type)) [[likely]] {
                    if(ssl.message_digest_sign_update(md_ctx, data))
                      [[likely]] {

                        auto free{skip(storage(), used.size())};
                        if(const ok sig{
                             ssl.message_digest_sign_final(md_ctx, free)}) {
                            crypto_flags |= message_crypto_flag::asymmetric;
                            crypto_flags |= message_crypto_flag::signed_content;
                            _buffer.resize(used.size() + sig.get().size());
                            return true;
                        } else {
                            user.log_debug("failed to finish ssl signature")
                              .arg("freeSize", free.size())
                              .arg("reason", (not sig).message());
                        }
                    } else {
                        user.log_debug("failed to update ssl signature");
                    }
                } else {
                    user.log_debug("failed to init ssl sign context");
                }
            } else {
                user.log_debug("failed to create ssl message digest")
                  .arg("reason", (not md_ctx).message());
            }
        } else {
            user.log_debug("not enough space for message signature")
              .arg("maxSize", max_size);
        }
    } else {
        user.log_debug("failed to get ssl message digest type")
          .arg("reason", (not md_type).message());
    }
    copy_into(data, _buffer);
    return true;
}
//------------------------------------------------------------------------------
auto stored_message::verify_bits(context& ctx, main_ctx_object&) const noexcept
  -> verification_bits {
    return ctx.verify_remote_signature(content(), signature(), source_id);
}
//------------------------------------------------------------------------------
// message_storage
//------------------------------------------------------------------------------
auto message_storage::fetch_all(const fetch_handler handler) noexcept -> bool {
    bool fetched_some{false};
    bool clear_all{true};
    for(auto& [msg_id, message, insert_time] : _messages) {
        const auto msg_age{std::chrono::duration_cast<message_age>(
          _clock_t::now() - insert_time)};
        if(handler(msg_id, msg_age, message)) {
            _buffers.eat(message.release_buffer());
            message.mark_too_old();
            fetched_some = true;
        } else {
            clear_all = false;
        }
    }
    if(clear_all) {
        _messages.clear();
    } else {
        std::erase_if(_messages, [](const auto& t) {
            return std::get<1>(t).too_many_hops();
        });
    }
    return fetched_some;
}
//------------------------------------------------------------------------------
void message_storage::cleanup(const cleanup_predicate predicate) noexcept {
    std::erase_if(_messages, [predicate](auto& t) {
        const auto msg_age{std::chrono::duration_cast<message_age>(
          _clock_t::now() - std::get<2>(t))};
        return predicate(msg_age);
    });
}
//------------------------------------------------------------------------------
void message_storage::log_stats(main_ctx_object& user) {
    _buffers.stats().and_then([&](const auto& stats) {
        user.log_stat("message storage buffer pool stats")
          .arg("maxBufSize", stats.max_buffer_size())
          .arg("maxCount", stats.max_buffer_count())
          .arg("poolGets", stats.number_of_gets())
          .arg("poolHits", stats.number_of_hits())
          .arg("poolEats", stats.number_of_eats())
          .arg("poolDscrds", stats.number_of_discards());
    });
}
//------------------------------------------------------------------------------
// serialized_message_storage
//------------------------------------------------------------------------------
auto serialized_message_storage::fetch_all(const fetch_handler handler) noexcept
  -> bool {
    bool fetched_some{false};
    bool clear_all{true};
    for(auto& [message, timestamp, priority] : _messages) {
        if(handler(timestamp, priority, view(message))) [[likely]] {
            _buffers.eat(std::move(message));
            fetched_some = true;
        } else {
            clear_all = false;
        }
    }
    if(clear_all) [[likely]] {
        _messages.clear();
    } else {
        std::erase_if(_messages, [](const auto& entry) {
            return std::get<0>(entry).empty();
        });
    }
    return fetched_some;
}
//------------------------------------------------------------------------------
class message_packing_context {
public:
    using bit_set = message_pack_info::bit_set;

    message_packing_context(const memory::block blk) noexcept
      : _blk{blk}
      , _info{_blk.size()} {}

    auto info() const noexcept -> const message_pack_info& {
        return _info;
    }

    auto dest() noexcept {
        return _blk;
    }

    auto is_full() const noexcept {
        return _current_bit == 0U;
    }

    void add(const span_size_t size, const message_priority priority) noexcept {
        _blk = skip(_blk, size);
        _info.add(size, priority, _current_bit);
    }

    void next() noexcept {
        _current_bit <<= 1U;
    }

    void finalize() noexcept {
        zero(_blk);
    }

private:
    bit_set _current_bit{1U};
    memory::block _blk;
    message_pack_info _info;
};
//------------------------------------------------------------------------------
void serialized_message_storage::push(
  const memory::const_block message,
  const message_priority priority) noexcept {
    assert(not message.empty());
    auto buf = _buffers.get(message.size());
    memory::copy_into(message, buf);
    _messages.emplace_back(std::move(buf), _clock_t::now(), priority);
}
//------------------------------------------------------------------------------
auto serialized_message_storage::pack_into(memory::block dest) noexcept
  -> message_pack_info {
    message_packing_context packing{dest};

    for(const auto& [message, timestamp, priority] : _messages) {
        (void)(timestamp);
        if(packing.is_full()) {
            break;
        }
        if(const auto packed{
             store_data_with_size(view(message), packing.dest())}) [[likely]] {
            packing.add(packed.size(), priority);
        }
        packing.next();
    }
    packing.finalize();

    return packing.info();
}
//------------------------------------------------------------------------------
void serialized_message_storage::cleanup(
  const message_pack_info& packed) noexcept {
    auto to_be_removed{packed.bits()};

    if(to_be_removed) {
        span_size_t i = 0;
        // don't try to "optimize" this into the erase_if predicate
        while(to_be_removed) {
            if((to_be_removed & 1U) == 1U) {
                _buffers.eat(std::move(std::get<0>(_messages[i])));
            }
            ++i;
            to_be_removed >>= 1U;
        }
        std::erase_if(_messages, [](const auto& entry) {
            return std::get<0>(entry).empty();
        });
    }
}
//------------------------------------------------------------------------------
void serialized_message_storage::log_stats(main_ctx_object& user) {
    _buffers.stats().and_then([&](auto stats) {
        user.log_stat("serialized message storage buffer pool stats")
          .arg("maxBufSize", stats.max_buffer_size())
          .arg("maxCount", stats.max_buffer_count())
          .arg("poolGets", stats.number_of_gets())
          .arg("poolHits", stats.number_of_hits())
          .arg("poolEats", stats.number_of_eats())
          .arg("poolDscrds", stats.number_of_discards());
    });
}
//------------------------------------------------------------------------------
// message_priority_queue
//------------------------------------------------------------------------------
auto message_priority_queue::process_all(
  const message_context& msg_ctx,
  const handler_type handler) noexcept -> span_size_t {
    std::size_t result{_messages.size()};
    bool clear_all{true};
    for(auto& message : _messages) {
        if(handler(msg_ctx, message)) {
            _buffers.eat(message.release_buffer());
            message.mark_too_old();
        } else {
            clear_all = false;
        }
    }
    if(clear_all) {
        _messages.clear();
    } else {
        result = std::erase_if(_messages, [](const auto& message) {
            return message.too_many_hops();
        });
    }
    return span_size(result);
}
//------------------------------------------------------------------------------
// connection_outgoing_messages
//------------------------------------------------------------------------------
auto connection_outgoing_messages::enqueue(
  main_ctx_object& user,
  const message_id msg_id,
  const message_view& message,
  memory::block temp) noexcept -> bool {

    block_data_sink sink(temp);
    default_serializer_backend backend(sink);
    if(const auto serialized{serialize_message(msg_id, message, backend)})
      [[likely]] {
        user.log_trace("enqueuing message ${message} to be sent")
          .arg("message", msg_id);
        _serialized.push(sink.done(), message.priority);
        return true;
    } else {
        user.log_error("failed to serialize message ${message}")
          .arg("message", msg_id)
          .arg("errors", get_errors(serialized))
          .arg("content", message.content());
    }
    return false;
}
//------------------------------------------------------------------------------
// connection_incoming_messages
//------------------------------------------------------------------------------
auto connection_incoming_messages::fetch_messages(
  main_ctx_object& user,
  const fetch_handler handler) noexcept -> bool {
    const auto unpacker{[this, &user](
                          const message_timestamp data_ts,
                          const message_priority,
                          const memory::const_block data) {
        for_each_data_with_size(
          data, [this, &user, data_ts](const memory::const_block blk) {
              if(not blk.empty()) [[likely]] {
                  _unpacked.push_if([&user, data_ts, blk](
                                      message_id& msg_id,
                                      message_timestamp& msg_ts,
                                      stored_message& message) {
                      block_data_source source(blk);
                      default_deserializer_backend backend(source);
                      if(const auto deserialized{deserialize_message(
                           msg_id, message, backend)}) [[likely]] {
                          user.log_trace("fetched message ${message}")
                            .arg("message", msg_id);
                          msg_ts = data_ts;
                          return true;
                      } else {
                          user.log_error("failed to deserialize message")
                            .arg("errors", get_errors(deserialized))
                            .arg("block", blk);
                          return false;
                      }
                  });
              }
          });
        return true;
    }};

    if(_packed.fetch_all({construct_from, unpacker})) {
        _unpacked.fetch_all(handler);
    }
    return false;
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
