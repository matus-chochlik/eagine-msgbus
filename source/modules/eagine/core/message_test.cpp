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
import <array>;
//------------------------------------------------------------------------------
// valid endpoint id
//------------------------------------------------------------------------------
void message_valid_endpoint_id(auto& s) {
    eagitest::case_ test{s, 1, "endpoint id"};
    test.check(
      not eagine::msgbus::is_valid_endpoint_id(
        eagine::msgbus::invalid_endpoint_id()),
      "invalid");
    test.check(eagine::msgbus::is_valid_endpoint_id(1), "1");
    test.check(eagine::msgbus::is_valid_endpoint_id(2), "2");
    test.check(eagine::msgbus::is_valid_endpoint_id(8), "8");
    test.check(eagine::msgbus::is_valid_endpoint_id(16), "16");
    test.check(eagine::msgbus::is_valid_endpoint_id(128), "128");
    test.check(eagine::msgbus::is_valid_endpoint_id(1024), "1024");
    test.check(eagine::msgbus::is_valid_endpoint_id(1024 * 1024), "1024^2");
}
//------------------------------------------------------------------------------
// is special
//------------------------------------------------------------------------------
void message_is_special(auto& s) {
    eagitest::case_ test{s, 2, "is special"};

    test.check(
      eagine::msgbus::is_special_message(eagine::msgbus::msgbus_id{"test1"}),
      "test1");
    test.check(
      eagine::msgbus::is_special_message(eagine::msgbus::msgbus_id{"test2"}),
      "test2");
    test.check(
      eagine::msgbus::is_special_message(eagine::msgbus::msgbus_id{"test3"}),
      "test3");
    test.check(
      eagine::msgbus::is_special_message({"eagiMsgBus", "ping"}), "ping");
    test.check(
      eagine::msgbus::is_special_message({"eagiMsgBus", "pong"}), "pong");
    test.check(
      not eagine::msgbus::is_special_message({"some", "message"}), "some");
    test.check(
      not eagine::msgbus::is_special_message({"other", "message"}), "other");
}
//------------------------------------------------------------------------------
// serialize header roundtrip
//------------------------------------------------------------------------------
void message_serialize_header_roundtrip_m(
  eagitest::case_& test,
  eagine::message_id msg_id) {
    std::array<eagine::byte, 128> buffer{};
    eagine::block_data_sink sink{eagine::cover(buffer)};

    eagine::msgbus::message_view message;
    eagine::msgbus::default_serializer_backend write_backend{sink};

    const auto serialized{
      eagine::msgbus::serialize_message_header(msg_id, message, write_backend)};
    test.ensure(bool(serialized), "serialized");

    eagine::block_data_source source{sink.done()};
    eagine::msgbus::default_deserializer_backend read_backend{source};
    eagine::identifier class_{};
    eagine::identifier method{};
    eagine::msgbus::stored_message dest;

    const auto deserialized{eagine::msgbus::deserialize_message_header(
      class_, method, dest, read_backend)};
    test.ensure(bool(deserialized), "deserialized");

    test.check(msg_id.class_() == class_, "class ok");
    test.check(msg_id.method() == method, "method ok");
}
//------------------------------------------------------------------------------
void message_serialize_header_roundtrip(auto& s) {
    eagitest::case_ test{s, 3, "serialize header round-trip"};

    message_serialize_header_roundtrip_m(test, {"some", "message"});
    message_serialize_header_roundtrip_m(test, {"other", "operation"});
    message_serialize_header_roundtrip_m(test, {"another", "operation"});
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    eagitest::ctx_suite test{ctx, "message", 3};
    test.once(message_valid_endpoint_id);
    test.once(message_is_special);
    test.once(message_serialize_header_roundtrip);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end.hpp>
