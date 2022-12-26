/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus.core:skeleton;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.serialization;
import eagine.core.utility;
import eagine.core.runtime;
import :types;
import :message;
import :handler_map;
import :endpoint;
import <array>;
export import <map>;
import <tuple>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export template <
  typename Signature,
  typename Serializer,
  typename Deserializer,
  typename Sink,
  typename Source,
  std::size_t MaxDataSize>
class skeleton {
public:
    auto call(
      const message_context& msg_ctx,
      const stored_message& request,
      const message_id response_id,
      memory::block buffer,
      const callable_ref<Signature> func) -> bool {
        return _do_call(
          msg_ctx, request, response_id, buffer, func, func.argument_tuple());
    }

    auto call(
      const message_context& msg_ctx,
      const stored_message& request,
      const message_id response_id,
      const callable_ref<Signature> func) -> bool {
        std::array<byte, MaxDataSize> buffer{};
        return call(msg_ctx, request, response_id, cover(buffer), func);
    }

private:
    template <typename... Params>
    auto _do_call(
      const message_context& msg_ctx,
      const stored_message& request,
      const message_id response_id,
      memory::block buffer,
      const callable_ref<Signature> func,
      std::tuple<Params...> args) -> bool {

        _source.reset(request.content());
        Deserializer read_backend(_source);

        if(request.has_serializer_id(read_backend.type_id())) [[likely]] {
            if(deserialize(args, read_backend)) [[likely]] {
                _sink.reset(buffer);
                Serializer write_backend(_sink);

                if(serialize(std::apply(func, args), write_backend))
                  [[likely]] {
                    message_view msg_out{_sink.done()};
                    msg_out.set_serializer_id(write_backend.type_id());
                    msg_ctx.bus_node().respond_to(
                      request, response_id, msg_out);
                }
                return true;
            }
        }
        return false;
    }

    auto _do_call(
      const message_context& msg_ctx,
      const stored_message& request,
      const message_id response_id,
      memory::block buffer,
      const callable_ref<Signature> func,
      std::tuple<>) -> bool {

        _sink.reset(buffer);
        Serializer write_backend(_sink);

        if(serialize(func(), write_backend)) [[likely]] {
            message_view msg_out{_sink.done()};
            msg_out.set_serializer_id(write_backend.type_id());
            msg_ctx.bus_node().respond_to(request, response_id, msg_out);
        }
        return true;
    }

private:
    Source _source{};
    Sink _sink{};
};
//------------------------------------------------------------------------------
export template <
  typename Signature,
  typename Serializer,
  typename Deserializer,
  typename Sink,
  typename Source,
  std::size_t MaxDataSize>
class function_skeleton
  : public skeleton<Signature, Serializer, Deserializer, Sink, Source, MaxDataSize> {

    using _function_t = basic_callable_ref<Signature, true>;

public:
    function_skeleton() noexcept = default;

    function_skeleton(message_id response_id, _function_t function) noexcept
      : _response_id{std::move(response_id)}
      , _function{std::move(function)} {}

    auto operator()(
      const message_id response_id,
      const _function_t function) noexcept -> function_skeleton& {
        _response_id = response_id;
        _function = function;
        return *this;
    }

    template <typename Class, typename MfcT, MfcT Mfc>
    auto operator()(
      const message_id response_id,
      Class* that,
      const member_function_constant<MfcT, Mfc> func) noexcept
      -> function_skeleton& {
        _response_id = response_id;
        _function = _function_t{that, func};
        return *this;
    }

    template <typename Class, typename MfcT, MfcT Mfc>
    auto operator()(
      const message_id response_id,
      const Class* that,
      const member_function_constant<MfcT, Mfc> func) noexcept
      -> function_skeleton& {
        _response_id = response_id;
        _function = _function_t{that, func};
        return *this;
    }

    auto invoke_by(
      const message_context& msg_ctx,
      const stored_message& request) noexcept -> bool {
        return this->call(msg_ctx.bus_node(), request, _response_id, _function);
    }

    constexpr auto map_invoke_by(const message_id msg_id) noexcept {
        return std::tuple<
          function_skeleton*,
          message_handler_map<
            member_function_constant_t<&function_skeleton::invoke_by>>>(
          this, msg_id);
    }

    constexpr auto operator[](const message_id msg_id) noexcept {
        return map_invoke_by(msg_id);
    }

private:
    message_id _response_id{};
    _function_t _function{};
};
//------------------------------------------------------------------------------
export template <
  typename Signature,
  typename Serializer,
  typename Deserializer,
  typename Sink,
  typename Source,
  std::size_t MaxDataSize>
class lazy_skeleton {
    using argument_tuple_type =
      typename callable_ref<Signature>::argument_tuple_type;

public:
    using id_t = message_sequence_t;

    lazy_skeleton() noexcept = default;

    template <typename R, typename P>
    lazy_skeleton(const std::chrono::duration<R, P> default_timeout) noexcept
      : _default_timeout{default_timeout} {}

    auto enqueue(
      const stored_message& request,
      const message_id response_id,
      const callable_ref<Signature> func) -> bool {
        auto [pos, emplaced] = _pending.try_emplace(request.sequence_no);

        if(emplaced) {
            if constexpr(std::tuple_size_v < argument_tuple_type >> 0) {
                _source.reset(request.content());
                Deserializer read_backend(_source);

                if(request.has_serializer_id(read_backend.type_id()))
                  [[likely]] {
                    auto& call = pos->second;
                    if(deserialize(call.args, read_backend)) [[likely]] {
                        call.too_late.reset(_default_timeout);
                        call.response_id = response_id;
                        call.invoker_id = request.source_id;
                        call.func = func;
                        return true;
                    }
                }
                _pending.erase(pos);
            } else {
                auto& call = pos->second;
                call.too_late.reset(_default_timeout);
                call.response_id = response_id;
                call.invoker_id = request.source_id;
                call.func = func;
                return true;
            }
        }
        return false;
    }

    auto handle_one(endpoint& bus, memory::block buffer) -> bool {
        const auto bgn = _pending.begin();
        auto pos = bgn;
        while(pos != _pending.end()) {
            const auto invocation_id = pos->first;
            const auto& call = pos->second;
            ++pos;
            if(not call.too_late) {
                _sink.reset(cover(buffer));
                Serializer write_backend(_sink);

                if(serialize(std::apply(call.func, call.args), write_backend))
                  [[likely]] {
                    message_view msg_out{_sink.done()};
                    msg_out.set_serializer_id(write_backend.type_id());
                    msg_out.set_target_id(call.invoker_id);
                    msg_out.set_sequence_no(invocation_id);
                    bus.post(call.response_id, msg_out);
                }
                break;
            }
        }
        if(bgn != pos) {
            _pending.erase(bgn, pos);
            return true;
        }
        return false;
    }

    auto handle_one(endpoint& bus) -> bool {
        std::array<byte, MaxDataSize> buffer{};
        return handle_one(bus, cover(buffer));
    }

private:
    const std::chrono::milliseconds _default_timeout{
      adjusted_duration(std::chrono::milliseconds{1000})};
    Source _source{};
    Sink _sink{};

    struct lazy_call {
        message_id response_id{};
        argument_tuple_type args{};
        callable_ref<Signature> func{};
        timeout too_late{};
        identifier_t invoker_id{};
    };

    flat_map<id_t, lazy_call> _pending{};
};
//------------------------------------------------------------------------------
export template <
  typename Signature,
  typename Serializer,
  typename Deserializer,
  typename Sink,
  typename Source,
  std::size_t MaxDataSize>
class async_skeleton {
    using argument_tuple_type =
      typename callable_ref<Signature>::argument_tuple_type;

public:
    using id_t = message_sequence_t;

    async_skeleton() noexcept = default;

    auto enqueue(
      const stored_message& request,
      const message_id response_id,
      const callable_ref<Signature> func,
      workshop& workers) -> bool {
        auto [pos, emplaced] = _pending.try_emplace(request.sequence_no);

        if(emplaced) {
            if constexpr(std::tuple_size_v<argument_tuple_type>) {
                _source.reset(request.content());
                Deserializer read_backend(_source);

                if(request.has_serializer_id(read_backend.type_id()))
                  [[likely]] {
                    auto& call = pos->second;
                    if(deserialize(call.args, read_backend)) [[likely]] {
                        call.response_id = response_id;
                        call.invoker_id = request.source_id;
                        call.func = func;
                        workers.enqueue(call);
                        return true;
                    }
                }
                _pending.erase(pos);
            } else {
                auto& call = pos->second;
                call.response_id = response_id;
                call.invoker_id = request.source_id;
                call.func = func;
                workers.enqueue(call);
                return true;
            }
        }
        return false;
    }

    auto handle_one(endpoint& bus, memory::block buffer) -> bool {
        const auto bgn = _pending.begin();
        auto pos = bgn;
        while(pos != _pending.end()) {
            const auto invocation_id = pos->first;
            const auto& call = pos->second;
            if(call.finished.load()) {
                _sink.reset(buffer);
                Serializer write_backend(_sink);

                if(serialize(call.result, write_backend)) [[likely]] {
                    message_view msg_out{_sink.done()};
                    msg_out.set_serializer_id(write_backend.type_id());
                    msg_out.set_target_id(call.invoker_id);
                    msg_out.set_sequence_no(invocation_id);
                    bus.post(call.response_id, msg_out);
                }
                _pending.erase(pos);
                return true;
            }
            ++pos;
        }
        return false;
    }

    auto handle_one(endpoint& bus) -> bool {
        std::array<byte, MaxDataSize> buffer{};
        return handle_one(bus, cover(buffer));
    }

private:
    Source _source{};
    Sink _sink{};

    struct async_call : work_unit {
        message_id response_id{};
        argument_tuple_type args{};
        callable_ref<Signature> func{};

        using result_type = std::remove_cv_t<
          std::remove_reference_t<decltype(std::apply(func, args))>>;
        result_type result{};

        identifier_t invoker_id{};
        std::atomic<bool> finished{false};

        auto do_it() noexcept -> bool final {
            result = std::apply(func, args);
            return true;
        }

        void deliver() noexcept final {
            finished.store(true);
        }
    };

    std::map<id_t, async_call> _pending{};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

