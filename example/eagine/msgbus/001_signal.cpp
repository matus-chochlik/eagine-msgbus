/// @example eagine/msgbus/001_signal.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
import eagine.core;
import eagine.msgbus;

namespace eagine {

auto main(main_ctx& ctx) -> int {
    signal<void(const int) noexcept> sig;
    callable_ref<void(const int) noexcept> f = sig;

    const auto fa = [&](const int i) {
        ctx.cio().print(identifier{"MsgBus"}, "A: ${i}").arg(identifier{"i"}, i);
    };
    const auto ka = sig.connect({construct_from, fa});
    f(1);

    const auto fb = [&](const int i) {
        ctx.cio().print(identifier{"MsgBus"}, "B: ${i}").arg(identifier{"i"}, i);
    };
    const auto kb = sig.connect({construct_from, fb});
    f(2);

    const auto fc = [&](const int i) {
        ctx.cio().print(identifier{"MsgBus"}, "C: ${i}").arg(identifier{"i"}, i);
    };
    const auto kc = sig.connect({construct_from, fc});
    f(3);

    sig.disconnect(ka);

    const auto fd = [&](const int i) {
        ctx.cio().print(identifier{"MsgBus"}, "D: ${i}").arg(identifier{"i"}, i);
    };
    const auto kd = sig.connect({construct_from, fd});
    f(4);

    sig.disconnect(ka);
    f(5);

    sig.disconnect(kc);
    f(6);

    sig.disconnect(kb);
    f(7);

    sig.disconnect(kd);
    f(8);

    const auto fe = [&](int i) {
        ctx.cio().print(identifier{"MsgBus"}, "E: ${i}").arg(identifier{"i"}, i);
    };
    if(auto be{sig.bind({construct_from, fe})}) {
        f(9);
    }

    return 0;
}
} // namespace eagine

auto main(int argc, const char** argv) -> int {
    return eagine::default_main(argc, argv, eagine::main);
}

