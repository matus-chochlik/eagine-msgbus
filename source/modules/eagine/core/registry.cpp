/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus.core:registry;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.core.main_ctx;
import :types;
import :direct;
import :interface;
import :endpoint;
import :router;
import :setup;
import :service;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
struct registered_entry {
    unique_holder<endpoint> _endpoint{};
    unique_holder<service_interface> _service{};

    auto endpoint() noexcept -> msgbus::endpoint& {
        return *_endpoint;
    }
    auto update_service() noexcept -> work_done;
    auto update_and_process_service() noexcept -> work_done;
};
//------------------------------------------------------------------------------
/// @brief Class combining a local bus router and a set of endpoints.
/// @ingroup msgbus
export class registry : public main_ctx_object {
public:
    /// @brief Construction from parent main context object.
    registry(main_ctx_parent parent) noexcept;

    /// @brief Establishes a new endpoint with the specified logger identifier.
    /// @see emplace
    [[nodiscard]] auto establish(const identifier log_id) noexcept
      -> endpoint& {
        return _add_entry(log_id).endpoint();
    }

    /// @brief Returns the id of the internal router.
    auto router_id() noexcept -> endpoint_id_t {
        return _router.get_id();
    }

    /// @brief Establishes an endpoint and instantiates a service object tied to it.
    /// @see establish
    /// @see remove
    template <std::derived_from<service_interface> Service, typename... Args>
    auto emplace(const identifier log_id, Args&&... args) noexcept
      -> Service& requires(std::is_base_of_v<service_interface, Service>) {
          auto& entry = _add_entry(log_id);
          unique_holder<service_interface> temp{
            hold<Service>, entry.endpoint(), std::forward<Args>(args)...};
          assert(temp);
          entry._service = std::move(temp);
          return *(entry._service.ref().as<Service>());
      }

    /// @brief Updates this registry until all registerd services have id or timeout.
    auto wait_for_ids(const std::chrono::milliseconds) noexcept -> bool;

    /// @brief Updates this registry until all specified services have id or timeout.
    /// @return Indicates if the service has_id
    template <typename R, typename P, composed_service... Service>
    auto wait_for_id_of(
      const std::chrono::duration<R, P> t,
      Service&... service) noexcept {
        timeout get_id_time{t};
        while(not(... and service.has_id())) {
            if(get_id_time.is_expired()) [[unlikely]] {
                return false;
            }
            update_and_process();
        }
        return true;
    }

    /// @brief Returns a view of the registered services.
    auto services() noexcept -> pointee_generator<service_interface*> {
        for(auto pos{_entries.begin()}; pos != _entries.end(); ++pos) {
            assert(pos->_service);
            co_yield pos->_service.get();
        }
    }

    /// @brief Removes a previously emplaced service.
    /// @see emplace
    void remove(service_interface&) noexcept;

    /// @brief Updates the internal router.
    /// @see update_only
    /// @see update_and_process
    auto update_self() noexcept -> work_done;

    /// @brief Updates the internal router and the services without processing message.
    /// @see update_self
    /// @see update_and_process
    auto update_only() noexcept -> work_done;

    /// @brief Updates the internal router and all emplaced services.
    /// @see update_self
    /// @see update_only
    auto update_and_process() noexcept -> work_done;

    auto is_done() noexcept -> bool {
        return _router.is_done();
    }

    void finish() noexcept {
        _router.finish();
    }

private:
    shared_holder<direct_acceptor_intf> _acceptor;
    router _router;
    std::vector<registered_entry> _entries;

    auto _add_entry(const identifier log_id) noexcept -> registered_entry&;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

