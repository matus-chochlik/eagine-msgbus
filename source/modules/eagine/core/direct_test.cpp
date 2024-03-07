/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/testing/unit_begin_ctx.hpp>
import std;
import eagine.core;
import eagine.msgbus.core;
//------------------------------------------------------------------------------
void direct_type_id(auto& s) {
    eagitest::case_ test{s, 1, "type id"};
    auto cacc{eagine::msgbus::make_direct_acceptor(s.context())};
    test.ensure(bool(cacc), "has acceptor");
    auto conn{cacc->make_connection()};
    test.ensure(bool(conn), "has connection");

    test.check(not conn->type_id().is_empty(), "has name");
}
//------------------------------------------------------------------------------
void direct_addr_kind(auto& s) {
    eagitest::case_ test{s, 2, "addr kind"};
    auto cacc{eagine::msgbus::make_direct_acceptor(s.context())};
    test.ensure(bool(cacc), "has acceptor");
    auto conn{cacc->make_connection()};
    test.ensure(bool(conn), "has connection");

    test.check(
      conn->addr_kind() == eagine::msgbus::connection_addr_kind::none,
      "no address");
}
//------------------------------------------------------------------------------
void direct_roundtrip(auto& s) {
    eagitest::case_ test{s, 3, "roundtrip"};
    eagitest::track trck{test, 0, 1};
    auto& rg{test.random()};

    auto fact{eagine::msgbus::make_direct_connection_factory(s.context())};
    test.ensure(bool(fact), "has factory");
    auto cacc{
      fact->make_acceptor(eagine::identifier{"test"})
        .as(std::type_identity<eagine::msgbus::direct_acceptor_intf>{})};
    test.ensure(bool(cacc), "has acceptor");
    auto read_conn{cacc->make_connection()};
    test.ensure(bool(read_conn), "has read connection");

    eagine::shared_holder<eagine::msgbus::connection> write_conn;
    test.check(not bool(write_conn), "has not write connection");

    cacc->process_accepted(
      {eagine::construct_from,
       [&](eagine::shared_holder<eagine::msgbus::connection> conn) {
           write_conn = std::move(conn);
       }});
    test.ensure(bool(write_conn), "has write connection");

    const eagine::message_id test_msg_id{"test", "method"};

    std::map<eagine::msgbus::message_sequence_t, std::size_t> hashes;
    std::vector<eagine::byte> src;

    eagine::msgbus::message_sequence_t seq{0};

    const auto read_func = [&](
                             const eagine::message_id msg_id,
                             const eagine::msgbus::message_age,
                             const eagine::msgbus::message_view& msg) -> bool {
        test.check(msg_id == test_msg_id, "message id");
        std::size_t h{0};
        for(const auto b : msg.content()) {
            h ^= std::hash<eagine::byte>{}(b);
        }
        test.check_equal(h, hashes[msg.sequence_no], "same hash");
        hashes.erase(msg.sequence_no);
        trck.checkpoint(1);
        return true;
    };

    for(unsigned r = 0; r < test.repeats(1000); ++r) {
        for(unsigned i = 0, n = rg.get_between<unsigned>(0, 20); i < n; ++i) {
            src.resize(rg.get_std_size(0, 1024));
            rg.fill(src);

            eagine::msgbus::message_view message{eagine::view(src)};
            message.set_sequence_no(seq);
            write_conn->send(test_msg_id, message);
            std::size_t h{0};
            for(const auto b : src) {
                h ^= std::hash<eagine::byte>{}(b);
            }
            hashes[seq] = h;
            ++seq;
        }
        if(rg.get_bool()) {
            read_conn->fetch_messages({eagine::construct_from, read_func});
        }
    }
    read_conn->fetch_messages({eagine::construct_from, read_func});
    test.check(hashes.empty(), "all hashes checked");
}
//------------------------------------------------------------------------------
void direct_roundtrip_thread(auto& s) {
    eagitest::case_ test{s, 4, "roundtrip thread"};
    eagitest::track trck{test, 0, 1};
    auto& rg{test.random()};

    auto fact{eagine::msgbus::make_direct_connection_factory(s.context())};
    test.ensure(bool(fact), "has factory");
    auto cacc{
      fact->make_acceptor(eagine::identifier{"test"})
        .as(std::type_identity<eagine::msgbus::direct_acceptor_intf>{})};
    test.ensure(bool(cacc), "has acceptor");
    auto read_conn{cacc->make_connection()};
    test.ensure(bool(read_conn), "has read connection");

    eagine::shared_holder<eagine::msgbus::connection> write_conn;
    test.check(not bool(write_conn), "has not write connection");

    cacc->process_accepted(
      {eagine::construct_from,
       [&](eagine::shared_holder<eagine::msgbus::connection> conn) {
           write_conn = std::move(conn);
       }});
    test.ensure(bool(write_conn), "has write connection");

    const eagine::message_id test_msg_id{"test", "method"};

    std::map<eagine::msgbus::message_sequence_t, std::size_t> hashes;
    std::vector<eagine::byte> src;
    std::atomic<std::size_t> send_count{0};
    std::atomic<bool> send_done{false};

    std::mutex sync;
    std::thread reader{[&] {
        while(not send_done or (send_count > 0)) {
            read_conn->fetch_messages(
              {eagine::construct_from,
               [&](
                 const eagine::message_id msg_id,
                 const eagine::msgbus::message_age,
                 const eagine::msgbus::message_view& msg) -> bool {
                   std::size_t h{0};
                   for(const auto b : msg.content()) {
                       h ^= std::hash<eagine::byte>{}(b);
                   }
                   trck.checkpoint(1);

                   const std::lock_guard<std::mutex> lock{sync};
                   test.check(msg_id == test_msg_id, "message id");
                   test.check_equal(h, hashes[msg.sequence_no], "same hash");
                   hashes.erase(msg.sequence_no);
                   --send_count;

                   return true;
               }});
            std::this_thread::sleep_for(std::chrono::milliseconds{25});
        }
    }};

    auto make_data = [&, seq{eagine::msgbus::message_sequence_t{0}}]() mutable {
        const std::lock_guard<std::mutex> lock{sync};
        src.resize(rg.get_std_size(0, 1024));
        rg.fill(src);
        std::size_t h{0};
        for(const auto b : src) {
            h ^= std::hash<eagine::byte>{}(b);
        }
        ++seq;
        hashes[seq] = h;
        return std::make_tuple(seq, eagine::view(src));
    };
    for(unsigned r = 0; r < test.repeats(10000); ++r) {
        for(unsigned i = 0, n = rg.get_between<unsigned>(0, 20); i < n; ++i) {
            const auto [seq, data] = make_data();

            eagine::msgbus::message_view message{data};
            message.set_sequence_no(seq);
            ++send_count;
            write_conn->send(test_msg_id, message);
        }
    }
    send_done = true;
    reader.join();
    test.check(hashes.empty(), "all hashes checked");
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    eagitest::ctx_suite test{ctx, "direct connection", 4};
    test.once(direct_type_id);
    test.once(direct_addr_kind);
    test.once(direct_roundtrip);
    test.once(direct_roundtrip_thread);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>
