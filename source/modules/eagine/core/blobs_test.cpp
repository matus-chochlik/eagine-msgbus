/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/testing/unit_begin_ctx.hpp>
import std;
import eagine.core;
import eagine.msgbus.core;
//------------------------------------------------------------------------------
// round-trip zeroes
//------------------------------------------------------------------------------
class zeroes_source_blob_io final : public eagine::msgbus::source_blob_io {
public:
    zeroes_source_blob_io(const eagine::span_size_t size) noexcept
      : _size{size} {}

    auto total_size() noexcept -> eagine::span_size_t final {
        return _size;
    }

    auto fetch_fragment(
      const eagine::span_size_t offs,
      eagine::memory::block dst) noexcept -> eagine::span_size_t final {
        using namespace eagine::memory;
        return zero(head(dst, _size - offs)).size();
    }

private:
    eagine::span_size_t _size;
};
//------------------------------------------------------------------------------
class zeroes_target_blob_io final : public eagine::msgbus::target_blob_io {
public:
    zeroes_target_blob_io(
      eagitest::case_& test,
      eagitest::track& trck,
      const eagine::span_size_t size,
      bool& done) noexcept
      : _test{test}
      , _trck{trck}
      , _expected_size{size}
      , _done{done} {}

    void handle_finished(
      const eagine::message_id msg_id,
      [[maybe_unused]] const eagine::msgbus::message_age msg_age,
      [[maybe_unused]] const eagine::msgbus::message_info& message,
      [[maybe_unused]] const eagine::msgbus::blob_info& info) noexcept final {

        _test.check(
          msg_id.class_() == eagine::identifier{"test"}, "message id");
        _done = true;
        _trck.checkpoint(2);
    }

    void handle_cancelled() noexcept final {
        _test.fail("blob cancelled");
        _done = true;
    }

    auto store_fragment(
      [[maybe_unused]] const eagine::span_size_t offs,
      [[maybe_unused]] eagine::memory::const_block data,
      [[maybe_unused]] const eagine::msgbus::blob_info& info) noexcept
      -> bool final {

        _test.check(offs >= 0, "offset ok 1");
        _test.check(offs < _expected_size, "offset ok 2");
        for(const auto b : data) {
            _test.check_equal(b, eagine::byte{0}, "is zero");
        }
        _done_size += data.size();
        _trck.checkpoint(3);
        return true;
    }

    auto check_stored(
      [[maybe_unused]] const eagine::span_size_t offs,
      [[maybe_unused]] eagine::memory::const_block data) noexcept
      -> bool final {

        _test.check(offs >= 0, "offset ok 3");
        _test.check(offs < _expected_size, "offset ok 4");
        for(const auto b : data) {
            _test.check_equal(b, eagine::byte{0}, "is zero");
        }
        _trck.checkpoint(4);
        return true;
    }

private:
    eagitest::case_& _test;
    eagitest::track& _trck;
    eagine::span_size_t _expected_size;
    eagine::span_size_t _done_size{0};
    bool& _done;
};
//------------------------------------------------------------------------------
void blobs_roundtrip_zeroes_single_big(auto& s) {
    eagitest::case_ test{s, 1, "round-trip zeroes big"};
    eagitest::track trck{test, 1, 4};

    const eagine::message_id test_msg_id{"test", eagine::random_identifier()};
    const eagine::message_id send_msg_id{"test", "send"};
    const eagine::message_id resend_msg_id{"test", "resend"};
    eagine::msgbus::blob_manipulator sender{
      s.context(), send_msg_id, resend_msg_id};
    eagine::msgbus::blob_manipulator receiver{
      s.context(), send_msg_id, resend_msg_id};

    auto send_s2r = [&](
                      const eagine::message_id msg_id,
                      const eagine::msgbus::message_view& message) -> bool {
        test.check(msg_id == send_msg_id, "message id");

        receiver.process_incoming(message);

        trck.checkpoint(1);
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_s2r{
      eagine::construct_from, send_s2r};

    auto send_r2s = [&](
                      const eagine::message_id,
                      const eagine::msgbus::message_view&) -> bool {
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_r2s{
      eagine::construct_from, send_r2s};

    sender.push_outgoing(
      test_msg_id,
      1234,
      2345,
      0,
      {eagine::hold<zeroes_source_blob_io>, 16 * 1024 * 1024},
      std::chrono::hours{1},
      eagine::msgbus::message_priority::normal);

    bool done{false};

    receiver.expect_incoming(
      test_msg_id,
      1234,
      eagine::msgbus::blob_id_t(0),
      {eagine::hold<zeroes_target_blob_io>, test, trck, 16 * 1024 * 1024, done},
      std::chrono::hours{1});

    const eagine::span_size_t max_message_size{4096};
    while(not done) {
        sender.update(handler_s2r, max_message_size);
        sender.process_outgoing(handler_s2r, max_message_size, 1);
        receiver.update(handler_r2s, max_message_size);
        receiver.handle_complete();
    }
}
//------------------------------------------------------------------------------
void blobs_roundtrip_zeroes_single(unsigned r, auto& s) {
    eagitest::case_ test{s, 2, "round-trip zeroes"};
    eagitest::track trck{test, 1, 4};

    const eagine::message_id test_msg_id{"test", eagine::random_identifier()};
    const eagine::message_id send_msg_id{"test", "send"};
    const eagine::message_id resend_msg_id{"test", "resend"};
    eagine::msgbus::blob_manipulator sender{
      s.context(), send_msg_id, resend_msg_id};
    eagine::msgbus::blob_manipulator receiver{
      s.context(), send_msg_id, resend_msg_id};

    auto send_s2r = [&](
                      const eagine::message_id msg_id,
                      const eagine::msgbus::message_view& message) -> bool {
        test.check(msg_id == send_msg_id, "message id");

        receiver.process_incoming(message);

        trck.checkpoint(1);
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_s2r{
      eagine::construct_from, send_s2r};

    auto send_r2s = [&](
                      const eagine::message_id,
                      const eagine::msgbus::message_view&) -> bool {
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_r2s{
      eagine::construct_from, send_r2s};

    sender.push_outgoing(
      test_msg_id,
      0,
      1,
      eagine::msgbus::blob_id_t(r),
      {eagine::hold<zeroes_source_blob_io>, 1024 * 1024},
      std::chrono::hours{1},
      eagine::msgbus::message_priority::normal);

    bool done{false};

    receiver.expect_incoming(
      test_msg_id,
      0,
      eagine::msgbus::blob_id_t(r),
      {eagine::hold<zeroes_target_blob_io>, test, trck, 1024 * 1024, done},
      std::chrono::hours{1});

    const eagine::span_size_t max_message_size{2048};
    while(not done) {
        sender.update(handler_s2r, max_message_size);
        sender.process_outgoing(handler_s2r, max_message_size, 2);
        receiver.update(handler_r2s, max_message_size);
        receiver.handle_complete();
    }
}
//------------------------------------------------------------------------------
// round-trip bfs
//------------------------------------------------------------------------------
class bfs_source_blob_io final : public eagine::msgbus::source_blob_io {
public:
    bfs_source_blob_io(const eagine::span_size_t size) noexcept
      : _size{size} {}

    auto total_size() noexcept -> eagine::span_size_t final {
        return _size;
    }

    auto fetch_fragment(
      const eagine::span_size_t offs,
      eagine::memory::block dst) noexcept -> eagine::span_size_t final {
        using namespace eagine::memory;
        return fill(head(dst, _size - offs), 0xBFU).size();
    }

private:
    eagine::span_size_t _size;
};
//------------------------------------------------------------------------------
class bfs_target_blob_io final : public eagine::msgbus::target_blob_io {
public:
    bfs_target_blob_io(
      eagitest::case_& test,
      eagitest::track& trck,
      const eagine::span_size_t size,
      bool& done) noexcept
      : _test{test}
      , _trck{trck}
      , _expected_size{size}
      , _done{done} {}

    void handle_finished(
      const eagine::message_id msg_id,
      [[maybe_unused]] const eagine::msgbus::message_age msg_age,
      [[maybe_unused]] const eagine::msgbus::message_info& message,
      [[maybe_unused]] const eagine::msgbus::blob_info& info) noexcept final {

        _test.check(
          msg_id.method() == eagine::identifier{"test"}, "message id");
        _done = true;
        _trck.checkpoint(2);
    }

    void handle_cancelled() noexcept final {
        _test.fail("blob cancelled");
        _done = true;
    }

    auto store_fragment(
      [[maybe_unused]] const eagine::span_size_t offs,
      [[maybe_unused]] eagine::memory::const_block data,
      [[maybe_unused]] const eagine::msgbus::blob_info& info) noexcept
      -> bool final {

        _test.check(offs >= 0, "offset ok 1");
        _test.check(offs < _expected_size, "offset ok 2");
        for(const auto b : data) {
            _test.check_equal(b, eagine::byte{0xBF}, "is 0xBF");
        }
        _done_size += data.size();
        _trck.checkpoint(3);
        return true;
    }

    auto check_stored(
      [[maybe_unused]] const eagine::span_size_t offs,
      [[maybe_unused]] eagine::memory::const_block data) noexcept
      -> bool final {

        _test.check(offs >= 0, "offset ok 3");
        _test.check(offs < _expected_size, "offset ok 4");
        for(const auto b : data) {
            _test.check_equal(b, eagine::byte{0xBF}, "is 0xBF");
        }
        _trck.checkpoint(4);
        return true;
    }

private:
    eagitest::case_& _test;
    eagitest::track& _trck;
    eagine::span_size_t _expected_size;
    eagine::span_size_t _done_size{0};
    bool& _done;
};
//------------------------------------------------------------------------------
void blobs_roundtrip_bfs_single(auto& s) {
    eagitest::case_ test{s, 3, "round-trip 0xBFs"};
    eagitest::track trck{test, 1, 4};

    const eagine::message_id test_msg_id{eagine::random_identifier(), "test"};
    const eagine::message_id send_msg_id{"check", "send"};
    const eagine::message_id resend_msg_id{"check", "resend"};
    eagine::msgbus::blob_manipulator sender{
      s.context(), send_msg_id, resend_msg_id};
    eagine::msgbus::blob_manipulator receiver{
      s.context(), send_msg_id, resend_msg_id};

    auto send_s2r = [&](
                      const eagine::message_id msg_id,
                      const eagine::msgbus::message_view& message) -> bool {
        test.check(msg_id == send_msg_id, "message id");

        receiver.process_incoming(message);

        trck.checkpoint(1);
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_s2r{
      eagine::construct_from, send_s2r};

    auto send_r2s = [&](
                      const eagine::message_id,
                      const eagine::msgbus::message_view&) -> bool {
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_r2s{
      eagine::construct_from, send_r2s};

    for(unsigned r = 0; r < test.repeats(5); ++r) {
        sender.push_outgoing(
          test_msg_id,
          1,
          0,
          eagine::msgbus::blob_id_t(r),
          {eagine::hold<bfs_source_blob_io>, 1024 * 1024},
          std::chrono::hours{1},
          eagine::msgbus::message_priority::normal);

        bool done{false};

        receiver.expect_incoming(
          test_msg_id,
          1,
          eagine::msgbus::blob_id_t(r),
          {eagine::hold<bfs_target_blob_io>, test, trck, 1024 * 1024, done},
          std::chrono::hours{1});

        const eagine::span_size_t max_message_size{1024};
        while(not done) {
            sender.update(handler_s2r, max_message_size);
            sender.process_outgoing(handler_s2r, max_message_size, 4);
            receiver.update(handler_r2s, max_message_size);
            receiver.handle_complete();
        }
    }
}
//------------------------------------------------------------------------------
// round-trip ces
//------------------------------------------------------------------------------
class ces_source_blob_io final : public eagine::msgbus::source_blob_io {
public:
    ces_source_blob_io(const eagine::span_size_t size) noexcept
      : _size{size} {}

    auto total_size() noexcept -> eagine::span_size_t final {
        return _size;
    }

    auto fetch_fragment(
      const eagine::span_size_t offs,
      eagine::memory::block dst) noexcept -> eagine::span_size_t final {
        using namespace eagine::memory;
        return fill(head(dst, _size - offs), 0xCEU).size();
    }

private:
    eagine::span_size_t _size;
};
//------------------------------------------------------------------------------
class ces_target_blob_io final : public eagine::msgbus::target_blob_io {
public:
    ces_target_blob_io(
      eagitest::case_& test,
      eagitest::track& trck,
      const eagine::span_size_t size,
      unsigned& done) noexcept
      : _test{test}
      , _trck{trck}
      , _expected_size{size}
      , _done{done} {}

    void handle_finished(
      const eagine::message_id msg_id,
      [[maybe_unused]] const eagine::msgbus::message_age msg_age,
      [[maybe_unused]] const eagine::msgbus::message_info& message,
      [[maybe_unused]] const eagine::msgbus::blob_info& info) noexcept final {

        _test.check(
          msg_id.method() == eagine::identifier{"test"}, "message id");
        ++_done;
        _trck.checkpoint(2);
    }

    void handle_cancelled() noexcept final {
        _test.fail("blob cancelled");
        ++_done;
    }

    auto store_fragment(
      [[maybe_unused]] const eagine::span_size_t offs,
      [[maybe_unused]] eagine::memory::const_block data,
      [[maybe_unused]] const eagine::msgbus::blob_info& info) noexcept
      -> bool final {

        _test.check(offs >= 0, "offset ok 1");
        _test.check(offs < _expected_size, "offset ok 2");
        for(const auto b : data) {
            _test.check_equal(b, eagine::byte{0xCE}, "is 0xCE");
        }
        _done_size += data.size();
        _trck.checkpoint(3);
        return true;
    }

    auto check_stored(
      [[maybe_unused]] const eagine::span_size_t offs,
      [[maybe_unused]] eagine::memory::const_block data) noexcept
      -> bool final {

        _test.check(offs >= 0, "offset ok 3");
        _test.check(offs < _expected_size, "offset ok 4");
        for(const auto b : data) {
            _test.check_equal(b, eagine::byte{0xCE}, "is 0xCE");
        }
        _trck.checkpoint(4);
        return true;
    }

private:
    eagitest::case_& _test;
    eagitest::track& _trck;
    eagine::span_size_t _expected_size;
    eagine::span_size_t _done_size{0};
    unsigned& _done;
};
//------------------------------------------------------------------------------
void blobs_roundtrip_ces_multiple(auto& s) {
    eagitest::case_ test{s, 4, "round-trip 0xCEs"};
    eagitest::track trck{test, 1, 4};

    const eagine::message_id test_msg_id{eagine::random_identifier(), "test"};
    const eagine::message_id send_msg_id{"check", "send"};
    const eagine::message_id resend_msg_id{"check", "resend"};
    eagine::msgbus::blob_manipulator sender{
      s.context(), send_msg_id, resend_msg_id};
    eagine::msgbus::blob_manipulator receiver{
      s.context(), send_msg_id, resend_msg_id};

    auto send_s2r = [&](
                      const eagine::message_id msg_id,
                      const eagine::msgbus::message_view& message) -> bool {
        test.check(msg_id == send_msg_id, "message id");

        receiver.process_incoming(message);

        trck.checkpoint(1);
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_s2r{
      eagine::construct_from, send_s2r};

    auto send_r2s = [&](
                      const eagine::message_id,
                      const eagine::msgbus::message_view&) -> bool {
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_r2s{
      eagine::construct_from, send_r2s};

    const unsigned todo{test.repeats(5)};
    unsigned done{0};

    for(unsigned r = 0; r < todo; ++r) {
        sender.push_outgoing(
          test_msg_id,
          1,
          0,
          eagine::msgbus::blob_id_t(r),
          {eagine::hold<ces_source_blob_io>, 128 * 1024},
          std::chrono::hours{1},
          eagine::msgbus::message_priority::normal);

        receiver.expect_incoming(
          test_msg_id,
          1,
          eagine::msgbus::blob_id_t(r),
          {eagine::hold<ces_target_blob_io>, test, trck, 128 * 1024, done},
          std::chrono::hours{1});
    }

    const eagine::span_size_t max_message_size{1024};
    while(done < todo) {
        sender.update(handler_s2r, max_message_size);
        sender.process_outgoing(handler_s2r, max_message_size, 6);
        receiver.update(handler_r2s, max_message_size);
        receiver.handle_complete();
    }
}
//------------------------------------------------------------------------------
// chunk signals
//------------------------------------------------------------------------------
void blobs_roundtrip_chunk_signals_finished(auto& s) {
    eagitest::case_ test{s, 5, "chunk signals"};
    eagitest::track trck{test, 1, 3};
    auto& rg{test.random()};

    const eagine::message_id test_msg_id{eagine::random_identifier(), "test"};
    const eagine::message_id send_msg_id{"check", "send"};
    const eagine::message_id resend_msg_id{"check", "resend"};
    eagine::msgbus::blob_manipulator sender{
      s.context(), send_msg_id, resend_msg_id};
    eagine::msgbus::blob_manipulator receiver{
      s.context(), send_msg_id, resend_msg_id};

    eagine::msgbus::blob_stream_signals signals;
    std::map<eagine::identifier_t, std::array<eagine::span_size_t, 2>>
      blob_sizes;
    const auto check_stream_data =
      [&](
        eagine::identifier_t blob_id,
        const eagine::span_size_t offset,
        const eagine::memory::span<const eagine::memory::const_block> data,
        const eagine::msgbus::blob_info&) {
          test.check(offset >= 0, "offset ok 1");
          test.check(offset <= blob_sizes[blob_id][1], "offset ok 2");
          for(const auto blk : data) {
              for(const auto b : blk) {
                  test.check(
                    b == eagine::byte{0xBF} or b == eagine::byte{0xCE},
                    "content is ok");
                  trck.checkpoint(2);
              }
              blob_sizes[blob_id][1] += blk.size();
          }
      };
    signals.blob_stream_data_appended.connect(
      {eagine::construct_from, check_stream_data});

    unsigned done{0};
    const auto check_stream_finished = [&](const eagine::identifier_t blob_id) {
        test.check_equal(
          blob_sizes[blob_id][0], blob_sizes[blob_id][1], "blob data complete");
        blob_sizes.erase(blob_id);
        ++done;
        trck.checkpoint(3);
    };
    signals.blob_stream_finished.connect(
      {eagine::construct_from, check_stream_finished});

    auto send_s2r = [&](
                      const eagine::message_id msg_id,
                      const eagine::msgbus::message_view& message) -> bool {
        test.check(msg_id == send_msg_id, "message id");

        receiver.process_incoming(message);

        trck.checkpoint(1);
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_s2r{
      eagine::construct_from, send_s2r};

    auto send_r2s = [&](
                      const eagine::message_id,
                      const eagine::msgbus::message_view&) -> bool {
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_r2s{
      eagine::construct_from, send_r2s};

    const unsigned todo{test.repeats(6)};

    eagine::memory::buffer_pool buffers;

    for(unsigned r = 0; r < todo; ++r) {
        const auto blob_id{eagine::msgbus::blob_id_t(r)};
        if(rg.get_bool()) {
            const auto blob_size{rg.get_between(4, 48) * 1024};
            sender.push_outgoing(
              test_msg_id,
              1,
              0,
              blob_id,
              {eagine::hold<bfs_source_blob_io>, blob_size},
              std::chrono::hours{1},
              eagine::msgbus::message_priority::normal);
            blob_sizes[blob_id] = {blob_size, 0};
        } else {
            const auto blob_size{rg.get_between(48, 96) * 1024};
            sender.push_outgoing(
              test_msg_id,
              1,
              0,
              blob_id,
              {eagine::hold<ces_source_blob_io>, blob_size},
              std::chrono::hours{1},
              eagine::msgbus::message_priority::normal);
            blob_sizes[blob_id] = {blob_size, 0};
        }

        receiver.expect_incoming(
          test_msg_id,
          1,
          eagine::msgbus::blob_id_t(r),
          eagine::msgbus::make_target_blob_chunk_io(
            eagine::msgbus::blob_id_t(r), 1024, signals, buffers),
          std::chrono::hours{1});
    }

    const eagine::span_size_t max_message_size{2048};
    while(done < todo) {
        sender.update(handler_s2r, max_message_size);
        sender.process_outgoing(handler_s2r, max_message_size, 3);
        receiver.update(handler_r2s, max_message_size);
        receiver.handle_complete();
    }

    test.check(blob_sizes.empty(), "all blobs finished");
}
//------------------------------------------------------------------------------
// stream signals
//------------------------------------------------------------------------------
void blobs_roundtrip_stream_signals_finished(auto& s) {
    eagitest::case_ test{s, 6, "stream signals"};
    eagitest::track trck{test, 1, 3};
    auto& rg{test.random()};

    const eagine::message_id test_msg_id{eagine::random_identifier(), "test"};
    const eagine::message_id send_msg_id{"check", "send"};
    const eagine::message_id resend_msg_id{"check", "resend"};
    eagine::msgbus::blob_manipulator sender{
      s.context(), send_msg_id, resend_msg_id};
    eagine::msgbus::blob_manipulator receiver{
      s.context(), send_msg_id, resend_msg_id};

    eagine::msgbus::blob_stream_signals signals;
    std::map<eagine::identifier_t, std::array<eagine::span_size_t, 2>>
      blob_sizes;
    const auto check_stream_data =
      [&](
        eagine::identifier_t blob_id,
        const eagine::span_size_t offset,
        const eagine::memory::span<const eagine::memory::const_block> data,
        const eagine::msgbus::blob_info&) {
          for(const auto blk : data) {
              for(const auto b : blk) {
                  test.check(
                    b == eagine::byte{0xBF} or b == eagine::byte{0xCE},
                    "content is ok");
                  trck.checkpoint(2);
              }
              test.check_equal(blob_sizes[blob_id][1], offset, "offset ok");
              blob_sizes[blob_id][1] += blk.size();
          }
      };
    signals.blob_stream_data_appended.connect(
      {eagine::construct_from, check_stream_data});

    unsigned done{0};
    const auto check_stream_finished = [&](const eagine::identifier_t blob_id) {
        test.check_equal(
          blob_sizes[blob_id][0], blob_sizes[blob_id][1], "blob data complete");
        blob_sizes.erase(blob_id);
        ++done;
        trck.checkpoint(3);
    };
    signals.blob_stream_finished.connect(
      {eagine::construct_from, check_stream_finished});

    auto send_s2r = [&](
                      const eagine::message_id msg_id,
                      const eagine::msgbus::message_view& message) -> bool {
        test.check(msg_id == send_msg_id, "message id");

        receiver.process_incoming(message);

        trck.checkpoint(1);
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_s2r{
      eagine::construct_from, send_s2r};

    auto send_r2s = [&](
                      const eagine::message_id,
                      const eagine::msgbus::message_view&) -> bool {
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_r2s{
      eagine::construct_from, send_r2s};

    const unsigned todo{test.repeats(6)};

    eagine::memory::buffer_pool buffers;

    for(unsigned r = 0; r < todo; ++r) {
        const auto blob_id{eagine::msgbus::blob_id_t(r)};
        if(rg.get_bool()) {
            const auto blob_size{rg.get_between(4, 64) * 1024};
            sender.push_outgoing(
              test_msg_id,
              1,
              0,
              blob_id,
              {eagine::hold<bfs_source_blob_io>, blob_size},
              std::chrono::hours{1},
              eagine::msgbus::message_priority::normal);
            blob_sizes[blob_id] = {blob_size, 0};
        } else {
            const auto blob_size{rg.get_between(64, 128) * 1024};
            sender.push_outgoing(
              test_msg_id,
              1,
              0,
              blob_id,
              {eagine::hold<ces_source_blob_io>, blob_size},
              std::chrono::hours{1},
              eagine::msgbus::message_priority::normal);
            blob_sizes[blob_id] = {blob_size, 0};
        }

        receiver.expect_incoming(
          test_msg_id,
          1,
          eagine::msgbus::blob_id_t(r),
          eagine::msgbus::make_target_blob_stream_io(
            eagine::msgbus::blob_id_t(r), signals, buffers),
          std::chrono::hours{1});
    }

    const eagine::span_size_t max_message_size{512};
    while(done < todo) {
        sender.update(handler_s2r, max_message_size);
        sender.process_outgoing(handler_s2r, max_message_size, 6);
        receiver.update(handler_r2s, max_message_size);
        receiver.handle_complete();
    }

    test.check(blob_sizes.empty(), "all blobs finished");
}
//------------------------------------------------------------------------------
// chunk signals failed
//------------------------------------------------------------------------------
void blobs_roundtrip_chunk_signals_failed(auto& s) {
    eagitest::case_ test{s, 7, "chunk signals failed"};
    eagitest::track trck{test, 1, 2};
    auto& rg{test.random()};

    const eagine::message_id test_msg_id{eagine::random_identifier(), "test"};
    const eagine::message_id send_msg_id{"check", "send"};
    const eagine::message_id resend_msg_id{"check", "resend"};
    eagine::msgbus::blob_manipulator sender{
      s.context(), send_msg_id, resend_msg_id};
    eagine::msgbus::blob_manipulator receiver{
      s.context(), send_msg_id, resend_msg_id};

    eagine::msgbus::blob_stream_signals signals;

    bool done{false};
    const auto check_stream_cancelled = [&](const eagine::identifier_t) {
        done = true;
        trck.checkpoint(2);
    };
    signals.blob_stream_cancelled.connect(
      {eagine::construct_from, check_stream_cancelled});

    auto send_s2r = [&](
                      const eagine::message_id msg_id,
                      const eagine::msgbus::message_view& message) -> bool {
        test.check(msg_id == send_msg_id, "message id");

        if(rg.one_of(1000000)) {
            if(rg.one_of(1000000)) {
                if(rg.one_of(1000000)) {
                    receiver.process_incoming(message);
                }
            }
        }

        trck.checkpoint(1);
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_s2r{
      eagine::construct_from, send_s2r};

    auto send_r2s = [&](
                      const eagine::message_id,
                      const eagine::msgbus::message_view&) -> bool {
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_r2s{
      eagine::construct_from, send_r2s};

    const unsigned todo{test.repeats(6)};

    eagine::memory::buffer_pool buffers;

    for(unsigned r = 0; r < todo; ++r) {
        const auto blob_id{eagine::msgbus::blob_id_t(r)};
        if(rg.get_bool()) {
            const auto blob_size{rg.get_between(4, 48) * 1024};
            sender.push_outgoing(
              test_msg_id,
              1,
              0,
              blob_id,
              {eagine::hold<bfs_source_blob_io>, blob_size},
              std::chrono::seconds{1},
              eagine::msgbus::message_priority::normal);
        } else {
            const auto blob_size{rg.get_between(48, 96) * 1024};
            sender.push_outgoing(
              test_msg_id,
              1,
              0,
              blob_id,
              {eagine::hold<ces_source_blob_io>, blob_size},
              std::chrono::seconds{1},
              eagine::msgbus::message_priority::normal);
        }

        receiver.expect_incoming(
          test_msg_id,
          1,
          eagine::msgbus::blob_id_t(r),
          eagine::msgbus::make_target_blob_chunk_io(
            eagine::msgbus::blob_id_t(r), 1024, signals, buffers),
          std::chrono::seconds{1});
    }

    const eagine::span_size_t max_message_size{2048};
    while(not done) {
        sender.update(handler_s2r, max_message_size);
        sender.process_outgoing(handler_s2r, max_message_size, 3);
        receiver.update(handler_r2s, max_message_size);
        receiver.handle_complete();
    }
}
//------------------------------------------------------------------------------
// round-trip resend 1
//------------------------------------------------------------------------------
void blobs_roundtrip_resend_1(auto& s) {
    eagitest::case_ test{s, 8, "round-trip resend"};
    eagitest::track trck{test, 1, 5};
    auto& rg{test.random()};

    const eagine::message_id test_msg_id{eagine::random_identifier(), "test"};
    const eagine::message_id send_msg_id{"check", "send"};
    const eagine::message_id resend_msg_id{"check", "resend"};
    eagine::msgbus::blob_manipulator sender{
      s.context(), send_msg_id, resend_msg_id};
    eagine::msgbus::blob_manipulator receiver{
      s.context(), send_msg_id, resend_msg_id};

    auto send_s2r = [&](
                      const eagine::message_id msg_id,
                      const eagine::msgbus::message_view& message) -> bool {
        test.check(msg_id == send_msg_id, "message id");

        if(not rg.one_of(5)) {
            receiver.process_incoming(message);
            trck.checkpoint(1);
        }
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_s2r{
      eagine::construct_from, send_s2r};

    auto send_r2s = [&](
                      const eagine::message_id msg_id,
                      const eagine::msgbus::message_view& message) -> bool {
        if(not rg.one_of(11)) {
            if(msg_id == resend_msg_id) {
                sender.process_resend(message);
                trck.checkpoint(5);
            }
        }
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_r2s{
      eagine::construct_from, send_r2s};

    for(unsigned r = 0; r < test.repeats(3); ++r) {
        sender.push_outgoing(
          test_msg_id,
          1,
          0,
          eagine::msgbus::blob_id_t(r),
          {eagine::hold<bfs_source_blob_io>, 48 * 1024},
          std::chrono::hours{1},
          eagine::msgbus::message_priority::normal);

        bool done{false};

        receiver.expect_incoming(
          test_msg_id,
          1,
          eagine::msgbus::blob_id_t(r),
          {eagine::hold<bfs_target_blob_io>, test, trck, 48 * 1024, done},
          std::chrono::hours{1});

        const eagine::span_size_t max_message_size{2048};
        while(not done) {
            sender.update(handler_s2r, max_message_size);
            sender.process_outgoing(handler_s2r, max_message_size, 7);
            receiver.update(handler_r2s, max_message_size);
            receiver.handle_complete();
        }
    }
}
//------------------------------------------------------------------------------
// round-trip resend 2
//------------------------------------------------------------------------------
void blobs_roundtrip_resend_2(auto& s) {
    eagitest::case_ test{s, 9, "round-trip resend 2"};
    eagitest::track trck{test, 1, 5};
    auto& rg{test.random()};

    const eagine::message_id test_msg_id{eagine::random_identifier(), "test"};
    const eagine::message_id send_msg_id{"check", "send"};
    const eagine::message_id resend_msg_id{"check", "resend"};
    eagine::msgbus::blob_manipulator sender{
      s.context(), send_msg_id, resend_msg_id};
    eagine::msgbus::blob_manipulator receiver{
      s.context(), send_msg_id, resend_msg_id};

    auto send_s2r = [&](
                      const eagine::message_id msg_id,
                      const eagine::msgbus::message_view& message) -> bool {
        test.check(msg_id == send_msg_id, "message id");

        if(not rg.one_of(5)) {
            receiver.process_incoming(message);
            trck.checkpoint(1);
        }
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_s2r{
      eagine::construct_from, send_s2r};

    auto send_r2s = [&](
                      const eagine::message_id msg_id,
                      const eagine::msgbus::message_view& message) -> bool {
        if(not rg.one_of(11)) {
            if(msg_id == resend_msg_id) {
                sender.process_resend(message);
                trck.checkpoint(5);
            }
        }
        return true;
    };
    const eagine::msgbus::blob_manipulator::send_handler handler_r2s{
      eagine::construct_from, send_r2s};

    const auto todo{test.repeats(4)};
    unsigned done{0};

    for(unsigned r = 0; r < todo; ++r) {
        sender.push_outgoing(
          test_msg_id,
          1,
          0,
          eagine::msgbus::blob_id_t(r),
          {eagine::hold<ces_source_blob_io>, 32 * 1024},
          std::chrono::hours{1},
          eagine::msgbus::message_priority::normal);

        receiver.expect_incoming(
          test_msg_id,
          1,
          eagine::msgbus::blob_id_t(r),
          {eagine::hold<ces_target_blob_io>, test, trck, 32 * 1024, done},
          std::chrono::hours{1});
    }

    const eagine::span_size_t max_message_size{1024};
    while(done < todo) {
        sender.update(handler_s2r, max_message_size);
        sender.process_outgoing(handler_s2r, max_message_size, 9);
        receiver.update(handler_r2s, max_message_size);
        receiver.handle_complete();
    }
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    eagitest::ctx_suite test{ctx, "blobs", 9};
    test.once(blobs_roundtrip_zeroes_single_big);
    test.repeat(5, blobs_roundtrip_zeroes_single);
    test.once(blobs_roundtrip_bfs_single);
    test.once(blobs_roundtrip_ces_multiple);
    test.once(blobs_roundtrip_chunk_signals_finished);
    test.once(blobs_roundtrip_stream_signals_finished);
    test.once(blobs_roundtrip_chunk_signals_failed);
    test.once(blobs_roundtrip_resend_1);
    test.once(blobs_roundtrip_resend_2);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>
