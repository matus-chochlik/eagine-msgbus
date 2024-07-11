/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus.core:blobs;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.valid_if;
import eagine.core.utility;
import eagine.core.main_ctx;
import :types;
import :message;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
using blob_options_t = std::underlying_type_t<blob_option>;
//------------------------------------------------------------------------------
export struct blob_info {
    endpoint_id_t source_id{};
    endpoint_id_t target_id{};
    span_size_t total_size{0};
    blob_options options;
    message_priority priority{message_priority::normal};
};
//------------------------------------------------------------------------------
export enum class blob_preparation_status : std::uint8_t {
    finished,
    working,
    failed
};

export class blob_preparation_result {
public:
    blob_preparation_result(float progress) noexcept;
    blob_preparation_result(
      float progress,
      blob_preparation_status status) noexcept;

    blob_preparation_result(
      std::integral auto cur,
      std::integral auto max,
      blob_preparation_status status) noexcept
      : blob_preparation_result{float(cur) / float(max), status} {}

    blob_preparation_result(
      std::integral auto cur,
      std::integral auto max) noexcept
      : blob_preparation_result{float(cur) / float(max)} {}

    blob_preparation_result(blob_preparation_status status) noexcept;

    static auto finished() noexcept -> blob_preparation_result {
        return blob_preparation_result{blob_preparation_status::finished};
    }

    auto is_working() const noexcept -> bool {
        return _status == blob_preparation_status::working;
    }

    auto has_finished() const noexcept -> bool {
        return _status == blob_preparation_status::finished;
    }

    auto has_failed() const noexcept -> bool {
        return _status == blob_preparation_status::failed;
    }

    auto progress() const noexcept -> float {
        return _progress;
    }

private:
    float _progress;
    blob_preparation_status _status;
};
//------------------------------------------------------------------------------
export class blob_preparation_context {
public:
    auto first() noexcept -> bool {
        if(_first) {
            _first = false;
            return true;
        }
        return false;
    }

    auto operator()(const work_done is_working) noexcept
      -> blob_preparation_result {
        const auto result{
          _was_working ? blob_preparation_status::working
                       : blob_preparation_status::finished};
        _was_working = _was_working and is_working;
        return {result};
    }

private:
    bool _first : 1 {true};
    bool _was_working : 1 {true};
};
//------------------------------------------------------------------------------
export struct source_blob_io : interface<source_blob_io> {

    virtual auto prepare() noexcept -> blob_preparation_result {
        return blob_preparation_result::finished();
    }

    virtual auto is_at_eod(const span_size_t offs) noexcept -> bool {
        return offs >= total_size();
    }

    virtual auto total_size() noexcept -> span_size_t {
        return 0;
    }

    virtual auto fetch_fragment(
      [[maybe_unused]] const span_size_t offs,
      [[maybe_unused]] memory::block dst) noexcept -> span_size_t {
        return 0;
    }
};
//------------------------------------------------------------------------------
export struct target_blob_io : interface<target_blob_io> {
    virtual void handle_prepared([[maybe_unused]] float progress) noexcept {}

    virtual void handle_finished(
      [[maybe_unused]] const message_id msg_id,
      [[maybe_unused]] const message_age msg_age,
      [[maybe_unused]] const message_info& message,
      [[maybe_unused]] const blob_info& info) noexcept {}

    virtual void handle_cancelled() noexcept {}

    virtual auto store_fragment(
      [[maybe_unused]] const span_size_t offs,
      [[maybe_unused]] memory::const_block data,
      [[maybe_unused]] const blob_info& info) noexcept -> bool {
        return false;
    }

    virtual auto check_stored(
      [[maybe_unused]] const span_size_t offs,
      [[maybe_unused]] memory::const_block src) noexcept -> bool {
        return true;
    }
};
//------------------------------------------------------------------------------
export class buffer_blob_io;
//------------------------------------------------------------------------------
/// @brief Chunk info object passed to blob_stream_data_appended signal.
/// @ingroup msgbus
/// @see blob_stream_signals
export struct blob_stream_chunk {
    /// @brief Id of the blob request.
    identifier_t request_id;
    /// @brief Offset from the blob start.
    const span_size_t offset;
    /// @brief Data blocks.
    const memory::span<const memory::const_block> data;
    /// @brief Additional blob information.
    const blob_info& info;

    /// @brief Returns the total size of all data blocks.
    auto total_data_size() const noexcept -> span_size_t {
        return std::accumulate(
          data.begin(),
          data.end(),
          span_size(0),
          [](span_size_t sz, const auto& blk) { return sz + blk.size(); });
    }
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the resource_data_loader_node.
/// @ingroup msgbus
/// @see resource_data_loader_node
/// @see make_target_blob_stream_io
/// @see make_target_blob_chunk_io
export struct blob_stream_signals {
    /// @brief Emitted repeatedly when blob preparation progresses.
    signal<void(identifier_t blob_id, float) noexcept>
      blob_preparation_progressed;

    /// @brief Emitted repeatedly when a new consecutive chunk of data is streamed.
    signal<void(const blob_stream_chunk&) noexcept> blob_stream_data_appended;

    /// @brief Emitted once when a blob stream is completed.
    signal<void(identifier_t blob_id) noexcept> blob_stream_finished;

    /// @brief Emitted once if a blob stream is cancelled.
    signal<void(identifier_t blob_id) noexcept> blob_stream_cancelled;
};
//------------------------------------------------------------------------------
export using blob_id_t = std::uint32_t;
//------------------------------------------------------------------------------
/// @brief Creates a data stream target I/O object.
/// @ingroup msgbus
/// @see blob_stream_signals
///
/// This I/O object merges incoming BLOB data into consecutive blocks
/// so that they appear in the order from the start to the end of the BLOB
/// and emits the blob_stream_data_appended signal on a blob_stream_signals.
export auto make_target_blob_stream_io(
  blob_id_t blob_id,
  blob_stream_signals& sigs,
  memory::buffer_pool& buffers) -> unique_holder<target_blob_io>;
//------------------------------------------------------------------------------
/// @brief Creates a data stream target I/O object.
/// @ingroup msgbus
/// @see blob_stream_signals
///
/// This I/O object loads the whole BLOB into consecutive chunks of the
/// specified size and then emits the blob_stream_data_appended signal on a
/// blob_stream_signals once.
export auto make_target_blob_chunk_io(
  blob_id_t blob_id,
  span_size_t chunk_size,
  blob_stream_signals& sigs,
  memory::buffer_pool& buffers) -> unique_holder<target_blob_io>;
//------------------------------------------------------------------------------
struct pending_blob {
    message_id msg_id{};
    blob_info info{};
    shared_holder<source_blob_io> source_io{};
    shared_holder<target_blob_io> target_io{};
    // TODO: recycle the done parts vectors?
    double_buffer<std::vector<std::tuple<span_size_t, span_size_t>>>
      fragment_parts{};
    std::chrono::steady_clock::time_point latest_update{};
    timeout linger_time{std::chrono::seconds{15}};
    timeout prepare_update_time{std::chrono::seconds{5}};
    timeout max_time{};
    blob_id_t source_blob_id{0U};
    blob_id_t target_blob_id{0U};
    float prepare_progress{0.F};
    float previous_progress{0.F};

    auto source_buffer_io() noexcept -> buffer_blob_io*;
    auto target_buffer_io() noexcept -> buffer_blob_io*;

    auto done_parts() const noexcept -> const auto& {
        return fragment_parts.current();
    }

    auto done_parts() noexcept -> auto& {
        return fragment_parts.current();
    }

    auto todo_parts() const noexcept -> const auto& {
        return fragment_parts.next();
    }

    auto todo_parts() noexcept -> auto& {
        return fragment_parts.next();
    }

    auto sent_size() const noexcept -> span_size_t;
    auto received_size() const noexcept -> span_size_t;
    auto total_size() const noexcept -> span_size_t;
    auto total_size_mismatch(const span_size_t size) const noexcept -> bool;

    void handle_source_preparing(float) noexcept;
    auto prepare() noexcept -> blob_preparation_result;
    auto sent_everything() const noexcept -> bool;
    auto received_everything() const noexcept -> bool;

    auto fetch(const span_size_t offs, memory::block dst) noexcept;
    auto store(const span_size_t offs, const memory::const_block src) noexcept;
    auto check(const span_size_t offs, const memory::const_block blk) noexcept;

    auto age() const noexcept -> message_age;

    auto merge_fragment(
      const span_size_t bgn,
      const memory::const_block) noexcept -> bool;
    void merge_resend_request(const span_size_t bgn, span_size_t end) noexcept;
    void handle_target_preparing(float) noexcept;
};
//------------------------------------------------------------------------------
export class blob_manipulator : main_ctx_object {
public:
    blob_manipulator(
      main_ctx_parent parent,
      message_id fragment_msg_id,
      message_id resend_msg_id,
      message_id prepare_msg_id) noexcept;

    auto max_blob_size() const noexcept -> valid_if_positive<span_size_t> {
        return {span_size(_max_blob_size)};
    }

    using target_io_getter = callable_ref<unique_holder<target_blob_io>(
      const message_id,
      const span_size_t,
      blob_manipulator&) noexcept>;

    auto make_target_io(const span_size_t total_size) noexcept
      -> unique_holder<target_blob_io>;

    using send_handler =
      callable_ref<bool(const message_id, const message_view&) noexcept>;

    auto update(
      const send_handler do_send,
      const span_size_t max_message_size) noexcept -> work_done;

    auto push_outgoing(
      const message_id msg_id,
      const endpoint_id_t source_id,
      const endpoint_id_t target_id,
      const blob_id_t target_blob_id,
      shared_holder<source_blob_io> io,
      const std::chrono::seconds max_time,
      const blob_options options,
      const message_priority priority) noexcept -> blob_id_t;

    auto push_outgoing(
      const message_id msg_id,
      const endpoint_id_t source_id,
      const endpoint_id_t target_id,
      const blob_id_t target_blob_id,
      shared_holder<source_blob_io> io,
      const std::chrono::seconds max_time,
      const message_priority priority) noexcept -> blob_id_t {
        return push_outgoing(
          msg_id,
          source_id,
          target_id,
          target_blob_id,
          std::move(io),
          max_time,
          blob_options{},
          priority);
    }

    auto push_outgoing(
      const message_id msg_id,
      const endpoint_id_t source_id,
      const endpoint_id_t target_id,
      const blob_id_t target_blob_id,
      const memory::const_block src,
      const std::chrono::seconds max_time,
      const blob_options options,
      const message_priority priority) noexcept -> blob_id_t;

    auto push_outgoing(
      const message_id msg_id,
      const endpoint_id_t source_id,
      const endpoint_id_t target_id,
      const blob_id_t target_blob_id,
      const memory::const_block src,
      const std::chrono::seconds max_time,
      const message_priority priority) noexcept -> blob_id_t {
        return push_outgoing(
          msg_id,
          source_id,
          target_id,
          target_blob_id,
          src,
          max_time,
          blob_options{},
          priority);
    }

    auto expect_incoming(
      const message_id msg_id,
      const endpoint_id_t source_id,
      const blob_id_t target_blob_id,
      shared_holder<target_blob_io> io,
      const std::chrono::seconds max_time) noexcept -> bool;

    auto push_incoming_fragment(
      const message_id msg_id,
      const endpoint_id_t source_id,
      const blob_id_t source_blob_id,
      const blob_id_t target_blob_id,
      const std::int64_t offset,
      const std::int64_t total,
      target_io_getter get_io,
      const memory::const_block fragment,
      const blob_options options,
      const message_priority priority) noexcept -> bool;

    auto process_incoming(const message_view& message) noexcept -> bool;
    auto process_incoming(target_io_getter, const message_view& message) noexcept
      -> bool;
    auto process_resend(const message_view& message) noexcept -> bool;
    auto process_prepare(const message_view& message) noexcept -> bool;

    auto cancel_incoming(const endpoint_id_t target_blob_id) noexcept -> bool;

    using fetch_handler = callable_ref<
      bool(const message_id, const message_age, const message_view&) noexcept>;

    auto handle_complete() noexcept -> span_size_t;
    auto fetch_all(const fetch_handler) noexcept -> span_size_t;

    auto has_outgoing() const noexcept -> bool {
        return not _outgoing.empty();
    }
    auto process_outgoing(
      const send_handler,
      const span_size_t max_data_size,
      span_size_t max_messages) noexcept -> work_done;

private:
    auto _cleanup_outgoing() noexcept -> std::size_t;
    auto _cleanup_incoming() noexcept -> std::size_t;
    auto _done_begin_end(
      const std::vector<std::tuple<span_size_t, span_size_t>>& done,
      const span_size_t total_size,
      const span_size_t max_message_size) const noexcept
      -> std::tuple<span_size_t, span_size_t>;

    auto _process_preparing_outgoing(
      const send_handler do_send,
      const span_size_t max_message_size,
      pending_blob& pending) noexcept -> work_done;
    auto _process_finished_outgoing(
      const send_handler do_send,
      const span_size_t max_message_size,
      pending_blob& pending) noexcept -> work_done;

    const message_id _fragment_msg_id;
    const message_id _resend_msg_id;
    const message_id _prepare_msg_id;
    std::int64_t _max_blob_size{128 * 1024 * 1024};
    blob_id_t _blob_id_sequence{0U};
    memory::buffer _scratch_buffer{};
    memory::buffer_pool _buffers{};
    std::size_t _outgoing_index{};
    std::vector<pending_blob> _outgoing{};
    std::vector<pending_blob> _incoming{};

    auto _message_size(const pending_blob&, const span_size_t max_message_size)
      const noexcept -> span_size_t;

    auto _make_target_io(
      const message_id,
      const span_size_t total_size,
      blob_manipulator&) noexcept -> unique_holder<target_blob_io>;

    auto _scratch_block(const span_size_t size) noexcept -> memory::block;
    auto _next_blob_id() noexcept -> blob_id_t;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

