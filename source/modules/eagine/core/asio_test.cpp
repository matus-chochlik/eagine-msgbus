/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/testing/unit_begin_ctx.hpp>
import eagine.core;
import eagine.msgbus.core;
import <atomic>;
import <functional>;
import <vector>;
import <chrono>;
import <memory>;
import <mutex>;
import <thread>;
//------------------------------------------------------------------------------
// type_id
//------------------------------------------------------------------------------
void asio_tcp_ipv4_type_id(auto& s) {
    eagitest::case_ test{s, 1, "type id TCP/IPv4"};
    auto fact{
      eagine::msgbus::make_asio_tcp_ipv4_connection_factory(s.context())};
    test.ensure(bool(fact), "has factory");
    auto cacc{fact->make_acceptor(eagine::identifier{"localhost"})};
    test.ensure(bool(cacc), "has acceptor");
    auto conn{fact->make_connector(eagine::identifier{"localhost"})};
    test.ensure(bool(conn), "has connection");

    test.check(not cacc->type_id().is_empty(), "has name");
    test.check(not conn->type_id().is_empty(), "has name");
}
//------------------------------------------------------------------------------
void asio_udp_ipv4_type_id(auto& s) {
    eagitest::case_ test{s, 2, "type id UDP/IPv4"};
    auto fact{
      eagine::msgbus::make_asio_udp_ipv4_connection_factory(s.context())};
    test.ensure(bool(fact), "has factory");
    auto cacc{fact->make_acceptor(eagine::identifier{"localhost"})};
    test.ensure(bool(cacc), "has acceptor");
    auto conn{fact->make_connector(eagine::identifier{"localhost"})};
    test.ensure(bool(conn), "has connection");

    test.check(not cacc->type_id().is_empty(), "has name");
    test.check(not conn->type_id().is_empty(), "has name");
}
//------------------------------------------------------------------------------
// address kind
//------------------------------------------------------------------------------
void asio_tcp_ipv4_addr_kind(auto& s) {
    eagitest::case_ test{s, 3, "addr kind TCP/IPv4"};
    auto fact{
      eagine::msgbus::make_asio_tcp_ipv4_connection_factory(s.context())};
    test.ensure(bool(fact), "has factory");
    auto cacc{fact->make_acceptor(eagine::identifier{"localhost"})};
    test.ensure(bool(cacc), "has acceptor");
    auto conn{fact->make_connector(eagine::identifier{"localhost"})};
    test.ensure(bool(conn), "has connection");

    test.check(
      cacc->addr_kind() == eagine::msgbus::connection_addr_kind::ipv4,
      "no address");
    test.check(
      conn->addr_kind() == eagine::msgbus::connection_addr_kind::ipv4,
      "no address");
}
//------------------------------------------------------------------------------
void asio_udp_ipv4_addr_kind(auto& s) {
    eagitest::case_ test{s, 4, "addr kind TCP/IPv4"};
    auto fact{
      eagine::msgbus::make_asio_udp_ipv4_connection_factory(s.context())};
    test.ensure(bool(fact), "has factory");
    auto cacc{fact->make_acceptor(eagine::identifier{"localhost"})};
    test.ensure(bool(cacc), "has acceptor");
    auto conn{fact->make_connector(eagine::identifier{"localhost"})};
    test.ensure(bool(conn), "has connection");

    test.check(
      cacc->addr_kind() == eagine::msgbus::connection_addr_kind::ipv4,
      "no address");
    test.check(
      conn->addr_kind() == eagine::msgbus::connection_addr_kind::ipv4,
      "no address");
}
//------------------------------------------------------------------------------
// roundtrip
//------------------------------------------------------------------------------
template <typename Fact>
void asio_roundtrip_F(
  eagitest::case_& test,
  Fact fact,
  eagine::string_view addr) {

    eagitest::track trck{test, 0, 1};
    auto& rg{test.random()};

    test.ensure(bool(fact), "has factory");
    auto cacc{fact->make_acceptor(addr)};
    test.ensure(bool(cacc), "has acceptor");
    auto read_conn{fact->make_connector(addr)};
    test.ensure(bool(read_conn), "has read connection");

    std::unique_ptr<eagine::msgbus::connection> write_conn;
    test.check(not bool(write_conn), "has not write connection");

    const eagine::timeout accept_time{std::chrono::seconds{5}};
    while(not write_conn) {
        read_conn->update();
        cacc->update();
        cacc->process_accepted(
          {eagine::construct_from,
           [&](std::unique_ptr<eagine::msgbus::connection> conn) {
               write_conn = std::move(conn);
           }});
        if(accept_time.is_expired()) {
            break;
        }
    }
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

    for(unsigned r = 0; r < test.repeats(100); ++r) {
        for(unsigned i = 0, n = rg.get_between<unsigned>(0, 20); i < n; ++i) {
            cacc->update();
            read_conn->update();
            write_conn->update();
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
        read_conn->update();
        write_conn->update();
        if(rg.get_bool()) {
            read_conn->fetch_messages({eagine::construct_from, read_func});
        }
    }
    read_conn->update();
    write_conn->update();
    read_conn->fetch_messages({eagine::construct_from, read_func});
}
//------------------------------------------------------------------------------
void asio_tcp_ipv4_roundtrip(auto& s) {
    eagitest::case_ test{s, 5, "roundtrip TCP/IPv4"};
    return asio_roundtrip_F(
      test,
      eagine::msgbus::make_asio_tcp_ipv4_connection_factory(s.context()),
      "localhost:34911");
}
//------------------------------------------------------------------------------
void asio_udp_ipv4_roundtrip(auto& s) {
    eagitest::case_ test{s, 6, "roundtrip UDP/IPv4"};
    return asio_roundtrip_F(
      test,
      eagine::msgbus::make_asio_udp_ipv4_connection_factory(s.context()),
      "localhost:34913");
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    eagitest::ctx_suite test{ctx, "asio connection", 5};
    test.once(asio_tcp_ipv4_type_id);
    test.once(asio_udp_ipv4_type_id);
    test.once(asio_tcp_ipv4_addr_kind);
    test.once(asio_udp_ipv4_addr_kind);
    test.once(asio_tcp_ipv4_roundtrip);
    // test.once(asio_udp_ipv4_roundtrip);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end_ctx.hpp>
