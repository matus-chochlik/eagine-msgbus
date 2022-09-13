/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.services:system_info;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.units;
import eagine.core.utility;
import eagine.core.valid_if;
import eagine.core.main_ctx;
import eagine.msgbus.core;
import <array>;
import <chrono>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
struct system_info_provider_intf : interface<system_info_provider_intf> {
    virtual void add_methods(subscriber& base) noexcept = 0;
};
//------------------------------------------------------------------------------
auto make_system_info_provider_impl(subscriber&)
  -> std::unique_ptr<system_info_provider_intf>;
//------------------------------------------------------------------------------
/// @brief Service providing basic information about endpoint's host system.
/// @ingroup msgbus
/// @see service_composition
/// @see system_info_consumer
/// @see system_info
export template <typename Base = subscriber>
class system_info_provider : public Base {

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        _impl->add_methods(*this);
    }

private:
    const std::unique_ptr<system_info_provider_intf> _impl{
      make_system_info_provider_impl(*this)};
};
//------------------------------------------------------------------------------
struct system_info_consumer_intf : interface<system_info_consumer_intf> {
    virtual void add_methods(subscriber& base) noexcept = 0;

    virtual void query_uptime(const identifier_t) noexcept = 0;
    virtual void query_cpu_concurrent_threads(const identifier_t) noexcept = 0;
    virtual void query_short_average_load(const identifier_t) noexcept = 0;
    virtual void query_long_average_load(const identifier_t) noexcept = 0;
    virtual void query_memory_page_size(const identifier_t) noexcept = 0;
    virtual void query_free_ram_size(const identifier_t) noexcept = 0;
    virtual void query_total_ram_size(const identifier_t) noexcept = 0;
    virtual void query_free_swap_size(const identifier_t) noexcept = 0;
    virtual void query_total_swap_size(const identifier_t) noexcept = 0;
    virtual void query_temperature_min_max(const identifier_t) noexcept = 0;
    virtual void query_power_supply_kind(const identifier_t) noexcept = 0;
    virtual void query_stats(const identifier_t) noexcept = 0;
    virtual void query_sensors(const identifier_t) noexcept = 0;
};
//------------------------------------------------------------------------------
/// @brief Service consuming basic information about endpoint's host system.
/// @ingroup msgbus
/// @see system_info_consumer
export struct system_info_consumer_signals {
    /// @brief Triggered on receipt of endpoint's system uptime.
    /// @see query_uptime
    signal<
      void(const result_context&, const std::chrono::duration<float>&) noexcept>
      uptime_received;

    /// @brief Triggered on receipt of CPU's supported concurrent thread count.
    /// @see query_cpu_concurrent_threads
    signal<void(
      const result_context&,
      const valid_if_positive<span_size_t>&) noexcept>
      cpu_concurrent_threads_received;

    /// @brief Triggered on receipt of endpoint's host short average load.
    /// @see query_short_average_load
    signal<
      void(const result_context&, const valid_if_nonnegative<float>&) noexcept>
      short_average_load_received;

    /// @brief Triggered on receipt of endpoint's host long average load.
    /// @see query_long_average_load
    signal<
      void(const result_context&, const valid_if_nonnegative<float>&) noexcept>
      long_average_load_received;

    /// @brief Triggered on receipt of endpoint's host system memory page size.
    /// @see query_memory_page_size
    signal<void(
      const result_context&,
      const valid_if_positive<span_size_t>&) noexcept>
      memory_page_size_received;

    /// @brief Triggered on receipt of endpoint's host system free RAM size.
    /// @see query_free_ram_size
    signal<void(
      const result_context&,
      const valid_if_positive<span_size_t>&) noexcept>
      free_ram_size_received;

    /// @brief Triggered on receipt of endpoint's host system total RAM size.
    /// @see query_total_ram_size
    signal<void(
      const result_context&,
      const valid_if_positive<span_size_t>&) noexcept>
      total_ram_size_received;

    /// @brief Triggered on receipt of endpoint's host system free swap size.
    /// @see query_free_swap_size
    signal<void(
      const result_context&,
      const valid_if_nonnegative<span_size_t>&) noexcept>
      free_swap_size_received;

    /// @brief Triggered on receipt of endpoint's host system total swap size.
    /// @see query_total_swap_size
    signal<void(
      const result_context&,
      const valid_if_nonnegative<span_size_t>&) noexcept>
      total_swap_size_received;

    /// @brief Triggered on receipt of endpoint's host system min/max temperatures.
    /// @see query_power_supply_kind
    signal<void(
      const result_context&,
      const std::tuple<
        valid_if_positive<kelvins_t<float>>,
        valid_if_positive<kelvins_t<float>>>&) noexcept>
      temperature_min_max_received;

    /// @brief Triggered on receipt of endpoint's host system power supply kind.
    /// @see query_power_supply_kind
    signal<void(const result_context&, power_supply_kind) noexcept>
      power_supply_kind_received;
};
//------------------------------------------------------------------------------
auto make_system_info_consumer_impl(subscriber&, system_info_consumer_signals&)
  -> std::unique_ptr<system_info_consumer_intf>;
//------------------------------------------------------------------------------
/// @brief Service consuming basic information about endpoint's host system.
/// @ingroup msgbus
/// @see service_composition
/// @see system_info_provider
/// @see system_info
export template <typename Base = subscriber>
class system_info_consumer
  : public Base
  , public system_info_consumer_signals {

public:
    /// @brief Queries the endpoint's host system uptime.
    /// @see uptime_received
    void query_uptime(const identifier_t endpoint_id) noexcept {
        _impl->query_uptime(endpoint_id);
    }

    /// @brief Queries the endpoint's host CPU's supported concurrent thread count.
    /// @see cpu_concurrent_threads_received
    void query_cpu_concurrent_threads(const identifier_t endpoint_id) noexcept {
        _impl->query_cpu_concurrent_threads(endpoint_id);
    }

    /// @brief Queries the endpoint's host system short average load (0.0 - 1.0).
    /// @see short_average_load_received
    void query_short_average_load(const identifier_t endpoint_id) noexcept {
        _impl->query_short_average_load(endpoint_id);
    }

    /// @brief Queries the endpoint's host system long average load (0.0 - 1.0).
    /// @see long_average_load_received
    void query_long_average_load(const identifier_t endpoint_id) noexcept {
        _impl->query_long_average_load(endpoint_id);
    }

    /// @brief Queries the endpoint's host system memory page size in bytes.
    /// @see memory_page_size_received
    void query_memory_page_size(const identifier_t endpoint_id) noexcept {
        _impl->query_memory_page_size(endpoint_id);
    }

    /// @brief Queries the endpoint's host system free RAM size in bytes.
    /// @see free_ram_size_received
    /// @see query_total_ram_size
    void query_free_ram_size(const identifier_t endpoint_id) noexcept {
        _impl->query_free_ram_size(endpoint_id);
    }

    /// @brief Queries the endpoint's host system total RAM size in bytes.
    /// @see total_ram_size_received
    /// @see query_free_ram_size
    void query_total_ram_size(const identifier_t endpoint_id) noexcept {
        _impl->query_total_ram_size(endpoint_id);
    }

    /// @brief Queries the endpoint's host system free swap size in bytes.
    /// @see free_swap_size_received
    /// @see query_total_swap_size
    void query_free_swap_size(const identifier_t endpoint_id) noexcept {
        _impl->query_free_swap_size(endpoint_id);
    }

    /// @brief Queries the endpoint's host system total swap size in bytes.
    /// @see total_swap_size_received
    /// @see query_free_swap_size
    void query_total_swap_size(const identifier_t endpoint_id) noexcept {
        _impl->query_total_swap_size(endpoint_id);
    }

    /// @brief Queries the endpoint's host system minimum and maximum temperature.
    void query_temperature_min_max(const identifier_t endpoint_id) noexcept {
        _impl->query_temperature_min_max(endpoint_id);
    }

    /// @brief Queries the endpoint's host system power supply kind information.
    void query_power_supply_kind(const identifier_t endpoint_id) noexcept {
        _impl->query_power_supply_kind(endpoint_id);
    }

    /// @brief Queries all endpoint's system stats information.
    /// @see query_cpu_concurrent_threads
    /// @see query_memory_page_size
    /// @see query_total_ram_size
    /// @see query_total_swap_size
    void query_stats(const identifier_t endpoint_id) noexcept {
        _impl->query_stats(endpoint_id);
    }

    /// @brief Queries all endpoint's sensor information.
    /// @see query_short_average_load
    /// @see query_long_average_load
    /// @see query_free_ram_size
    /// @see query_free_swap_size
    /// @see query_power_supply_kind
    void query_sensors(const identifier_t endpoint_id) noexcept {
        _impl->query_sensors(endpoint_id);
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        _impl->add_methods(*this);
    }

private:
    const std::unique_ptr<system_info_consumer_intf> _impl{
      make_system_info_consumer_impl(*this, *this)};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

