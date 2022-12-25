/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/testing/unit_begin_ctx.hpp>
#include <memory>
import eagine.core;
import eagine.msgbus.core;
import <atomic>;
import <functional>;
import <vector>;
import <mutex>;
import <thread>;
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
    auto cacc{std::dynamic_pointer_cast<eagine::msgbus::direct_acceptor_intf>(
      std::shared_ptr<eagine::msgbus::acceptor>(
        fact->make_acceptor(eagine::identifier{"test"})))};
    test.ensure(bool(cacc), "has acceptor");
    auto read_conn{cacc->make_connection()};
    test.ensure(bool(read_conn), "has read connection");

    std::unique_ptr<eagine::msgbus::connection> write_conn;
    test.check(not bool(write_conn), "has not write connection");

    cacc->process_accepted(
      {eagine::construct_from,
       [&](std::unique_ptr<eagine::msgbus::connection> conn) {
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
        trck.passed_part(1);
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
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    eagitest::ctx_suite test{ctx, "direct connection", 3};
    test.once(direct_type_id);
    test.once(direct_addr_kind);
    test.once(direct_roundtrip);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>
