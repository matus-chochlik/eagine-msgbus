/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus:router_address;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.logging;
import eagine.core.main_ctx;
import <string>;
import <vector>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Class loading and managing message bus router address(es).
/// @ingroup msgbus
export class router_address : public main_ctx_object {
public:
    router_address(main_ctx_parent parent, const nothing_t) noexcept
      : main_ctx_object{identifier{"RouterAddr"}, parent} {}

    /// @brief Construction from parent main context object.
    router_address(main_ctx_parent parent) noexcept
      : router_address{parent, nothing} {
        configure(app_config());
    }

    void configure(application_config& config) {
        if(config.fetch("msgbus.router.address", _addrs)) {
            log_debug("configured router address(es) ${addr}")
              .arg_func([&](logger_backend& backend) {
                  for(auto& addr : _addrs) {
                      backend.add_string(
                        identifier{"address"}, identifier{"string"}, addr);
                  }
              });
        }
    }

    /// @brief Indicates if this instance contains at least one address.
    explicit operator bool() const noexcept {
        return !_addrs.empty();
    }

    /// @brief Implicit conversion to string_view, returning the first address.
    operator string_view() const noexcept {
        if(_addrs.empty()) {
            return {};
        }
        return {_addrs.front()};
    }

    /// @brief Returns the number of addresses stored in this instance.
    auto count() const noexcept -> span_size_t {
        return span_size(_addrs.size());
    }

    /// @brief Returns a const iterator to the start of the range of addresses.
    auto begin() const noexcept {
        return _addrs.cbegin();
    }

    /// @brief Returns a const iterator past the end of the range of addresses.
    auto end() const noexcept {
        return _addrs.cend();
    }

private:
    std::vector<std::string> _addrs{};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
