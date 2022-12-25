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
import <chrono>;
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

    eagine::msgbus::message_sequence_t sequence_no{0};
    for(const auto& info : eagine::enumerator_mapping(
          std::type_identity<eagine::msgbus::message_priority>{},
          eagine::default_selector)) {
        eagine::block_data_sink sink{eagine::cover(buffer)};

        eagine::msgbus::message_view message;
        message.set_sequence_no(sequence_no);
        message.set_priority(info.enumerator);
        message.add_age(std::chrono::seconds{1});
        eagine::msgbus::default_serializer_backend write_backend{sink};

        const auto serialized{eagine::msgbus::serialize_message_header(
          msg_id, message, write_backend)};
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
        test.check(dest.sequence_no == sequence_no, "sequence ok");
        test.check(dest.priority == info.enumerator, "priority ok");
        test.check(dest.age() >= std::chrono::seconds{1}, "age ok");

        ++sequence_no;
    }
}
//------------------------------------------------------------------------------
void message_serialize_header_roundtrip(auto& s) {
    eagitest::case_ test{s, 3, "serialize header round-trip"};

    message_serialize_header_roundtrip_m(test, {"some", "message"});
    message_serialize_header_roundtrip_m(test, {"other", "operation"});
    message_serialize_header_roundtrip_m(test, {"another", "operation"});
}
//------------------------------------------------------------------------------
// serialize message roundtrip
//------------------------------------------------------------------------------
void message_serialize_message_roundtrip_m(
  eagitest::case_& test,
  eagine::message_id msg_id) {

    std::vector<eagine::byte> buffer{};
    buffer.resize(2048);
    auto& rg{test.random()};

    std::vector<eagine::byte> content{};

    eagine::msgbus::message_sequence_t sequence_no{0};
    for(unsigned i = 0; i < test.repeats(1000); ++i) {
        for(const auto& info : eagine::enumerator_mapping(
              std::type_identity<eagine::msgbus::message_priority>{},
              eagine::default_selector)) {
            eagine::block_data_sink sink{eagine::cover(buffer)};

            content.resize(rg.get_between<std::size_t>(0, 1280));
            rg.fill(content);

            eagine::msgbus::message_view message{eagine::view(content)};
            const auto age{rg.get_between(
              eagine::msgbus::message_age{1}, eagine::msgbus::message_age{25})};
            message.set_sequence_no(sequence_no);
            message.set_priority(info.enumerator);
            message.add_age(age);
            eagine::msgbus::default_serializer_backend write_backend{sink};

            const auto serialized{eagine::msgbus::serialize_message(
              msg_id, message, write_backend)};
            test.ensure(bool(serialized), "serialized");

            eagine::block_data_source source{sink.done()};
            eagine::msgbus::default_deserializer_backend read_backend{source};
            eagine::identifier class_{};
            eagine::identifier method{};
            eagine::msgbus::stored_message dest;

            const auto deserialized{eagine::msgbus::deserialize_message(
              class_, method, dest, read_backend)};
            test.ensure(bool(deserialized), "deserialized");

            test.check(msg_id.class_() == class_, "class ok");
            test.check(msg_id.method() == method, "method ok");
            test.check_equal(
              eagine::view(content).size(),
              dest.content().size(),
              "content size ok");
            test.check(
              eagine::are_equal(eagine::view(content), dest.const_content()),
              "content ok");
            test.check(dest.sequence_no == sequence_no, "sequence ok");
            test.check(dest.priority == info.enumerator, "priority ok");
            test.check(dest.age() >= age, "age ok");
        }
    }
}
//------------------------------------------------------------------------------
void message_serialize_message_roundtrip(auto& s) {
    eagitest::case_ test{s, 4, "serialize message round-trip"};

    message_serialize_message_roundtrip_m(test, {"some", "message"});
    message_serialize_message_roundtrip_m(test, {"other", "message"});
    message_serialize_message_roundtrip_m(test, {"another", "operation"});
}
//------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------
auto test_main(eagine::test_ctx& ctx) -> int {
    eagitest::ctx_suite test{ctx, "message", 4};
    test.once(message_valid_endpoint_id);
    test.once(message_is_special);
    test.once(message_serialize_header_roundtrip);
    test.once(message_serialize_message_roundtrip);
    return test.exit_code();
}
//------------------------------------------------------------------------------
auto main(int argc, const char** argv) -> int {
    return eagine::test_main_impl(argc, argv, test_main);
}
//------------------------------------------------------------------------------
#include <eagine/testing/unit_end.hpp>