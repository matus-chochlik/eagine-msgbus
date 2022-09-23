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

import eagine.core.types;
import eagine.core.debug;
import eagine.core.memory;
import eagine.core.utility;
import eagine.core.identifier;
import eagine.core.serialization;
import eagine.core.valid_if;
import <array>;
import <chrono>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// buffer blob I/O
//------------------------------------------------------------------------------
class buffer_blob_io final
  : public source_blob_io
  , public target_blob_io {
public:
    buffer_blob_io(memory::buffer buf) noexcept
      : _buf{std::move(buf)} {
        zero(cover(_buf));
    }

    buffer_blob_io(memory::buffer buf, memory::const_block src) noexcept
      : _buf{std::move(buf)} {
        copy(src, cover(_buf));
    }

    auto is_at_eod(const span_size_t offs) noexcept -> bool final {
        return offs >= _buf.size();
    }

    auto total_size() noexcept -> span_size_t final {
        return _buf.size();
    }

    auto fetch_fragment(const span_size_t offs, memory::block dst) noexcept
      -> span_size_t final {
        const auto src = head(skip(view(_buf), offs), dst.size());
        copy(src, dst);
        return src.size();
    }

    auto store_fragment(
      const span_size_t offs,
      const memory::const_block src) noexcept -> bool final {
        auto dst = skip(cover(_buf), offs);
        if(src.size() <= dst.size()) [[likely]] {
            copy(src, dst);
            return true;
        }
        return false;
    }

    auto check_stored(
      const span_size_t offs,
      const memory::const_block blk) noexcept -> bool final {
        return are_equal(head(skip(view(_buf), offs), blk.size()), blk);
    }

    auto release_buffer() noexcept -> memory::buffer {
        return std::move(_buf);
    }

private:
    memory::buffer _buf;
};
//------------------------------------------------------------------------------
// blob_stream_io
//------------------------------------------------------------------------------
class blob_stream_io : public target_blob_io {
public:
    blob_stream_io(
      identifier_t blob_id,
      blob_stream_signals& sigs,
      memory::buffer_pool& buffers) noexcept
      : _blob_id{blob_id}
      , signals{sigs}
      , _buffers{buffers} {}

    auto store_fragment(span_size_t offset, memory::const_block data) noexcept
      -> bool final;

    void handle_finished(
      const message_id,
      const message_age,
      const message_info&) noexcept final {
        signals.blob_stream_finished(_blob_id);
    }

    void handle_cancelled() noexcept final {
        signals.blob_stream_cancelled(_blob_id);
    }

private:
    const identifier_t _blob_id;
    blob_stream_signals& signals;

    void _append(
      span_size_t offset,
      const memory::span<const memory::const_block> data) const noexcept {
        signals.blob_stream_data_appended(_blob_id, offset, data);
    }

    void _append_one(span_size_t offset, memory::const_block data)
      const noexcept {
        _append(offset, memory::view_one(data));
    }

    memory::buffer_pool& _buffers;
    span_size_t _offs_done{0};
    std::vector<std::tuple<span_size_t, memory::buffer>> _unmerged;
    std::vector<memory::buffer> _merged;
    std::vector<memory::const_block> _consecutive;
};
//------------------------------------------------------------------------------
auto make_blob_stream_io(
  identifier_t blob_id,
  blob_stream_signals& sigs,
  memory::buffer_pool& buffers) -> std::unique_ptr<target_blob_io> {
    return std::make_unique<blob_stream_io>(blob_id, sigs, buffers);
}
//------------------------------------------------------------------------------
auto blob_stream_io::store_fragment(
  span_size_t offset,
  memory::const_block data) noexcept -> bool {
    assert(!data.empty());
    assert(offset >= _offs_done);

    if(offset == _offs_done) {
        if(_unmerged.empty()) {
            _append_one(offset, data);
            _offs_done = safe_add(offset + data.size());
        } else {
            assert(_merged.empty());
            _merged.reserve(_unmerged.size());
            assert(_consecutive.empty());
            _consecutive.push_back(data);

            auto data_end{safe_add(offset, data.size())};
            while(!_unmerged.empty()) {
                auto& [merge_offs, merge_buf] = _unmerged.back();

                assert(data_end <= merge_offs);
                if(data_end == merge_offs) {
                    _merged.emplace_back(std::move(merge_buf));
                    auto& merge_data = _merged.back();
                    _unmerged.pop_back();
                    data_end = safe_add(merge_offs, merge_data.size());
                    _consecutive.push_back(view(merge_data));
                } else {
                    break;
                }
            }

            _offs_done = data_end;
            _append(offset, view(_consecutive));
            _consecutive.clear();
            for(auto& buf : _merged) {
                _buffers.eat(std::move(buf));
            }
            _merged.clear();
        }
    } else {
        auto buf{_buffers.get(data.size())};
        memory::copy(data, buf);
        if(_unmerged.empty()) {
            _unmerged.emplace_back(offset, std::move(buf));
        } else {
            const auto rpos{std::find_if(
              _unmerged.rbegin(), _unmerged.rend(), [=](const auto& entry) {
                  return std::get<0>(entry) >= offset;
              })};
            _unmerged.emplace(rpos.base(), offset, std::move(buf));
        }
    }
    return true;
}
//------------------------------------------------------------------------------
// pending blob
//------------------------------------------------------------------------------
auto pending_blob::source_buffer_io() noexcept -> buffer_blob_io* {
    return dynamic_cast<buffer_blob_io*>(source_io.get());
}
//------------------------------------------------------------------------------
auto pending_blob::target_buffer_io() noexcept -> buffer_blob_io* {
    return dynamic_cast<buffer_blob_io*>(target_io.get());
}
//------------------------------------------------------------------------------
auto pending_blob::sent_size() const noexcept -> span_size_t {
    span_size_t result = 0;
    for(const auto& [bgn, end] : todo_parts()) {
        result += end - bgn;
    }
    return total_size - result;
}
//------------------------------------------------------------------------------
auto pending_blob::received_size() const noexcept -> span_size_t {
    span_size_t result = 0;
    for(const auto& [bgn, end] : done_parts()) {
        result += end - bgn;
    }
    return result;
}
//------------------------------------------------------------------------------
auto pending_blob::total_size_mismatch(const span_size_t size) const noexcept
  -> bool {
    return (total_size != 0) && (total_size != size);
}
//------------------------------------------------------------------------------
auto pending_blob::sent_everything() const noexcept -> bool {
    if(!todo_parts().empty()) {
        assert(source_io);
        return source_io->is_at_eod(std::get<0>(todo_parts().front()));
    }
    return true;
}
//------------------------------------------------------------------------------
auto pending_blob::received_everything() const noexcept -> bool {
    auto& done = done_parts();
    if(done.size() == 1) {
        const auto [bgn, end] = done.front();
        return (bgn == 0) && (total_size != 0) && (end >= total_size);
    }
    return (total_size != 0) && done.empty();
}
//------------------------------------------------------------------------------
auto pending_blob::merge_fragment(
  const span_size_t bgn,
  const memory::const_block fragment) noexcept -> bool {
    const auto end = bgn + fragment.size();
    fragment_parts.swap();
    auto& dst = done_parts();
    const auto& src = todo_parts();
    dst.clear();

    bool result = true;
    bool new_done = false;

    for(const auto& [src_bgn, src_end] : src) {
        if(bgn < src_bgn) {
            if(end < src_bgn) {
                if(!new_done) {
                    dst.emplace_back(bgn, end);
                    result &= store(bgn, fragment);
                    new_done = true;
                }
                dst.emplace_back(src_bgn, src_end);
            } else if(end <= src_end) {
                if(new_done) {
                    std::get<1>(dst.back()) = src_end;
                } else {
                    dst.emplace_back(bgn, src_end);
                    result &= store(bgn, head(fragment, src_bgn - bgn));
                    new_done = true;
                }
                result &= check(src_bgn, skip(fragment, src_bgn - bgn));
            } else {
                if(!new_done) {
                    dst.emplace_back(bgn, end);
                    result &= store(bgn, head(fragment, src_bgn - bgn));
                    result &= store(src_end, skip(fragment, src_end - bgn));
                    new_done = true;
                }
                result &= check(
                  src_bgn,
                  head(skip(fragment, src_bgn - bgn), src_end - src_bgn));
            }
        } else if(bgn <= src_end) {
            if(end <= src_end) {
                dst.emplace_back(src_bgn, src_end);
                new_done = true;
                result &= check(bgn, fragment);
            } else {
                dst.emplace_back(src_bgn, end);
                result &= store(src_end, skip(fragment, src_end - bgn));
                result &= check(bgn, head(fragment, src_end - bgn));
                new_done = true;
            }
        } else {
            dst.emplace_back(src_bgn, src_end);
        }
    }
    if(!new_done) {
        dst.emplace_back(bgn, end);
        result &= store(bgn, fragment);
    }
    latest_update = std::chrono::steady_clock::now();

    return result;
}
//------------------------------------------------------------------------------
void pending_blob::merge_resend_request(
  const span_size_t bgn,
  span_size_t end) noexcept {
    if(end == 0) {
        end = total_size;
    }
    if(bgn < end) {
        fragment_parts.swap();
        auto& dst = todo_parts();
        const auto& src = done_parts();
        dst.clear();

        bool new_done = false;

        for(const auto& [src_bgn, src_end] : src) {
            if(bgn < src_bgn) {
                if(end < src_bgn) {
                    if(!new_done) {
                        dst.emplace_back(bgn, end);
                        new_done = true;
                    }
                    dst.emplace_back(src_bgn, src_end);
                } else if(end <= src_end) {
                    if(new_done) {
                        std::get<1>(dst.back()) = src_end;
                    } else {
                        dst.emplace_back(bgn, src_end);
                        new_done = true;
                    }
                } else {
                    if(!new_done) {
                        dst.emplace_back(bgn, end);
                        new_done = true;
                    }
                }
            } else if(bgn <= src_end) {
                if(end <= src_end) {
                    dst.emplace_back(src_bgn, src_end);
                    new_done = true;
                } else {
                    dst.emplace_back(src_bgn, end);
                    new_done = true;
                }
            } else {
                dst.emplace_back(src_bgn, src_end);
            }
        }
        if(!new_done) {
            dst.emplace_back(bgn, end);
        }
    }
}
//------------------------------------------------------------------------------
// blob manipulator
//------------------------------------------------------------------------------
auto blob_manipulator::update(
  const blob_manipulator::send_handler do_send) noexcept -> work_done {
    const auto exec_time{measure_time_interval("blobUpdate")};
    const auto now = std::chrono::steady_clock::now();
    some_true something_done{};

    std::erase_if(
      _incoming, [this, now, do_send, &something_done](auto& pending) {
          bool should_erase = false;
          if(pending.max_time.is_expired()) {
              extract(pending.target_io).handle_cancelled();
              if(auto buf_io{pending.target_buffer_io()}) {
                  _buffers.eat(extract(buf_io).release_buffer());
              }
              something_done();
              should_erase = true;
          } else if(now - pending.latest_update > std::chrono::seconds{2}) {
              auto& done = pending.done_parts();
              if(!done.empty()) {
                  const auto bgn = std::get<1>(done[0]);
                  const auto end =
                    done.size() > 1 ? std::get<1>(done[0]) : pending.total_size;
                  const std::tuple<identifier_t, std::uint64_t, std::uint64_t>
                    params{pending.source_blob_id, bgn, end};
                  auto buffer{default_serialize_buffer_for(params)};
                  auto serialized{default_serialize(params, cover(buffer))};
                  assert(serialized);
                  message_view resend_request{extract(serialized)};
                  resend_request.set_target_id(pending.source_id);
                  pending.latest_update = now;
                  something_done(do_send(_resend_msg_id, resend_request));
              }
          }
          return should_erase;
      });

    std::erase_if(_outgoing, [this, &something_done](auto& pending) {
        if(pending.max_time.is_expired()) {
            if(auto buf_io{pending.source_buffer_io()}) {
                _buffers.eat(extract(buf_io).release_buffer());
            }
            something_done();
            return true;
        }
        return false;
    });

    return something_done;
}
//------------------------------------------------------------------------------
auto blob_manipulator::make_target_io(const span_size_t total_size) noexcept
  -> std::unique_ptr<target_blob_io> {
    if(total_size < _max_blob_size) {
        return std::make_unique<buffer_blob_io>(_buffers.get(total_size));
    }
    log_warning("blob is too big ${total_size}")
      .arg("total", "ByteSize", total_size)
      .arg("offset", _max_blob_size);
    return {};
}
//------------------------------------------------------------------------------
auto blob_manipulator::_make_target_io(
  const message_id,
  const span_size_t total_size,
  blob_manipulator&) noexcept -> std::unique_ptr<target_blob_io> {
    return make_target_io(total_size);
}
//------------------------------------------------------------------------------
auto blob_manipulator::expect_incoming(
  const message_id msg_id,
  const identifier_t source_id,
  const blob_id_t target_blob_id,
  std::shared_ptr<target_blob_io> io,
  const std::chrono::seconds max_time) noexcept -> bool {
    log_debug("expecting incoming fragment")
      .arg("source", source_id)
      .arg("tgtBlobId", target_blob_id)
      .arg("timeout", max_time);

    _incoming.emplace_back();
    auto& pending = _incoming.back();
    pending.msg_id = msg_id;
    pending.source_id = source_id;
    pending.source_blob_id = 0U;
    pending.target_blob_id = target_blob_id;
    pending.target_io = std::move(io);
    pending.latest_update = std::chrono::steady_clock::now();
    pending.max_time = timeout{max_time};
    pending.priority = message_priority::normal;
    return true;
}
//------------------------------------------------------------------------------
auto blob_manipulator::push_incoming_fragment(
  const message_id msg_id,
  const identifier_t source_id,
  const blob_id_t source_blob_id,
  const blob_id_t target_blob_id,
  const std::int64_t offset,
  const std::int64_t total_size,
  target_io_getter get_io,
  const memory::const_block fragment,
  const message_priority priority) noexcept -> bool {

    auto pos = std::find_if(
      _incoming.begin(),
      _incoming.end(),
      [source_id, source_blob_id](const auto& pending) {
          return (pending.source_id == source_id) &&
                 (pending.source_blob_id == source_blob_id);
      });
    if(pos == _incoming.end()) {
        pos = std::find_if(
          _incoming.begin(),
          _incoming.end(),
          [msg_id, source_id, target_blob_id](const auto& pending) {
              return (pending.msg_id == msg_id) &&
                     (pending.target_blob_id == target_blob_id) &&
                     ((pending.source_id == source_id) ||
                      (pending.source_id == broadcast_endpoint_id()));
          });
        if(pos != _incoming.end()) {
            auto& pending = *pos;
            if(pending.source_id == broadcast_endpoint_id()) {
                pending.source_id = source_id;
            }
            pending.source_blob_id = source_blob_id;
            pending.priority = priority;
            pending.total_size = limit_cast<span_size_t>(total_size);
            log_debug("updating expected blob fragment")
              .arg("source", source_id)
              .arg("srcBlobId", source_blob_id)
              .arg("tgtBlobId", target_blob_id)
              .arg("total", "ByteSize", total_size)
              .arg("size", "ByteSize", fragment.size());
        }
    }
    if(pos != _incoming.end()) {
        auto& pending = *pos;
        if(!pending.total_size_mismatch(integer(total_size))) [[likely]] {
            if(pending.msg_id == msg_id) [[likely]] {
                pending.max_time.reset();
                if(pending.priority < priority) [[unlikely]] {
                    pending.priority = priority;
                }
                if(pending.merge_fragment(integer(offset), fragment)) {
                    log_debug("merged blob fragment")
                      .arg("source", source_id)
                      .arg("srcBlobId", source_blob_id)
                      .arg("parts", pending.done_parts().size())
                      .arg("offset", offset)
                      .arg("size", fragment.size());
                } else {
                    log_warning("failed to merge blob fragment")
                      .arg("offset", offset)
                      .arg("size", fragment.size());
                }
            } else {
                log_debug("message id mismatch in blob fragment message")
                  .arg("pending", pending.msg_id)
                  .arg("message", msg_id);
            }
        } else {
            log_debug("total size mismatch in blob fragment message")
              .arg("pending", "ByteSize", pending.total_size)
              .arg("message", "ByteSize", total_size);
        }
    } else if(source_id != broadcast_endpoint_id()) {
        if(auto io{get_io(msg_id, integer(total_size), *this)}) {
            _incoming.emplace_back();
            auto& pending = _incoming.back();
            pending.msg_id = msg_id;
            pending.source_id = source_id;
            pending.source_blob_id = source_blob_id;
            pending.target_blob_id = target_blob_id;
            pending.target_io = std::move(io);
            pending.total_size = limit_cast<span_size_t>(total_size);
            pending.max_time = timeout{adjusted_duration(
              std::chrono::seconds{60}, memory_access_rate::high)};
            pending.priority = priority;
            pending.done_parts().clear();
            if(pending.merge_fragment(integer(offset), fragment)) {
                log_debug("merged first blob fragment")
                  .arg("source", source_id)
                  .arg("srcBlobId", source_blob_id)
                  .arg("tgtBlobId", target_blob_id)
                  .arg("parts", pending.done_parts().size())
                  .arg("offset", offset)
                  .arg("size", fragment.size());
            }
        } else {
            log_warning("failed to create blob I/O object")
              .arg("source", source_id)
              .arg("srcBlobId", source_blob_id)
              .arg("tgtBlobId", target_blob_id)
              .arg("offset", offset)
              .arg("size", fragment.size());
        }
    }
    return true;
}
//------------------------------------------------------------------------------
auto blob_manipulator::process_incoming(
  blob_manipulator::target_io_getter get_io,
  const message_view& message) noexcept -> bool {

    identifier class_id{};
    identifier method_id{};
    blob_id_t source_blob_id{0U};
    blob_id_t target_blob_id{0U};
    std::int64_t offset{0};
    std::int64_t total_size{0};

    auto header = std::tie(
      class_id, method_id, source_blob_id, target_blob_id, offset, total_size);
    block_data_source source{message.content()};
    default_deserializer_backend backend(source);
    auto errors = deserialize(header, backend);
    const message_id msg_id{class_id, method_id};
    if(!errors) [[likely]] {
        if((offset >= 0) && (offset < total_size)) {
            const auto fragment = source.remaining();
            const auto max_frag_size = span_size(total_size - offset);
            if(fragment.size() <= max_frag_size) [[likely]] {
                return push_incoming_fragment(
                  msg_id,
                  message.source_id,
                  source_blob_id,
                  target_blob_id,
                  offset,
                  total_size,
                  get_io,
                  fragment,
                  message.priority);
            } else {
                log_error("invalid blob fragment size ${size}")
                  .arg("size", fragment.size())
                  .arg("offset", offset)
                  .arg("total", "ByteSize", total_size);
            }
        } else {
            log_error("invalid blob fragment offset ${offset}")
              .arg("offset", offset)
              .arg("total", "ByteSize", total_size);
        }
    } else {
        log_error("failed to deserialize header of blob")
          .arg("errors", errors)
          .arg("data", message.content());
    }
    return false;
}
//------------------------------------------------------------------------------
auto blob_manipulator::process_incoming(const message_view& message) noexcept
  -> bool {
    return process_incoming(
      make_callable_ref<&blob_manipulator::_make_target_io>(this), message);
}
//------------------------------------------------------------------------------
auto blob_manipulator::process_resend(const message_view& message) noexcept
  -> bool {
    std::tuple<identifier_t, std::uint64_t, std::uint64_t> params{};
    if(default_deserialize(params, message.content())) {
        const auto source_blob_id = std::get<0>(params);
        const auto bgn = limit_cast<span_size_t>(std::get<1>(params));
        const auto end = limit_cast<span_size_t>(std::get<2>(params));
        log_debug("received resend request from ${target}")
          .arg("target", message.source_id)
          .arg("srcBlobId", source_blob_id)
          .arg("begin", bgn)
          .arg("end", end);
        const auto pos = std::find_if(
          _outgoing.begin(), _outgoing.end(), [source_blob_id](auto& pending) {
              return pending.source_blob_id == source_blob_id;
          });
        if(pos != _outgoing.end()) {
            pos->merge_resend_request(bgn, end);
        }
    }
    return true;
}
//------------------------------------------------------------------------------
auto blob_manipulator::cancel_incoming(
  const identifier_t target_blob_id) noexcept -> bool {
    const auto pos = std::find_if(
      _incoming.begin(), _incoming.end(), [target_blob_id](auto& pending) {
          return pending.target_blob_id == target_blob_id;
      });
    if(pos != _incoming.end()) {
        auto& pending = *pos;
        extract(pending.target_io).handle_cancelled();
        if(auto buf_io{pending.target_buffer_io()}) {
            _buffers.eat(extract(buf_io).release_buffer());
        }
        _incoming.erase(pos);
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
auto blob_manipulator::_message_size(
  const pending_blob& pending,
  const span_size_t max_message_size) const noexcept -> span_size_t {
    switch(pending.priority) {
        case message_priority::critical:
            return max_message_size - 92;
        case message_priority::high:
            return max_message_size / 2 - 64;
        case message_priority::normal:
            return max_message_size / 3 - 48;
        case message_priority::low:
            return max_message_size / 4 - 32;
        case message_priority::idle:
            return max_message_size / 8 - 12;
    }
    return 0;
}
//------------------------------------------------------------------------------
inline auto blob_manipulator::_scratch_block(const span_size_t size) noexcept
  -> memory::block {
    return cover(_scratch_buffer.resize(size));
}
//------------------------------------------------------------------------------
auto blob_manipulator::push_outgoing(
  const message_id msg_id,
  const identifier_t source_id,
  const identifier_t target_id,
  const blob_id_t target_blob_id,
  std::shared_ptr<source_blob_io> io,
  const std::chrono::seconds max_time,
  const message_priority priority) noexcept -> blob_id_t {
    assert(io);
    _outgoing.emplace_back();
    auto& pending = _outgoing.back();
    pending.msg_id = msg_id;
    pending.source_id = source_id;
    pending.target_id = target_id;
    pending.source_blob_id = ++_blob_id_sequence;
    pending.target_blob_id = target_blob_id;
    pending.total_size = extract(io).total_size();
    pending.source_io = std::move(io);
    pending.max_time = timeout{max_time};
    pending.priority = priority;
    pending.todo_parts().emplace_back(0, pending.total_size);
    return pending.source_blob_id;
}
//------------------------------------------------------------------------------
auto blob_manipulator::push_outgoing(
  const message_id msg_id,
  const identifier_t source_id,
  const identifier_t target_id,
  const blob_id_t target_blob_id,
  const memory::const_block src,
  const std::chrono::seconds max_time,
  const message_priority priority) noexcept -> blob_id_t {
    return push_outgoing(
      msg_id,
      source_id,
      target_id,
      target_blob_id,
      std::make_unique<buffer_blob_io>(_buffers.get(src.size()), src),
      max_time,
      priority);
}
//------------------------------------------------------------------------------
auto blob_manipulator::process_outgoing(
  const send_handler do_send,
  const span_size_t max_message_size) noexcept -> work_done {
    some_true something_done{};

    for(auto& pending : _outgoing) {
        if(!pending.sent_everything()) {
            auto& [bgn, end] = pending.todo_parts().back();
            assert(end != 0);

            const auto header = std::make_tuple(
              pending.msg_id.class_(),
              pending.msg_id.method(),
              pending.source_blob_id,
              pending.target_blob_id,
              limit_cast<std::int64_t>(bgn),
              limit_cast<std::int64_t>(pending.total_size));

            block_data_sink sink(
              _scratch_block(_message_size(pending, max_message_size)));
            default_serializer_backend backend(sink);

            const auto errors = serialize(header, backend);
            if(!errors) {

                const auto offset = bgn;
                if(auto written_size{pending.fetch(offset, sink.free())}) {

                    sink.mark_used(written_size);
                    bgn += written_size;
                    if(bgn >= end) {
                        pending.todo_parts().pop_back();
                    }
                    message_view message(sink.done());
                    message.set_source_id(pending.source_id);
                    message.set_target_id(pending.target_id);
                    message.set_priority(pending.priority);
                    something_done(do_send(_fragment_msg_id, message));

                    log_debug("sent blob fragment")
                      .arg("source", pending.source_id)
                      .arg("srcBlobId", pending.source_blob_id)
                      .arg("parts", pending.todo_parts().size())
                      .arg("offset", offset)
                      .arg("size", "ByteSize", written_size);
                } else {
                    log_error("failed to write fragment of blob ${message}")
                      .arg("message", pending.msg_id);
                }
            } else {
                log_error("failed to serialize header of blob ${message}")
                  .arg("errors", errors)
                  .arg("message", pending.msg_id);
            }
        }
    }

    return something_done;
}
//------------------------------------------------------------------------------
auto blob_manipulator::handle_complete() noexcept -> span_size_t {

    auto predicate = [this](auto& pending) {
        if(pending.received_everything()) {
            log_debug("handling complete blob ${id}")
              .arg("source", pending.source_id)
              .arg("srcBlobId", pending.source_blob_id)
              .arg("message", pending.msg_id)
              .arg("size", "ByteSize", pending.total_size);

            message_info info{};
            info.set_source_id(pending.source_id);
            info.set_target_id(pending.target_id);
            info.set_sequence_no(pending.target_blob_id);
            info.set_priority(pending.priority);
            extract(pending.target_io)
              .handle_finished(pending.msg_id, pending.age(), info);
            return true;
        }
        return false;
    };

    return span_size(std::erase_if(_incoming, predicate));
}
//------------------------------------------------------------------------------
auto blob_manipulator::fetch_all(
  const blob_manipulator::fetch_handler handle_fetch) noexcept -> span_size_t {

    auto predicate = [this, &handle_fetch](auto& pending) {
        if(pending.received_everything()) {
            log_debug("fetching complete blob ${id}")
              .arg("source", pending.source_id)
              .arg("srcBlobId", pending.source_blob_id)
              .arg("message", pending.msg_id)
              .arg("size", "ByteSize", pending.total_size);

            if(auto buf_io{pending.target_buffer_io()}) {
                auto blob = extract(buf_io).release_buffer();
                message_view message{view(blob)};
                message.set_source_id(pending.source_id);
                message.set_target_id(pending.target_id);
                message.set_sequence_no(pending.target_blob_id);
                message.set_priority(pending.priority);
                handle_fetch(pending.msg_id, pending.age(), message);
                _buffers.eat(std::move(blob));
            } else {
                message_info info{};
                info.set_source_id(pending.source_id);
                info.set_target_id(pending.target_id);
                info.set_sequence_no(pending.target_blob_id);
                info.set_priority(pending.priority);
                extract(pending.target_io)
                  .handle_finished(pending.msg_id, pending.age(), info);
            }
            return true;
        }
        return false;
    };

    return span_size(std::erase_if(_incoming, predicate));
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
