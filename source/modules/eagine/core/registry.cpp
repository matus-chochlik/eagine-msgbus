/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus.core:registry;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.core.main_ctx;
import :direct;
import :interface;
import :endpoint;
import :router;
import :setup;
import <concepts>;
import <vector>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
struct registered_entry {
    std::unique_ptr<endpoint> _endpoint{};
    std::unique_ptr<service_interface> _service{};

    auto update_service() noexcept -> work_done;
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
        return extract(_add_entry(log_id)._endpoint);
    }

    /// @brief Establishes an endpoint and instantiates a service object tied to it.
    /// @see establish
    template <std::derived_from<service_interface> Service, typename... Args>
    auto emplace(const identifier log_id, Args&&... args) noexcept
      -> Service& requires(std::is_base_of_v<service_interface, Service>) {
          auto& entry = _add_entry(log_id);
          auto temp{std::make_unique<Service>(
            extract(entry._endpoint), std::forward<Args>(args)...)};
          assert(temp);
          entry._service = std::move(temp);
          return *static_cast<Service*>(entry._service.get());
      }

    /// @brief Removes a previously emplaced service.
    void remove(service_interface&) noexcept;

    auto update() noexcept -> work_done;
    auto update_all() noexcept -> work_done;

    auto is_done() noexcept -> bool {
        return _router.is_done();
    }

    void finish() noexcept {
        _router.finish();
    }

private:
    std::shared_ptr<direct_acceptor_intf> _acceptor;
    router _router;
    std::vector<registered_entry> _entries;

    auto _add_entry(const identifier log_id) noexcept -> registered_entry&;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

