/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///

#ifndef EAGINE_MSGBUS_REGISTRY_HPP
#define EAGINE_MSGBUS_REGISTRY_HPP

#include "direct.hpp"
#include "endpoint.hpp"
#include "router.hpp"
#include "service_interface.hpp"
#include <eagine/bool_aggregate.hpp>
#include <eagine/extract.hpp>
#include <eagine/main_ctx_object.hpp>
#include <memory>

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
class registry : public main_ctx_object {
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
    template <typename Service, typename... Args>
    auto emplace(const identifier log_id, Args&&... args) noexcept -> std::
      enable_if_t<std::is_base_of_v<service_interface, Service>, Service&> {
        auto& entry = _add_entry(log_id);
        auto temp{std::make_unique<Service>(
          extract(entry._endpoint), std::forward<Args>(args)...)};
        auto& result = extract(temp);
        entry._service = std::move(temp);
        return result;
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
    std::shared_ptr<direct_acceptor> _acceptor;
    router _router;
    std::vector<registered_entry> _entries;

    auto _add_entry(const identifier log_id) noexcept -> registered_entry&;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

#if !EAGINE_MSGBUS_LIBRARY || defined(EAGINE_IMPLEMENTING_MSGBUS_LIBRARY)
#include <eagine/msgbus/registry.inl>
#endif

#endif // EAGINE_MSGBUS_REGISTRY_HPP
