/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module eagine.msgbus.services;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.msgbus.core;
import std;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
class shutdown_target_impl
  : public shutdown_target_intf
  , protected shutdown_service_clock {
public:
    shutdown_target_impl(subscriber& sub, shutdown_target_signals& sigs) noexcept
      : base{sub}
      , signals{sigs} {}

    void add_methods() noexcept final {
        base.add_method(
          this,
          message_map<
            "Shutdown",
            "shutdown",
            &shutdown_target_impl::_handle_shutdown>{});
    }

private:
    auto _handle_shutdown(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        typename shutdown_service_duration::rep count{0};
        if(default_deserialize(count, message.content())) {
            const shutdown_service_duration ticks{count};
            const typename shutdown_service_clock::time_point ts{ticks};
            const auto age{this->now() - ts};
            signals.shutdown_requested(
              std::chrono::duration_cast<std::chrono::milliseconds>(age),
              message.source_id,
              base.verify_bits(message));
        }
        return true;
    }

    subscriber& base;
    shutdown_target_signals& signals;
};
//------------------------------------------------------------------------------
auto make_shutdown_target_impl(subscriber& base, shutdown_target_signals& sigs)
  -> std::unique_ptr<shutdown_target_intf> {
    return std::make_unique<shutdown_target_impl>(base, sigs);
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
