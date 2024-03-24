/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

module eagine.msgbus.core;

import std;
import eagine.core.debug;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.math;
import eagine.core.utility;
import eagine.core.container;
import eagine.core.identifier;
import eagine.core.serialization;
import eagine.core.valid_if;
import eagine.core.logging;
import eagine.core.main_ctx;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
blob_preparation_result::blob_preparation_result(float progress) noexcept
  : _progress{math::clamp(progress, 0.F, 1.F)}
  , _status{
      progress < 1.F ? blob_preparation_status::working
                     : blob_preparation_status::finished} {}
//------------------------------------------------------------------------------
blob_preparation_result::blob_preparation_result(
  blob_preparation_status status) noexcept
  : _progress{status == blob_preparation_status::working ? 0.F : 1.F}
  , _status{status} {}
//------------------------------------------------------------------------------
blob_preparation_result::blob_preparation_result(
  float progress,
  blob_preparation_status status) noexcept
  : _progress{progress}
  , _status{status} {}
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
      const memory::const_block src,
      const blob_info& info) noexcept -> bool final {
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
class blob_stream_io final : public target_blob_io {
public:
    blob_stream_io(
      blob_id_t blob_id,
      blob_stream_signals& sigs,
      memory::buffer_pool& buffers) noexcept
      : _blob_id{blob_id}
      , _signals{sigs}
      , _buffers{buffers} {}

    auto store_fragment(
      span_size_t offset,
      memory::const_block data,
      const blob_info& info) noexcept -> bool final;

    void handle_prepared(float progress) noexcept final {
        _signals.blob_preparation_progressed(_blob_id, progress);
    }

    void handle_finished(
      const message_id,
      const message_age,
      const message_info&,
      const blob_info&) noexcept final {
        _signals.blob_stream_finished(_blob_id);
    }

    void handle_cancelled() noexcept final {
        _signals.blob_stream_cancelled(_blob_id);
    }

private:
    const identifier_t _blob_id;
    blob_stream_signals& _signals;

    void _append(
      span_size_t offset,
      const memory::span<const memory::const_block> data,
      const blob_info& info) const noexcept {
        _signals.blob_stream_data_appended(
          {.request_id = _blob_id,
           .offset = offset,
           .data = data,
           .info = info});
    }

    void _append_one(
      span_size_t offset,
      memory::const_block data,
      const blob_info& info) const noexcept {
        _append(offset, memory::view_one(data), info);
    }

    memory::buffer_pool& _buffers;
    span_size_t _offs_done{0};
    flat_map<span_size_t, memory::buffer> _unmerged;
    std::vector<memory::buffer> _merged;
    std::vector<memory::const_block> _consecutive;
};
//------------------------------------------------------------------------------
auto blob_stream_io::store_fragment(
  span_size_t offset,
  memory::const_block data,
  const blob_info& info) noexcept -> bool {
    assert(not data.empty());
    assert(offset >= _offs_done);

    if(offset == _offs_done) {
        if(_unmerged.empty()) {
            _append_one(offset, data, info);
            _offs_done = safe_add(offset, data.size());
        } else {
            assert(_consecutive.empty());
            assert(_merged.empty());
            _merged.reserve(_unmerged.size() + 1U);
            _consecutive.push_back(data);

            auto data_end{safe_add(offset, data.size())};
            auto bgn = _unmerged.begin();
            auto end = _unmerged.end();
            auto pos = bgn;

            while(pos != end) {
                auto& [merge_offs, merge_buf] = *pos;
                if(merge_offs != data_end) {
                    break;
                }
                _merged.emplace_back(std::move(merge_buf));
                auto& merge_data = _merged.back();
                data_end = safe_add(merge_offs, merge_data.size());
                _consecutive.push_back(view(merge_data));
                ++pos;
            }
            _unmerged.erase(bgn, pos);

            _offs_done = data_end;
            _append(offset, view(_consecutive), info);
            _consecutive.clear();
            for(auto& buf : _merged) {
                _buffers.eat(std::move(buf));
            }
            _merged.clear();
        }
    } else {
        auto buf{_buffers.get(data.size())};
        memory::copy(data, buf);
        _unmerged[offset] = std::move(buf);
    }

    return true;
}
//------------------------------------------------------------------------------
auto make_target_blob_stream_io(
  blob_id_t blob_id,
  blob_stream_signals& sigs,
  memory::buffer_pool& buffers) -> unique_holder<target_blob_io> {
    return {hold<blob_stream_io>, blob_id, sigs, buffers};
}
//------------------------------------------------------------------------------
// blob_chunk_io
//------------------------------------------------------------------------------
class blob_chunk_io final : public target_blob_io {
public:
    blob_chunk_io(
      identifier_t blob_id,
      span_size_t chunk_size,
      blob_stream_signals& sigs,
      memory::buffer_pool& buffers) noexcept
      : _blob_id{blob_id}
      , _chunk_size{chunk_size}
      , _signals{sigs}
      , _buffers{buffers} {}

    auto store_fragment(
      span_size_t offset,
      memory::const_block data,
      const blob_info& info) noexcept -> bool final;

    void handle_prepared(float progress) noexcept final;

    void handle_finished(
      const message_id,
      const message_age,
      const message_info&,
      const blob_info&) noexcept final;

    void handle_cancelled() noexcept final {
        _recycle_chunks();
        _signals.blob_stream_cancelled(_blob_id);
    }

private:
    const identifier_t _blob_id;
    const span_size_t _chunk_size;
    blob_stream_signals& _signals;

    void _recycle_chunks() {
        for(auto& chunk : _chunks) {
            _buffers.eat(std::move(chunk));
        }
        _chunks.clear();
    }

    memory::buffer_pool& _buffers;
    std::vector<memory::buffer> _chunks;
};
//------------------------------------------------------------------------------
auto blob_chunk_io::store_fragment(
  span_size_t offset,
  memory::const_block data,
  const blob_info& info) noexcept -> bool {
    assert(not data.empty());

    span_size_t chunk_idx{offset / _chunk_size};
    const span_size_t last_chunk{safe_add(offset, data.size()) / _chunk_size};

    span_size_t copy_srco{0};
    auto copy_dsto{offset - (chunk_idx * _chunk_size)};
    auto copy_size{std::min(_chunk_size - copy_dsto, data.size())};
    const auto store{[&, this]() {
        if(copy_size > 0) {
            const auto idx{std_size(chunk_idx)};
            const auto nsz{idx + 1U};
            if(_chunks.size() < nsz) {
                _chunks.resize(nsz);
            }
            auto& chunk = _chunks[idx];
            if(chunk.empty()) {
                chunk = _buffers.get(_chunk_size);
                chunk.clear();
            }
            // the data may come out of order so we don't want resize here
            chunk.ensure(copy_dsto + copy_size);
            memory::copy(
              head(skip(data, copy_srco), copy_size),
              head(skip(cover(chunk), copy_dsto), copy_size));
            copy_srco += copy_size;
        }
        ++chunk_idx;
    }};
    store();
    while(chunk_idx <= last_chunk) {
        copy_dsto = 0;
        copy_size = std::min(_chunk_size, data.size() - copy_srco);
        store();
    }
    return true;
}
//------------------------------------------------------------------------------
void blob_chunk_io::handle_prepared(float progress) noexcept {
    _signals.blob_preparation_progressed(_blob_id, progress);
}
//------------------------------------------------------------------------------
void blob_chunk_io::handle_finished(
  const message_id,
  const message_age,
  const message_info&,
  const blob_info& info) noexcept {
    std::vector<memory::const_block> data;
    data.reserve(_chunks.size());
    auto prev_size{_chunk_size};
    for(const auto& chunk : _chunks) {
        assert(prev_size == _chunk_size);
        data.push_back(view(chunk));
        prev_size = span_size(chunk.size());
    }
    _signals.blob_stream_data_appended(
      {.request_id = _blob_id, .offset = 0, .data = view(data), .info = info});
    _signals.blob_stream_finished(_blob_id);
    _recycle_chunks();
}
//------------------------------------------------------------------------------
auto make_target_blob_chunk_io(
  blob_id_t blob_id,
  span_size_t chunk_size,
  blob_stream_signals& sigs,
  memory::buffer_pool& buffers) -> unique_holder<target_blob_io> {
    return {hold<blob_chunk_io>, blob_id, chunk_size, sigs, buffers};
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
    return info.total_size - result;
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
auto pending_blob::total_size() const noexcept -> span_size_t {
    return info.total_size;
}
//------------------------------------------------------------------------------
auto pending_blob::total_size_mismatch(const span_size_t size) const noexcept
  -> bool {
    return (info.total_size != 0) and (info.total_size != size);
}
//------------------------------------------------------------------------------
void pending_blob::handle_source_preparing(float new_progress) noexcept {
    if(not todo_parts().empty()) {
        linger_time.reset();
        info.total_size = source_io->total_size();
        std::get<1>(todo_parts().back()) = info.total_size;
    }
    prepare_progress = new_progress;
}
//------------------------------------------------------------------------------
auto pending_blob::prepare() noexcept -> blob_preparation_result {
    assert(source_io);
    const auto result{source_io->prepare()};
    if(result.is_working()) {
        handle_source_preparing(result.progress());
    } else {
        if(result.has_failed()) {
            todo_parts().clear();
        }
        prepare_progress = 1.F;
    }
    return result;
}
//------------------------------------------------------------------------------
auto pending_blob::sent_everything() const noexcept -> bool {
    if(not todo_parts().empty()) {
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
        return (bgn == 0) and (info.total_size != 0) and
               (end >= info.total_size);
    }
    return (info.total_size != 0) and done.empty();
}
//------------------------------------------------------------------------------
auto pending_blob::fetch(const span_size_t offs, memory::block dst) noexcept {
    assert(source_io);
    return source_io->fetch_fragment(offs, dst);
}
//------------------------------------------------------------------------------
auto pending_blob::store(
  const span_size_t offs,
  const memory::const_block src) noexcept {
    assert(target_io);
    return target_io->store_fragment(offs, src, info);
}
//------------------------------------------------------------------------------
auto pending_blob::check(
  const span_size_t offs,
  const memory::const_block blk) noexcept {
    assert(target_io);
    return target_io->check_stored(offs, blk);
}
//------------------------------------------------------------------------------
auto pending_blob::age() const noexcept -> message_age {
    return std::chrono::duration_cast<message_age>(max_time.elapsed_time());
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

    for(const auto [src_bgn, src_end] : src) {
        if(bgn < src_bgn) {
            if(end < src_bgn) {
                if(not new_done) {
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
                if(not new_done) {
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
    if(not new_done) {
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
        end = info.total_size;
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
                    if(not new_done) {
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
                    if(not new_done) {
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
        if(not new_done) {
            dst.emplace_back(bgn, end);
        }
    }
}
//------------------------------------------------------------------------------
void pending_blob::handle_target_preparing(float new_progress) noexcept {
    if(prepare_progress < new_progress) {
        previous_progress = prepare_progress;
        prepare_progress = new_progress;
        target_io->handle_prepared(new_progress);
    }
}
//------------------------------------------------------------------------------
// blob manipulator
//------------------------------------------------------------------------------
blob_manipulator::blob_manipulator(
  main_ctx_parent parent,
  message_id fragment_msg_id,
  message_id resend_msg_id,
  message_id prepare_msg_id) noexcept
  : main_ctx_object{"BlobManipl", parent}
  , _fragment_msg_id{std::move(fragment_msg_id)}
  , _resend_msg_id{std::move(resend_msg_id)}
  , _prepare_msg_id{std::move(prepare_msg_id)} {}
//------------------------------------------------------------------------------
auto blob_manipulator::update(
  const blob_manipulator::send_handler do_send,
  const span_size_t max_message_size) noexcept -> work_done {
    static const auto exec_time_id{register_time_interval("blobUpdate")};
    const auto exec_time{measure_time_interval(exec_time_id)};
    const auto now = std::chrono::steady_clock::now();
    some_true something_done{};

    if(const auto erased_count{std::erase_if(
         _outgoing,
         [this, &something_done](auto& pending) {
             if(
               pending.max_time.is_expired() or
               (pending.sent_everything() and
                pending.linger_time.is_expired())) {
                 if(auto buf_io{pending.source_buffer_io()}) {
                     _buffers.eat(buf_io->release_buffer());
                 }
                 something_done();
                 return true;
             }
             return false;
         })};
       erased_count > 0) {
        log_debug("erased ${erased} outgoing blobs")
          .tag("delOutBlob")
          .arg("erased", erased_count)
          .arg("remaining", _outgoing.size());
    }

    if(const auto erased_count{std::erase_if(
         _incoming,
         [this, &something_done](auto& pending) {
             if(pending.max_time.is_expired()) {
                 pending.target_io->handle_cancelled();
                 if(auto buf_io{pending.target_buffer_io()}) {
                     _buffers.eat(buf_io->release_buffer());
                 }
                 something_done();
                 return true;
             }
             return false;
         })};
       erased_count > 0) {
        log_debug("erased ${erased} incoming  blobs")
          .tag("delIncBlob")
          .arg("erased", erased_count)
          .arg("remaining", _incoming.size());
    }

    for(auto& pending : _incoming) {
        auto& done = pending.done_parts();
        if(not done.empty()) {
            if(now - pending.latest_update > std::chrono::milliseconds{250}) {
                const auto [bgn, end] =
                  [&]() -> std::tuple<span_size_t, span_size_t> {
                    const auto max{2 * max_message_size / 3};
                    if(std::get<0>(done[0]) > 0) {
                        return {0, std::min(std::get<0>(done[0]), max)};
                    } else {
                        if(done.size() == 1) {
                            return {
                              std::get<1>(done[0]),
                              std::get<1>(done[0]) + std::min(
                                                       pending.info.total_size -
                                                         std::get<1>(done[0]),
                                                       max)};
                        } else {
                            return {
                              std::get<1>(done[0]),
                              std::get<1>(done[0]) +
                                std::min(
                                  std::get<0>(done[1]) - std::get<1>(done[0]),
                                  max)};
                        }
                    }
                }();

                const std::tuple<identifier_t, std::uint64_t, std::uint64_t>
                  params{pending.source_blob_id, bgn, end};
                auto buffer{default_serialize_buffer_for(params)};
                const auto serialized{default_serialize(params, cover(buffer))};
                assert(serialized);
                message_view resend_request{*serialized};
                resend_request.set_target_id(pending.info.source_id);
                pending.latest_update = now;
                something_done(do_send(_resend_msg_id, resend_request));
            }
        }
    }

    return something_done;
}
//------------------------------------------------------------------------------
auto blob_manipulator::make_target_io(const span_size_t total_size) noexcept
  -> unique_holder<target_blob_io> {
    if(total_size < _max_blob_size) {
        return {hold<buffer_blob_io>, _buffers.get(total_size)};
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
  blob_manipulator&) noexcept -> unique_holder<target_blob_io> {
    return make_target_io(total_size);
}
//------------------------------------------------------------------------------
auto blob_manipulator::expect_incoming(
  const message_id msg_id,
  const endpoint_id_t source_id,
  const blob_id_t target_blob_id,
  shared_holder<target_blob_io> io,
  const std::chrono::seconds max_time) noexcept -> bool {
    log_debug("expecting incoming fragment")
      .arg("source", source_id)
      .arg("tgtBlobId", target_blob_id)
      .arg("timeout", max_time);

    _incoming.emplace_back();
    auto& pending = _incoming.back();
    pending.msg_id = msg_id;
    pending.info.source_id = source_id;
    pending.info.priority = message_priority::normal;
    pending.source_blob_id = 0U;
    pending.target_blob_id = target_blob_id;
    pending.target_io = std::move(io);
    pending.latest_update = std::chrono::steady_clock::now();
    pending.max_time = timeout{max_time};
    return true;
}
//------------------------------------------------------------------------------
auto blob_manipulator::push_incoming_fragment(
  const message_id msg_id,
  const endpoint_id_t source_id,
  const blob_id_t source_blob_id,
  const blob_id_t target_blob_id,
  const std::int64_t offset,
  const std::int64_t total_size,
  target_io_getter get_io,
  const memory::const_block fragment,
  const blob_options options,
  const message_priority priority) noexcept -> bool {

    auto pos = std::find_if(
      _incoming.begin(),
      _incoming.end(),
      [source_id, source_blob_id](const auto& pending) {
          return (pending.info.source_id == source_id) and
                 (pending.source_blob_id == source_blob_id);
      });
    if(pos == _incoming.end()) {
        pos = std::find_if(
          _incoming.begin(),
          _incoming.end(),
          [msg_id, source_id, target_blob_id](const auto& pending) {
              return (pending.msg_id == msg_id) and
                     (pending.target_blob_id == target_blob_id) and
                     ((pending.info.source_id == source_id) or
                      (pending.info.source_id == broadcast_endpoint_id()));
          });
        if(pos != _incoming.end()) {
            auto& pending = *pos;
            if(pending.info.source_id == broadcast_endpoint_id()) {
                pending.info.source_id = source_id;
            }
            pending.source_blob_id = source_blob_id;
            pending.info.priority = priority;
            pending.info.options = options;
            pending.info.total_size = limit_cast<span_size_t>(total_size);
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
        if(not pending.total_size_mismatch(integer(total_size))) [[likely]] {
            if(pending.msg_id == msg_id) [[likely]] {
                pending.max_time.reset();
                pending.info.priority =
                  std::max(pending.info.priority, priority);
                pending.info.total_size = std::max(
                  pending.info.total_size, limit_cast<span_size_t>(total_size));
                if(pending.merge_fragment(integer(offset), fragment)) {
                    log_debug("merged blob fragment (${progress})")
                      .arg("source", source_id)
                      .arg("srcBlobId", source_blob_id)
                      .arg("parts", pending.done_parts().size())
                      .arg("offset", offset)
                      .arg("size", fragment.size())
                      .arg_func([&](logger_backend& backend) {
                          backend.add_float(
                            "progress",
                            "Progress",
                            0.F,
                            float(pending.received_size()),
                            float(pending.total_size()));
                      });
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
              .arg("pending", "ByteSize", pending.info.total_size)
              .arg("message", "ByteSize", total_size);
        }
    } else if(source_id != broadcast_endpoint_id()) {
        if(auto io{get_io(msg_id, integer(total_size), *this)}) {
            _incoming.emplace_back();
            auto& pending = _incoming.back();
            pending.msg_id = msg_id;
            pending.info.source_id = source_id;
            pending.info.total_size = limit_cast<span_size_t>(total_size);
            pending.info.priority = priority;
            pending.info.options = options;
            pending.source_blob_id = source_blob_id;
            pending.target_blob_id = target_blob_id;
            pending.target_io = std::move(io);
            pending.max_time = timeout{adjusted_duration(
              std::chrono::seconds{60}, memory_access_rate::high)};
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
    blob_options_t options{0};

    auto header{std::tie(
      class_id,
      method_id,
      source_blob_id,
      target_blob_id,
      offset,
      total_size,
      options)};
    block_data_source source{message.content()};
    default_deserializer_backend backend(source);
    if(const auto deserialized{deserialize(header, backend)}) [[likely]] {
        const message_id msg_id{class_id, method_id};
        if((offset >= 0) and (offset < total_size)) {
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
                  blob_options{options},
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
          .arg("errors", get_errors(deserialized))
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
        const auto source_blob_id{std::get<0>(params)};
        const auto bgn{limit_cast<span_size_t>(std::get<1>(params))};
        const auto end{limit_cast<span_size_t>(std::get<2>(params))};
        log_debug("received resend request from ${target}")
          .arg("target", message.source_id)
          .arg("srcBlobId", source_blob_id)
          .arg("begin", bgn)
          .arg("end", end);
        const auto pos{std::find_if(
          _outgoing.begin(), _outgoing.end(), [source_blob_id](auto& pending) {
              return pending.source_blob_id == source_blob_id;
          })};
        if(pos != _outgoing.end()) {
            pos->merge_resend_request(bgn, end);
        }
    }
    return true;
}
//------------------------------------------------------------------------------
auto blob_manipulator::process_prepare(const message_view& message) noexcept
  -> bool {
    std::tuple<identifier_t, float> params{};
    if(default_deserialize(params, message.content())) {
        const auto target_blob_id{std::get<0>(params)};
        const auto pos{std::find_if(
          _incoming.begin(), _incoming.end(), [target_blob_id](auto& pending) {
              return pending.target_blob_id == target_blob_id;
          })};
        if(pos != _incoming.end()) {
            pos->handle_target_preparing(std::get<1>(params));
        }
    }
    return true;
}
//------------------------------------------------------------------------------
auto blob_manipulator::cancel_incoming(
  const endpoint_id_t target_blob_id) noexcept -> bool {
    const auto pos{std::find_if(
      _incoming.begin(), _incoming.end(), [target_blob_id](auto& pending) {
          return pending.target_blob_id == target_blob_id;
      })};
    if(pos != _incoming.end()) {
        auto& pending = *pos;
        pending.target_io->handle_cancelled();
        if(auto buf_io{pending.target_buffer_io()}) {
            _buffers.eat(buf_io->release_buffer());
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
    switch(pending.info.priority) {
        case message_priority::critical:
        case message_priority::high:
            return max_message_size - 92;
        case message_priority::normal:
            return max_message_size * 3 / 4;
        case message_priority::low:
            return max_message_size * 2 / 3;
        case message_priority::idle:
            return max_message_size / 2;
    }
    return 0;
}
//------------------------------------------------------------------------------
inline auto blob_manipulator::_scratch_block(const span_size_t size) noexcept
  -> memory::block {
    return cover(_scratch_buffer.resize(size));
}
//------------------------------------------------------------------------------
auto blob_manipulator::_next_blob_id() noexcept -> blob_id_t {
    if(_blob_id_sequence == std::numeric_limits<blob_id_t>::max())
      [[unlikely]] {
        _blob_id_sequence = 0;
    }
    return ++_blob_id_sequence;
}
//------------------------------------------------------------------------------
auto blob_manipulator::push_outgoing(
  const message_id msg_id,
  const endpoint_id_t source_id,
  const endpoint_id_t target_id,
  const blob_id_t target_blob_id,
  shared_holder<source_blob_io> io,
  const std::chrono::seconds max_time,
  const blob_options options,
  const message_priority priority) noexcept -> blob_id_t {
    assert(io);
    if(io->total_size() > 0) {
        _outgoing.emplace_back();
        auto& pending = _outgoing.back();
        pending.msg_id = msg_id;
        pending.info.source_id = source_id;
        pending.info.target_id = target_id;
        pending.info.total_size = io->total_size();
        pending.info.priority = priority;
        pending.info.options = options;
        pending.source_blob_id = _next_blob_id();
        pending.target_blob_id = target_blob_id;
        pending.source_io = std::move(io);
        pending.linger_time.reset();
        pending.max_time = timeout{max_time};
        pending.todo_parts().emplace_back(0, pending.info.total_size);
        return pending.source_blob_id;
    }
    return 0;
}
//------------------------------------------------------------------------------
auto blob_manipulator::push_outgoing(
  const message_id msg_id,
  const endpoint_id_t source_id,
  const endpoint_id_t target_id,
  const blob_id_t target_blob_id,
  const memory::const_block src,
  const std::chrono::seconds max_time,
  const blob_options options,
  const message_priority priority) noexcept -> blob_id_t {
    return push_outgoing(
      msg_id,
      source_id,
      target_id,
      target_blob_id,
      {hold<buffer_blob_io>, _buffers.get(src.size()), src},
      max_time,
      options,
      priority);
}
//------------------------------------------------------------------------------
auto blob_manipulator::_process_preparing_outgoing(
  const send_handler do_send,
  const span_size_t max_message_size,
  pending_blob& pending) noexcept -> work_done {
    if(
      (pending.prepare_progress >= 1.F) or
      (pending.prepare_progress - pending.previous_progress >= 0.001F)) {
        pending.previous_progress = pending.prepare_progress;
        const std::tuple<identifier_t, float> params{
          pending.target_blob_id, pending.prepare_progress};
        auto buffer{default_serialize_buffer_for(params)};
        const auto serialized{default_serialize(params, cover(buffer))};
        assert(serialized);
        message_view message(*serialized);
        message.set_source_id(pending.info.source_id);
        message.set_target_id(pending.info.target_id);
        message.set_priority(message_priority::normal);
        return {do_send(_prepare_msg_id, message)};
    }
    return {false};
}
//------------------------------------------------------------------------------
auto blob_manipulator::_process_finished_outgoing(
  const send_handler do_send,
  const span_size_t max_message_size,
  pending_blob& pending) noexcept -> work_done {
    some_true something_done{};
    auto& [bgn, end] = pending.todo_parts().back();
    assert(end != 0);

    const auto header{std::make_tuple(
      pending.msg_id.class_(),
      pending.msg_id.method(),
      pending.source_blob_id,
      pending.target_blob_id,
      limit_cast<std::int64_t>(bgn),
      limit_cast<std::int64_t>(pending.info.total_size),
      static_cast<blob_options_t>(pending.info.options))};

    block_data_sink sink(
      _scratch_block(_message_size(pending, max_message_size)));
    default_serializer_backend backend(sink);

    if(const auto serialized{serialize(header, backend)}) {
        const auto offset = bgn;
        if(auto written_size{pending.fetch(offset, sink.free())}) {

            sink.mark_used(written_size);
            bgn += written_size;
            if(bgn >= end) {
                pending.todo_parts().pop_back();
            }
            message_view message(sink.done());
            message.set_source_id(pending.info.source_id);
            message.set_target_id(pending.info.target_id);
            message.set_priority(pending.info.priority);
            something_done(do_send(_fragment_msg_id, message));

            log_debug("sent blob fragment (${progress})")
              .arg("source", pending.info.source_id)
              .arg("srcBlobId", pending.source_blob_id)
              .arg("parts", pending.todo_parts().size())
              .arg("offset", offset)
              .arg("size", "ByteSize", written_size)
              .arg_func([&](logger_backend& backend) {
                  backend.add_float(
                    "progress",
                    "Progress",
                    0.F,
                    float(pending.sent_size()),
                    float(pending.total_size()));
              });
        } else {
            log_error("failed to write fragment of blob ${message}")
              .arg("message", pending.msg_id);
        }
    } else {
        log_error("failed to serialize header of blob ${message}")
          .arg("errors", get_errors(serialized))
          .arg("message", pending.msg_id);
    }
    pending.linger_time.reset();
    return something_done;
}
//------------------------------------------------------------------------------
auto blob_manipulator::process_outgoing(
  const send_handler do_send,
  const span_size_t max_message_size,
  span_size_t max_messages) noexcept -> work_done {
    some_true something_done{};

    max_messages = std::min(max_messages, span_size(_outgoing.size()));
    while(max_messages-- > 0) {
        auto& pending{_outgoing[_outgoing_index++ % _outgoing.size()]};
        const auto preparation{pending.prepare()};
        if(preparation.has_finished() and not pending.sent_everything()) {
            something_done(
              _process_finished_outgoing(do_send, max_message_size, pending));
        } else if(preparation.is_working()) {
            something_done(
              _process_preparing_outgoing(do_send, max_message_size, pending));
        }
    }

    return something_done;
}
//------------------------------------------------------------------------------
auto blob_manipulator::handle_complete() noexcept -> span_size_t {

    const auto predicate{[this](auto& pending) {
        if(pending.received_everything()) {
            log_debug("handling complete blob ${id}")
              .arg("source", pending.info.source_id)
              .arg("srcBlobId", pending.source_blob_id)
              .arg("message", pending.msg_id)
              .arg("size", "ByteSize", pending.info.total_size);

            message_info info{};
            info.set_source_id(pending.info.source_id);
            info.set_target_id(pending.info.target_id);
            info.set_sequence_no(pending.target_blob_id);
            info.set_priority(pending.info.priority);
            pending.target_io->handle_finished(
              pending.msg_id, pending.age(), info, pending.info);
            return true;
        }
        return false;
    }};

    return span_size(std::erase_if(_incoming, predicate));
}
//------------------------------------------------------------------------------
auto blob_manipulator::fetch_all(
  const blob_manipulator::fetch_handler handle_fetch) noexcept -> span_size_t {

    const auto predicate{[this, &handle_fetch](auto& pending) {
        if(pending.received_everything()) {
            log_debug("fetching complete blob ${id}")
              .arg("source", pending.info.source_id)
              .arg("srcBlobId", pending.source_blob_id)
              .arg("message", pending.msg_id)
              .arg("size", "ByteSize", pending.info.total_size);

            if(auto buf_io{pending.target_buffer_io()}) {
                auto blob = buf_io->release_buffer();
                message_view message{view(blob)};
                message.set_source_id(pending.info.source_id);
                message.set_target_id(pending.info.target_id);
                message.set_sequence_no(pending.target_blob_id);
                message.set_priority(pending.info.priority);
                handle_fetch(pending.msg_id, pending.age(), message);
                _buffers.eat(std::move(blob));
            } else {
                message_info info{};
                info.set_source_id(pending.info.source_id);
                info.set_target_id(pending.info.target_id);
                info.set_sequence_no(pending.target_blob_id);
                info.set_priority(pending.info.priority);
                pending.target_io->handle_finished(
                  pending.msg_id, pending.age(), info, pending.info);
            }
            return true;
        }
        return false;
    }};

    return span_size(std::erase_if(_incoming, predicate));
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
