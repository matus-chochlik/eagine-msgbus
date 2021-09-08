/// @example eagine/msgbus/001_signal.cpp
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#include <eagine/msgbus/signal.hpp>
#include <iostream>

auto main() -> int {
    eagine::msgbus::signal<void(const int) noexcept> sig;
    eagine::callable_ref<void(const int) noexcept> f = sig;

    const auto fa = [](const int i) {
        std::cout << "A: " << i << std::endl;
    };
    const auto ka = sig.connect({eagine::construct_from, fa});
    f(1);

    const auto fb = [](const int i) {
        std::cout << "B: " << i << std::endl;
    };
    const auto kb = sig.connect({eagine::construct_from, fb});
    f(2);

    const auto fc = [](const int i) {
        std::cout << "C: " << i << std::endl;
    };
    const auto kc = sig.connect({eagine::construct_from, fc});
    f(3);

    sig.disconnect(ka);

    const auto fd = [](const int i) {
        std::cout << "D: " << i << std::endl;
    };
    const auto kd = sig.connect({eagine::construct_from, fd});
    f(4);

    sig.disconnect(ka);
    f(5);

    sig.disconnect(kc);
    f(6);

    sig.disconnect(kb);
    f(7);

    sig.disconnect(kd);
    f(8);

    const auto fe = [](int i) {
        std::cout << "E: " << i << std::endl;
    };
    if(auto be{sig.bind({eagine::construct_from, fe})}) {
        f(9);
    }

    return 0;
}
