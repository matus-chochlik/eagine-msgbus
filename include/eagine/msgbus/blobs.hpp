/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_BLOBS_HPP
#define EAGINE_MSGBUS_BLOBS_HPP

#include "message.hpp"
#include <eagine/bool_aggregate.hpp>
#include <eagine/callable_ref.hpp>
#include <eagine/double_buffer.hpp>
#include <eagine/interface.hpp>
#include <eagine/main_ctx_object.hpp>
#include <eagine/memory/buffer_pool.hpp>
#include <eagine/memory/split_block.hpp>
#include <eagine/overwrite_guard.hpp>
#include <eagine/timeout.hpp>
#include <eagine/valid_if/positive.hpp>
#include <cstdint>
#include <vector>

namespace eagine::msgbus {
//------------------------------------------------------------------------------
struct blob_io : interface<blob_io> {

    virtual auto is_at_eod(const span_size_t offs) -> bool {
        return offs >= total_size();
    }

    virtual auto total_size() -> span_size_t {
        return 0;
    }

    virtual auto fetch_fragment(const span_size_t offs, memory::block dst)
      -> span_size_t {
        EAGINE_MAYBE_UNUSED(offs);
        EAGINE_MAYBE_UNUSED(dst);
        return 0;
    }

    virtual auto store_fragment(const span_size_t offs, memory::const_block src)
      -> bool {
        EAGINE_MAYBE_UNUSED(offs);
        EAGINE_MAYBE_UNUSED(src);
        return false;
    }

    virtual auto check_stored(const span_size_t offs, memory::const_block src)
      -> bool {
        EAGINE_MAYBE_UNUSED(offs);
        EAGINE_MAYBE_UNUSED(src);
        return true;
    }

    virtual void handle_finished(
      const message_id msg_id,
      const message_age msg_age,
      const message_info& message) {
        EAGINE_MAYBE_UNUSED(msg_id);
        EAGINE_MAYBE_UNUSED(msg_age);
        EAGINE_MAYBE_UNUSED(message);
    }

    virtual void handle_cancelled() {}
};
//------------------------------------------------------------------------------
class buffer_blob_io;
//------------------------------------------------------------------------------
using blob_id_t = std::uint32_t;
struct pending_blob : overwrite_guard<> {
    message_id msg_id{};
    identifier_t source_id{0U};
    identifier_t target_id{0U};
    std::shared_ptr<blob_io> io{};
    span_size_t total_size{0};
    // TODO: recycle the done parts vectors?
    double_buffer<std::vector<std::tuple<span_size_t, span_size_t>>>
      fragment_parts{};
    std::chrono::steady_clock::time_point latest_update{};
    timeout max_time{};
    blob_id_t source_blob_id{0U};
    blob_id_t target_blob_id{0U};
    message_priority priority{message_priority::normal};

    auto buffer_io() noexcept -> buffer_blob_io*;

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

    auto fetch(const span_size_t offs, memory::block dst) {
        EAGINE_ASSERT(io);
        return io->fetch_fragment(offs, dst);
    }

    auto store(const span_size_t offs, const memory::const_block src) {
        EAGINE_ASSERT(io);
        return io->store_fragment(offs, src);
    }

    auto check(const span_size_t offs, const memory::const_block blk) {
        EAGINE_ASSERT(io);
        return io->check_stored(offs, blk);
    }

    auto age() const noexcept -> message_age {
        return std::chrono::duration_cast<message_age>(max_time.elapsed_time());
    }

    auto merge_fragment(const span_size_t bgn, const memory::const_block)
      -> bool;
    void merge_resend_request(const span_size_t bgn, span_size_t end);
};
//------------------------------------------------------------------------------
class blob_manipulator : main_ctx_object {
public:
    blob_manipulator(
      main_ctx_parent parent,
      message_id fragment_msg_id,
      message_id resend_msg_id)
      : main_ctx_object{EAGINE_ID(BlobManipl), parent}
      , _fragment_msg_id{std::move(fragment_msg_id)}
      , _resend_msg_id{std::move(resend_msg_id)} {}

    auto max_blob_size() const noexcept -> valid_if_positive<span_size_t> {
        return {span_size(_max_blob_size)};
    }

    auto message_size(const pending_blob&, const span_size_t max_message_size)
      const noexcept -> span_size_t;

    using io_getter = callable_ref<std::unique_ptr<
      blob_io>(const message_id, const span_size_t, blob_manipulator&)>;

    auto make_io(const span_size_t total_size) -> std::unique_ptr<blob_io>;

    using send_handler =
      callable_ref<bool(const message_id, const message_view&)>;

    auto update(const send_handler do_send) -> work_done;

    auto push_outgoing(
      const message_id msg_id,
      const identifier_t source_id,
      const identifier_t target_id,
      const blob_id_t target_blob_id,
      std::shared_ptr<blob_io> io,
      const std::chrono::seconds max_time,
      const message_priority priority) -> blob_id_t;

    auto push_outgoing(
      const message_id msg_id,
      const identifier_t source_id,
      const identifier_t target_id,
      const blob_id_t target_blob_id,
      const memory::const_block src,
      const std::chrono::seconds max_time,
      const message_priority priority) -> blob_id_t;

    auto expect_incoming(
      const message_id msg_id,
      const identifier_t source_id,
      const blob_id_t target_blob_id,
      std::shared_ptr<blob_io> io,
      const std::chrono::seconds max_time) -> bool;

    auto push_incoming_fragment(
      const message_id msg_id,
      const identifier_t source_id,
      const blob_id_t source_blob_id,
      const blob_id_t target_blob_id,
      const std::int64_t offset,
      const std::int64_t total,
      io_getter get_io,
      const memory::const_block fragment,
      const message_priority priority) -> bool;

    auto process_incoming(const message_view& message) -> bool;
    auto process_incoming(io_getter, const message_view& message) -> bool;
    auto process_resend(const message_view& message) -> bool;

    auto cancel_incoming(const identifier_t target_blob_id) -> bool;

    using fetch_handler = callable_ref<
      bool(const message_id, const message_age, const message_view&)>;

    auto handle_complete() -> span_size_t;
    auto fetch_all(const fetch_handler) -> span_size_t;

    auto has_outgoing() const noexcept -> bool {
        return !_outgoing.empty();
    }
    auto process_outgoing(const send_handler, const span_size_t max_data_size)
      -> work_done;

private:
    const message_id _fragment_msg_id;
    const message_id _resend_msg_id;
    std::int64_t _max_blob_size{128 * 1024 * 1024};
    blob_id_t _blob_id_sequence{0U};
    memory::buffer _scratch_buffer{};
    memory::buffer_pool _buffers{};
    std::vector<pending_blob> _outgoing{};
    std::vector<pending_blob> _incoming{};

    auto _make_io(
      const message_id,
      const span_size_t total_size,
      blob_manipulator&) -> std::unique_ptr<blob_io>;

    auto _scratch_block(const span_size_t size) -> memory::block;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#if !EAGINE_MSGBUS_LIBRARY || defined(EAGINE_IMPLEMENTING_LIBRARY)
#include <eagine/msgbus/blobs.inl>
#endif
#endif // EAGINE_MSGBUS_BLOBS_HPP
