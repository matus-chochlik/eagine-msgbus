/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus.core:blobs;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.valid_if;
import eagine.core.utility;
import eagine.core.main_ctx;
import :types;
import :message;
import <cstdint>;
import <chrono>;
import <vector>;
import <type_traits>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
using blob_options_t = std::underlying_type_t<blob_option>;
//------------------------------------------------------------------------------
export struct blob_info {
    identifier_t source_id{0U};
    identifier_t target_id{0U};
    span_size_t total_size{0};
    blob_options options;
    message_priority priority{message_priority::normal};
};
//------------------------------------------------------------------------------
export struct source_blob_io : interface<source_blob_io> {

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
export struct blob_stream_signals {
    signal<void(
      identifier_t blob_id,
      const span_size_t offset,
      const memory::span<const memory::const_block>,
      const blob_info& info) noexcept>
      blob_stream_data_appended;

    signal<void(identifier_t blob_id) noexcept> blob_stream_finished;

    signal<void(identifier_t blob_id) noexcept> blob_stream_cancelled;
};
//------------------------------------------------------------------------------
export auto make_target_blob_stream_io(
  identifier_t blob_id,
  blob_stream_signals& sigs,
  memory::buffer_pool& buffers) -> std::unique_ptr<target_blob_io>;
//------------------------------------------------------------------------------
export auto make_target_blob_chunk_io(
  identifier_t blob_id,
  span_size_t chunk_size,
  blob_stream_signals& sigs,
  memory::buffer_pool& buffers) -> std::unique_ptr<target_blob_io>;
//------------------------------------------------------------------------------
export using blob_id_t = std::uint32_t;
//------------------------------------------------------------------------------
struct pending_blob {
    message_id msg_id{};
    blob_info info{};
    std::shared_ptr<source_blob_io> source_io{};
    std::shared_ptr<target_blob_io> target_io{};
    // TODO: recycle the done parts vectors?
    double_buffer<std::vector<std::tuple<span_size_t, span_size_t>>>
      fragment_parts{};
    std::chrono::steady_clock::time_point latest_update{};
    timeout max_time{};
    blob_id_t source_blob_id{0U};
    blob_id_t target_blob_id{0U};

    auto source_buffer_io() noexcept -> buffer_blob_io*;
    auto target_buffer_io() noexcept -> buffer_blob_io*;

    auto done_parts() const noexcept -> const auto& {
        return fragment_parts.front();
    }

    auto done_parts() noexcept -> auto& {
        return fragment_parts.front();
    }

    auto todo_parts() const noexcept -> const auto& {
        return fragment_parts.back();
    }

    auto todo_parts() noexcept -> auto& {
        return fragment_parts.back();
    }

    auto sent_size() const noexcept -> span_size_t;
    auto received_size() const noexcept -> span_size_t;
    auto total_size_mismatch(const span_size_t size) const noexcept -> bool;

    auto sent_everything() const noexcept -> bool;
    auto received_everything() const noexcept -> bool;

    auto fetch(const span_size_t offs, memory::block dst) noexcept {
        assert(source_io);
        return source_io->fetch_fragment(offs, dst);
    }

    auto store(const span_size_t offs, const memory::const_block src) noexcept {
        assert(target_io);
        return target_io->store_fragment(offs, src, info);
    }

    auto check(const span_size_t offs, const memory::const_block blk) noexcept {
        assert(target_io);
        return target_io->check_stored(offs, blk);
    }

    auto age() const noexcept -> message_age {
        return std::chrono::duration_cast<message_age>(max_time.elapsed_time());
    }

    auto merge_fragment(
      const span_size_t bgn,
      const memory::const_block) noexcept -> bool;
    void merge_resend_request(const span_size_t bgn, span_size_t end) noexcept;
};
//------------------------------------------------------------------------------
export class blob_manipulator : main_ctx_object {
public:
    blob_manipulator(
      main_ctx_parent parent,
      message_id fragment_msg_id,
      message_id resend_msg_id) noexcept
      : main_ctx_object{"BlobManipl", parent}
      , _fragment_msg_id{std::move(fragment_msg_id)}
      , _resend_msg_id{std::move(resend_msg_id)} {}

    auto max_blob_size() const noexcept -> valid_if_positive<span_size_t> {
        return {span_size(_max_blob_size)};
    }

    using target_io_getter = callable_ref<std::unique_ptr<target_blob_io>(
      const message_id,
      const span_size_t,
      blob_manipulator&) noexcept>;

    auto make_target_io(const span_size_t total_size) noexcept
      -> std::unique_ptr<target_blob_io>;

    using send_handler =
      callable_ref<bool(const message_id, const message_view&) noexcept>;

    auto update(const send_handler do_send) noexcept -> work_done;

    auto push_outgoing(
      const message_id msg_id,
      const identifier_t source_id,
      const identifier_t target_id,
      const blob_id_t target_blob_id,
      std::shared_ptr<source_blob_io> io,
      const std::chrono::seconds max_time,
      const blob_options options,
      const message_priority priority) noexcept -> blob_id_t;

    auto push_outgoing(
      const message_id msg_id,
      const identifier_t source_id,
      const identifier_t target_id,
      const blob_id_t target_blob_id,
      std::shared_ptr<source_blob_io> io,
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
      const identifier_t source_id,
      const identifier_t target_id,
      const blob_id_t target_blob_id,
      const memory::const_block src,
      const std::chrono::seconds max_time,
      const blob_options options,
      const message_priority priority) noexcept -> blob_id_t;

    auto push_outgoing(
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
          src,
          max_time,
          blob_options{},
          priority);
    }

    auto expect_incoming(
      const message_id msg_id,
      const identifier_t source_id,
      const blob_id_t target_blob_id,
      std::shared_ptr<target_blob_io> io,
      const std::chrono::seconds max_time) noexcept -> bool;

    auto push_incoming_fragment(
      const message_id msg_id,
      const identifier_t source_id,
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

    auto cancel_incoming(const identifier_t target_blob_id) noexcept -> bool;

    using fetch_handler = callable_ref<
      bool(const message_id, const message_age, const message_view&) noexcept>;

    auto handle_complete() noexcept -> span_size_t;
    auto fetch_all(const fetch_handler) noexcept -> span_size_t;

    auto has_outgoing() const noexcept -> bool {
        return !_outgoing.empty();
    }
    auto process_outgoing(
      const send_handler,
      const span_size_t max_data_size) noexcept -> work_done;

private:
    const message_id _fragment_msg_id;
    const message_id _resend_msg_id;
    std::int64_t _max_blob_size{128 * 1024 * 1024};
    blob_id_t _blob_id_sequence{0U};
    memory::buffer _scratch_buffer{};
    memory::buffer_pool _buffers{};
    std::vector<pending_blob> _outgoing{};
    std::vector<pending_blob> _incoming{};

    auto _message_size(const pending_blob&, const span_size_t max_message_size)
      const noexcept -> span_size_t;

    auto _make_target_io(
      const message_id,
      const span_size_t total_size,
      blob_manipulator&) noexcept -> std::unique_ptr<target_blob_io>;

    auto _scratch_block(const span_size_t size) noexcept -> memory::block;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

