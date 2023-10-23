/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:loopback;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.utility;
import eagine.core.main_ctx;
import :types;
import :message;
import :interface;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Loopback implementation of connection. Used mainly for testing and development.
/// @ingroup msgbus
export class loopback_connection : public connection {
public:
    auto kind() noexcept -> connection_kind final {
        return connection_kind::in_process;
    }

    auto addr_kind() noexcept -> connection_addr_kind final {
        return connection_addr_kind::none;
    }

    auto type_id() noexcept -> identifier final {
        return "Loopback";
    }

    auto send(const message_id msg_id, const message_view& message) noexcept
      -> bool final {
        const std::unique_lock lock{_mutex};
        _messages.push(msg_id, message);
        return true;
    }

    auto fetch_messages(const connection::fetch_handler handler) noexcept
      -> work_done final {
        const std::unique_lock lock{_mutex};
        return _messages.fetch_all(handler);
    }

    auto query_statistics(connection_statistics& stats) noexcept -> bool final {
        stats.block_usage_ratio = 1.F;
        return true;
    }

    auto routing_weight() noexcept -> float final {
        return 0.4F;
    }

private:
    std::mutex _mutex;
    message_storage _messages;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

