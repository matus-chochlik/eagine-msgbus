/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/testing/unit_begin.hpp>
import eagine.core;
import eagine.msgbus.core;
import <atomic>;
import <functional>;
import <vector>;
import <mutex>;
import <thread>;
//------------------------------------------------------------------------------
void loopback_type_id(auto& s) {
    eagitest::case_ test{s, 1, "type id"};
    eagine::msgbus::loopback_connection conn;

    test.check(not conn.type_id().is_empty(), "has name");
}
//------------------------------------------------------------------------------
void loopback_addr_kind(auto& s) {
    eagitest::case_ test{s, 2, "addr kind"};
    eagine::msgbus::loopback_connection conn;

    test.check(
      conn.addr_kind() == eagine::msgbus::connection_addr_kind::none,
      "no address");
}
//------------------------------------------------------------------------------
void loopback_roundtrip(auto& s) {
    eagitest::case_ test{s, 3, "roundtrip"};
    eagitest::track trck{test, 0, 1};
    auto& rg{test.random()};

    const eagine::message_id test_msg_id{"test", "message"};

    std::map<eagine::msgbus::message_sequence_t, std::size_t> hashes;
    std::vector<eagine::byte> src;

    eagine::msgbus::loopback_connection conn;
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
            conn.send(test_msg_id, message);
            std::size_t h{0};
            for(const auto b : src) {
                h ^= std::hash<eagine::byte>{}(b);
            }
            hashes[seq] = h;
            ++seq;
        }
        if(rg.get_bool()) {
            conn.fetch_messages({eagine::construct_from, read_func});
        }
    }
    conn.fetch_messages({eagine::construct_from, read_func});
    test.check(hashes.empty(), "all hashes checked");
}
//------------------------------------------------------------------------------
void loopback_roundtrip_threads(auto& s) {
    eagitest::case_ test{s, 4, "roundtrip threads"};
    eagitest::track trck{test, 0, 1};
    auto& rg{test.random()};

    const eagine::message_id test_msg_id{"test", "message"};

    std::map<eagine::msgbus::message_sequence_t, std::size_t> hashes;
    std::vector<eagine::byte> src;
    std::atomic<std::size_t> send_count{0};
    std::atomic<bool> send_done{false};

    eagine::msgbus::loopback_connection conn;

    std::mutex sync;
    std::thread reader{[&]() {
        while(not send_done or (send_count > 0)) {
            conn.fetch_messages(
              {eagine::construct_from,
               [&](
                 const eagine::message_id msg_id,
                 const eagine::msgbus::message_age,
                 const eagine::msgbus::message_view& msg) -> bool {
                   std::size_t h{0};
                   for(const auto b : msg.content()) {
                       h ^= std::hash<eagine::byte>{}(b);
                   }
                   trck.passed_part(1);

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
            conn.send(test_msg_id, message);
        }
    }
    send_done = true;
    reader.join();
    test.check_equal(hashes.size(), std::size_t(0), "all hashes checked");
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    eagitest::suite test{argc, argv, "loopback", 4};
    test.once(loopback_type_id);
    test.once(loopback_addr_kind);
    test.once(loopback_roundtrip);
    test.once(loopback_roundtrip_threads);
    return test.exit_code();
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end.hpp>
