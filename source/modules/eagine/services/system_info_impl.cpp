/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module eagine.msgbus.services;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.units;
import eagine.core.valid_if;
import eagine.core.main_ctx;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
class system_info_provider_impl : public system_info_provider_intf {
public:
    void add_methods(subscriber& base) noexcept final {
        base.add_method(_uptime(
                          {"eagiSysInf", "uptime"},
                          &main_ctx::get().system(),
                          member_function_constant_t<&system_info::uptime>{})
                          .map_invoke_by({"eagiSysInf", "rqUptime"}));

        base.add_method(
          _cpu_concurrent_threads(
            {"eagiSysInf", "cpuThreads"},
            &main_ctx::get().system(),
            member_function_constant_t<&system_info::cpu_concurrent_threads>{})
            .map_invoke_by({"eagiSysInf", "rqCpuThrds"}));

        base.add_method(
          _short_average_load(
            {"eagiSysInf", "shortLoad"},
            &main_ctx::get().system(),
            member_function_constant_t<&system_info::short_average_load>{})
            .map_invoke_by({"eagiSysInf", "rqShrtLoad"}));

        base.add_method(
          _long_average_load(
            {"eagiSysInf", "longLoad"},
            &main_ctx::get().system(),
            member_function_constant_t<&system_info::long_average_load>{})
            .map_invoke_by({"eagiSysInf", "rqLongLoad"}));

        base.add_method(
          _memory_page_size(
            {"eagiSysInf", "memPageSz"},
            &main_ctx::get().system(),
            member_function_constant_t<&system_info::memory_page_size>{})
            .map_invoke_by({"eagiSysInf", "rqMemPgSz"}));

        base.add_method(
          _free_ram_size(
            {"eagiSysInf", "freeRamSz"},
            &main_ctx::get().system(),
            member_function_constant_t<&system_info::free_ram_size>{})
            .map_invoke_by({"eagiSysInf", "rqFreRamSz"}));

        base.add_method(
          _total_ram_size(
            {"eagiSysInf", "totalRamSz"},
            &main_ctx::get().system(),
            member_function_constant_t<&system_info::total_ram_size>{})
            .map_invoke_by({"eagiSysInf", "rqTtlRamSz"}));

        base.add_method(
          _free_swap_size(
            {"eagiSysInf", "freeSwpSz"},
            &main_ctx::get().system(),
            member_function_constant_t<&system_info::free_swap_size>{})
            .map_invoke_by({"eagiSysInf", "rqFreSwpSz"}));

        base.add_method(
          _total_swap_size(
            {"eagiSysInf", "totalSwpSz"},
            &main_ctx::get().system(),
            member_function_constant_t<&system_info::total_swap_size>{})
            .map_invoke_by({"eagiSysInf", "rqTtlSwpSz"}));

        base.add_method(
          _temperature_min_max(
            {"eagiSysInf", "tempMinMax"},
            &main_ctx::get().system(),
            member_function_constant_t<&system_info::temperature_min_max>{})
            .map_invoke_by({"eagiSysInf", "rqTempMnMx"}));

        base.add_method(
          _power_supply_kind(
            {"eagiSysInf", "powerSuply"},
            &main_ctx::get().system(),
            member_function_constant_t<&system_info::power_supply>{})
            .map_invoke_by({"eagiSysInf", "rqPwrSuply"}));

        base.add_method(
          this,
          message_map<
            "eagiSysInf",
            "qryStats",
            &system_info_provider_impl::_handle_stats_query>{});

        base.add_method(
          this,
          message_map<
            "eagiSysInf",
            "qrySensors",
            &system_info_provider_impl::_handle_sensor_query>{});
    }

private:
    default_function_skeleton<std::chrono::duration<float>() noexcept, 32>
      _uptime;

    default_function_skeleton<valid_if_positive<span_size_t>() noexcept, 32>
      _cpu_concurrent_threads;

    default_function_skeleton<valid_if_nonnegative<float>() noexcept, 32>
      _short_average_load;

    default_function_skeleton<valid_if_nonnegative<float>() noexcept, 32>
      _long_average_load;

    default_function_skeleton<valid_if_positive<span_size_t>() noexcept, 32>
      _memory_page_size;

    default_function_skeleton<valid_if_positive<span_size_t>() noexcept, 32>
      _free_ram_size;

    default_function_skeleton<valid_if_positive<span_size_t>() noexcept, 32>
      _total_ram_size;

    default_function_skeleton<valid_if_nonnegative<span_size_t>() noexcept, 32>
      _free_swap_size;

    default_function_skeleton<valid_if_nonnegative<span_size_t>() noexcept, 32>
      _total_swap_size;

    default_function_skeleton<
      std::tuple<
        valid_if_positive<kelvins_t<float>>,
        valid_if_positive<kelvins_t<float>>>() noexcept,
      64>
      _temperature_min_max;

    default_function_skeleton<power_supply_kind() noexcept, 32>
      _power_supply_kind;

    auto _handle_stats_query(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool {
        _cpu_concurrent_threads.invoke_by(msg_ctx, message);
        _memory_page_size.invoke_by(msg_ctx, message);
        _total_ram_size.invoke_by(msg_ctx, message);
        _total_swap_size.invoke_by(msg_ctx, message);
        return true;
    }

    auto _handle_sensor_query(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool {
        _short_average_load.invoke_by(msg_ctx, message);
        _long_average_load.invoke_by(msg_ctx, message);
        _free_ram_size.invoke_by(msg_ctx, message);
        _free_swap_size.invoke_by(msg_ctx, message);
        _temperature_min_max.invoke_by(msg_ctx, message);
        _power_supply_kind.invoke_by(msg_ctx, message);
        return true;
    }
};
//------------------------------------------------------------------------------
auto make_system_info_provider_impl(subscriber&)
  -> unique_holder<system_info_provider_intf> {
    return {hold<system_info_provider_impl>};
}
//------------------------------------------------------------------------------
class system_info_consumer_impl : public system_info_consumer_intf {
public:
    system_info_consumer_impl(
      subscriber& sub,
      system_info_consumer_signals& sigs) noexcept
      : base{sub}
      , signals{sigs} {}

    void add_methods(subscriber& base) noexcept final {
        base.add_method(_uptime(signals.uptime_received)
                          .map_fulfill_by({"eagiSysInf", "uptime"}));

        base.add_method(
          _cpu_concurrent_threads(signals.cpu_concurrent_threads_received)
            .map_fulfill_by({"eagiSysInf", "cpuThreads"}));

        base.add_method(_short_average_load(signals.short_average_load_received)
                          .map_fulfill_by({"eagiSysInf", "shortLoad"}));

        base.add_method(_long_average_load(signals.long_average_load_received)
                          .map_fulfill_by({"eagiSysInf", "longLoad"}));

        base.add_method(_memory_page_size(signals.memory_page_size_received)
                          .map_fulfill_by({"eagiSysInf", "memPageSz"}));

        base.add_method(_free_ram_size(signals.free_ram_size_received)
                          .map_fulfill_by({"eagiSysInf", "freeRamSz"}));

        base.add_method(_total_ram_size(signals.total_ram_size_received)
                          .map_fulfill_by({"eagiSysInf", "totalRamSz"}));

        base.add_method(_free_swap_size(signals.free_swap_size_received)
                          .map_fulfill_by({"eagiSysInf", "freeSwpSz"}));

        base.add_method(_total_swap_size(signals.total_swap_size_received)
                          .map_fulfill_by({"eagiSysInf", "totalSwpSz"}));

        base.add_method(
          _temperature_min_max(signals.temperature_min_max_received)
            .map_fulfill_by({"eagiSysInf", "tempMinMax"}));

        base.add_method(_power_supply_kind(signals.power_supply_kind_received)
                          .map_fulfill_by({"eagiSysInf", "powerSuply"}));
    }

    void query_uptime(const endpoint_id_t endpoint_id) noexcept final {
        _uptime.invoke_on(
          base.bus_node(), endpoint_id, {"eagiSysInf", "rqUptime"});
    }

    void query_cpu_concurrent_threads(
      const endpoint_id_t endpoint_id) noexcept final {
        _cpu_concurrent_threads.invoke_on(
          base.bus_node(), endpoint_id, {"eagiSysInf", "rqCpuThrds"});
    }

    void query_short_average_load(
      const endpoint_id_t endpoint_id) noexcept final {
        _short_average_load.invoke_on(
          base.bus_node(), endpoint_id, {"eagiSysInf", "rqShrtLoad"});
    }

    void query_long_average_load(
      const endpoint_id_t endpoint_id) noexcept final {
        _long_average_load.invoke_on(
          base.bus_node(), endpoint_id, {"eagiSysInf", "rqLongLoad"});
    }

    void query_memory_page_size(const endpoint_id_t endpoint_id) noexcept final {
        _memory_page_size.invoke_on(
          base.bus_node(), endpoint_id, {"eagiSysInf", "rqMemPgSz"});
    }

    void query_free_ram_size(const endpoint_id_t endpoint_id) noexcept final {
        _free_ram_size.invoke_on(
          base.bus_node(), endpoint_id, {"eagiSysInf", "rqFreRamSz"});
    }

    void query_total_ram_size(const endpoint_id_t endpoint_id) noexcept final {
        _total_ram_size.invoke_on(
          base.bus_node(), endpoint_id, {"eagiSysInf", "rqTtlRamSz"});
    }

    void query_free_swap_size(const endpoint_id_t endpoint_id) noexcept final {
        _free_swap_size.invoke_on(
          base.bus_node(), endpoint_id, {"eagiSysInf", "rqFreSwpSz"});
    }

    void query_total_swap_size(const endpoint_id_t endpoint_id) noexcept final {
        _total_swap_size.invoke_on(
          base.bus_node(), endpoint_id, {"eagiSysInf", "rqTtlSwpSz"});
    }

    void query_temperature_min_max(const endpoint_id_t endpoint_id) noexcept {
        _temperature_min_max.invoke_on(
          base.bus_node(), endpoint_id, {"eagiSysInf", "rqTempMnMx"});
    }

    void query_power_supply_kind(const endpoint_id_t endpoint_id) noexcept {
        _power_supply_kind.invoke_on(
          base.bus_node(), endpoint_id, {"eagiSysInf", "rqPwrSuply"});
    }

    void query_stats(const endpoint_id_t endpoint_id) noexcept {
        message_view message{};
        const message_id msg_id{"eagiSysInf", "qryStats"};
        message.set_target_id(endpoint_id);
        base.bus_node().post(msg_id, message);
    }

    void query_sensors(const endpoint_id_t endpoint_id) noexcept {
        message_view message{};
        const message_id msg_id{"eagiSysInf", "qrySensors"};
        message.set_target_id(endpoint_id);
        base.bus_node().post(msg_id, message);
    }

    subscriber& base;
    system_info_consumer_signals& signals;

private:
    default_callback_invoker<std::chrono::duration<float>() noexcept, 32>
      _uptime;

    default_callback_invoker<valid_if_positive<span_size_t>() noexcept, 32>
      _cpu_concurrent_threads;

    default_callback_invoker<valid_if_nonnegative<float>() noexcept, 32>
      _short_average_load;

    default_callback_invoker<valid_if_nonnegative<float>() noexcept, 32>
      _long_average_load;

    default_callback_invoker<valid_if_positive<span_size_t>() noexcept, 32>
      _memory_page_size;

    default_callback_invoker<valid_if_positive<span_size_t>() noexcept, 32>
      _free_ram_size;

    default_callback_invoker<valid_if_positive<span_size_t>() noexcept, 32>
      _total_ram_size;

    default_callback_invoker<valid_if_nonnegative<span_size_t>() noexcept, 32>
      _free_swap_size;

    default_callback_invoker<valid_if_nonnegative<span_size_t>() noexcept, 32>
      _total_swap_size;

    default_callback_invoker<
      std::tuple<
        valid_if_positive<kelvins_t<float>>,
        valid_if_positive<kelvins_t<float>>>() noexcept,
      64>
      _temperature_min_max;

    default_callback_invoker<power_supply_kind() noexcept, 32>
      _power_supply_kind;
};
//------------------------------------------------------------------------------
auto make_system_info_consumer_impl(
  subscriber& base,
  system_info_consumer_signals& sigs)
  -> unique_holder<system_info_consumer_intf> {
    return {hold<system_info_consumer_impl>, base, sigs};
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
