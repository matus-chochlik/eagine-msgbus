/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus.core:direct;

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.utility;
import eagine.core.main_ctx;
import :types;
import :message;
import :interface;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
/// @brief Common shared state for a direct connection.
/// @ingroup msgbus
/// @note Implementation detail. Do not use directly.
///
/// Connectors and acceptors sharing the same shared state object are "connected".
template <typename Lockable>
class direct_connection_state final : public main_ctx_object {
public:
    /// @brief Construction from a parent main context object.
    direct_connection_state(main_ctx_parent parent) noexcept
      : main_ctx_object{"DrctConnSt", parent} {}

    /// @brief Says that the server has disconnected.
    auto server_disconnect() noexcept {
        _server_connected = false;
    }

    /// @brief Says that the client has connected.
    auto client_connect() noexcept {
        _client_connected = true;
    }

    /// @brief Says that the client has disconnected.
    auto client_disconnect() noexcept {
        _client_connected = false;
    }

    /// @brief Indicates if the connection state is usable.
    auto is_usable() const noexcept -> bool {
        return _server_connected;
    }

    /// @brief Sends a message to the server counterpart.
    void send_to_server(
      const message_id msg_id,
      const message_view& message) noexcept {
        const std::unique_lock lock{_lockable};
        _client_to_server.back().push(msg_id, message);
    }

    /// @brief Sends a message to the client counterpart.
    auto send_to_client(
      const message_id msg_id,
      const message_view& message) noexcept -> bool {
        if(_client_connected) [[likely]] {
            const std::unique_lock lock{_lockable};
            _server_to_client.back().push(msg_id, message);
            return true;
        }
        return false;
    }

    /// @brief Fetches received messages from the client counterpart.
    auto fetch_from_client(const connection::fetch_handler handler) noexcept
      -> std::tuple<bool, bool> {
        auto& c2s = [this]() -> message_storage& {
            const std::unique_lock lock{_lockable};
            _client_to_server.swap();
            return _client_to_server.front();
        }();
        return {c2s.fetch_all(handler), _client_connected};
    }

    /// @brief Fetches received messages from the service counterpart.
    auto fetch_from_server(const connection::fetch_handler handler) noexcept
      -> bool {
        auto& s2c = [this]() -> message_storage& {
            const std::unique_lock lock{_lockable};
            _server_to_client.swap();
            return _server_to_client.front();
        }();
        return s2c.fetch_all(handler);
    }

private:
    Lockable _lockable;
    double_buffer<message_storage> _server_to_client;
    double_buffer<message_storage> _client_to_server;
    std::atomic<bool> _server_connected{true};
    std::atomic<bool> _client_connected{false};
};
//------------------------------------------------------------------------------
/// @brief Class acting as the "address" of a direct connection.
/// @ingroup msgbus
/// @see direct_acceptor
/// @see direct_client_connection
/// @see direct_server_connection
/// @see direct_connection_factory
///
template <typename Lockable>
class direct_connection_address : public main_ctx_object {
public:
    /// @brief Alias for shared pointer to direct state type.
    using shared_state = std::shared_ptr<direct_connection_state<Lockable>>;

    /// @brief Alias for shared state accept handler callable.
    /// @see process_all
    using process_handler = callable_ref<void(shared_state&)>;

    /// @brief Construction from a parent main context object.
    direct_connection_address(main_ctx_parent parent)
      : main_ctx_object{"DrctConnAd", parent} {}

    /// @brief Creates and returns the shared state for a new client connection.
    /// @see process_all
    auto connect() noexcept -> shared_state {
        auto state{std::make_shared<direct_connection_state<Lockable>>(*this)};
        _pending.push_back(state);
        return state;
    }

    /// @brief Handles the pending server counterparts for created client connections.
    /// @see connect
    auto process_all(const process_handler handler) noexcept -> work_done {
        some_true something_done{};
        for(auto& state : _pending) {
            handler(state);
            something_done();
        }
        _pending.clear();
        return something_done;
    }

private:
    small_vector<shared_state, 4> _pending;
};
//------------------------------------------------------------------------------
/// @brief Implementation of the connection_info interface for direct connections.
/// @ingroup msgbus
/// @see direct_client_connection
/// @see direct_server_connection
export template <typename Base>
class direct_connection_info : public Base {
public:
    using Base::Base;

    auto kind() noexcept -> connection_kind final {
        return connection_kind::in_process;
    }

    auto addr_kind() noexcept -> connection_addr_kind final {
        return connection_addr_kind::none;
    }

    auto type_id() noexcept -> identifier final {
        return "Direct";
    }
};
//------------------------------------------------------------------------------
/// @brief Implementation of client-side direct connection.
/// @ingroup msgbus
/// @see direct_server_connection
/// @see direct_acceptor
/// @see direct_connection_factory
export template <typename Lockable>
class direct_client_connection final
  : public direct_connection_info<connection> {
public:
    direct_client_connection(
      const std::shared_ptr<direct_connection_address<Lockable>>&
        address) noexcept
      : _weak_address{address}
      , _state{address->connect()} {
        if(_state) [[likely]] {
            _state->client_connect();
        }
    }

    direct_client_connection(direct_client_connection&&) = delete;
    direct_client_connection(const direct_client_connection&) = delete;
    auto operator=(direct_client_connection&&) = delete;
    auto operator=(const direct_client_connection&) = delete;

    ~direct_client_connection() noexcept final {
        if(_state) [[likely]] {
            _state->client_disconnect();
        }
    }

    auto is_usable() noexcept -> bool final {
        _checkup();
        return _state && _state->is_usable();
    }

    auto send(const message_id msg_id, const message_view& message) noexcept
      -> bool final {
        _checkup();
        if(_state) [[likely]] {
            _state->send_to_server(msg_id, message);
            return true;
        }
        return false;
    }

    auto fetch_messages(const connection::fetch_handler handler) noexcept
      -> work_done final {
        some_true something_done{_checkup()};
        if(_state) [[likely]] {
            something_done(_state->fetch_from_server(handler));
        }
        return something_done;
    }

    auto query_statistics(connection_statistics& stats) noexcept -> bool final {
        stats.block_usage_ratio = 1.F;
        return true;
    }

    void cleanup() noexcept final {}

private:
    const std::weak_ptr<direct_connection_address<Lockable>> _weak_address;
    std::shared_ptr<direct_connection_state<Lockable>> _state;

    auto _checkup() -> work_done {
        some_true something_done;
        if(not _state) [[unlikely]] {
            if(const auto address{_weak_address.lock()}) {
                _state = extract(address).connect();
                something_done();
            }
        }
        return something_done;
    }
};
//------------------------------------------------------------------------------
/// @brief Implementation of server-side direct connection.
/// @ingroup msgbus
/// @see direct_client_connection
/// @see direct_acceptor
/// @see direct_connection_factory
export template <typename Lockable>
class direct_server_connection final
  : public direct_connection_info<connection> {
public:
    direct_server_connection(
      std::shared_ptr<direct_connection_state<Lockable>>& state) noexcept
      : _state{state} {}

    direct_server_connection(direct_server_connection&&) = delete;
    direct_server_connection(const direct_server_connection&) = delete;
    auto operator=(direct_server_connection&&) = delete;
    auto operator=(const direct_server_connection&) = delete;

    ~direct_server_connection() noexcept final {
        if(_state) [[likely]] {
            _state->server_disconnect();
        }
    }

    auto is_usable() noexcept -> bool final {
        if(_state) [[likely]] {
            if(_is_usable) [[likely]] {
                return true;
            }
            _state.reset();
        }
        return false;
    }

    auto send(const message_id msg_id, const message_view& message) noexcept
      -> bool final {
        if(_state) [[likely]] {
            return _state->send_to_client(msg_id, message);
        }
        return false;
    }

    auto fetch_messages(const connection::fetch_handler handler) noexcept
      -> work_done final {
        bool result = false;
        if(_state) [[likely]] {
            std::tie(result, _is_usable) = _state->fetch_from_client(handler);
        }
        return result;
    }

    auto query_statistics(connection_statistics&) noexcept -> bool final {
        return false;
    }

private:
    std::shared_ptr<direct_connection_state<Lockable>> _state;
    bool _is_usable{true};
};
//------------------------------------------------------------------------------
export struct direct_acceptor_intf : direct_connection_info<acceptor> {
    virtual auto make_connection() noexcept -> std::unique_ptr<connection> = 0;
};
//------------------------------------------------------------------------------
//
/// @brief Implementation of acceptor for direct connections.
/// @ingroup msgbus
/// @see direct_connection_factory
export template <typename Lockable>
class direct_acceptor
  : public direct_acceptor_intf
  , public main_ctx_object {
    using shared_state = std::shared_ptr<direct_connection_state<Lockable>>;

public:
    /// @brief Construction from a parent main context object and an address object.
    direct_acceptor(
      main_ctx_parent parent,
      std::shared_ptr<direct_connection_address<Lockable>> address) noexcept
      : main_ctx_object{"DrctAccptr", parent}
      , _address{std::move(address)} {}

    /// @brief Construction from a parent main context object with implicit address.
    direct_acceptor(main_ctx_parent parent) noexcept
      : main_ctx_object{"DrctAccptr", parent}
      , _address{std::make_shared<direct_connection_address<Lockable>>(*this)} {
    }

    auto process_accepted(const accept_handler handler) noexcept
      -> work_done final {
        some_true something_done{};
        if(_address) {
            auto wrapped_handler = [&handler](shared_state& state) {
                handler(std::unique_ptr<connection>{
                  std::make_unique<direct_server_connection<Lockable>>(state)});
            };
            something_done(
              _address->process_all({construct_from, wrapped_handler}));
        }
        return something_done;
    }

    /// @brief Makes a new client-side direct connection.
    auto make_connection() noexcept -> std::unique_ptr<connection> final {
        if(_address) {
            return std::unique_ptr<connection>{
              std::make_unique<direct_client_connection<Lockable>>(_address)};
        }
        return {};
    }

private:
    const std::shared_ptr<direct_connection_address<Lockable>> _address{};
};
//------------------------------------------------------------------------------
/// @brief Implementation of connection_factory for direct connections.
/// @ingroup msgbus
/// @see direct_acceptor
export template <typename Lockable>
class direct_connection_factory
  : public direct_connection_info<connection_factory>
  , public main_ctx_object {
public:
    using connection_factory::make_acceptor;
    using connection_factory::make_connector;

    /// @brief Construction from a parent main context object with implicit address.
    direct_connection_factory(main_ctx_parent parent) noexcept
      : main_ctx_object{"DrctConnFc", parent}
      , _default_addr{_make_addr()} {}

    auto make_acceptor(const string_view addr_str) noexcept
      -> std::unique_ptr<acceptor> final {
        if(addr_str) {
            return std::make_unique<direct_acceptor<Lockable>>(
              *this, _get(addr_str));
        }
        return std::make_unique<direct_acceptor<Lockable>>(
          *this, _default_addr);
    }

    auto make_connector(const string_view addr_str) noexcept
      -> std::unique_ptr<connection> final {
        if(addr_str) {
            return std::make_unique<direct_client_connection<Lockable>>(
              _get(addr_str));
        }
        return std::make_unique<direct_client_connection<Lockable>>(
          _default_addr);
    }

private:
    std::shared_ptr<direct_connection_address<Lockable>> _default_addr;
    std::map<
      std::string,
      std::shared_ptr<direct_connection_address<Lockable>>,
      basic_str_view_less<std::string, string_view>>
      _addrs;

    auto _make_addr() noexcept
      -> std::shared_ptr<direct_connection_address<Lockable>> {
        return std::make_shared<direct_connection_address<Lockable>>(*this);
    }

    auto _get(const string_view addr_str) noexcept
      -> std::shared_ptr<direct_connection_address<Lockable>>& {
        auto pos = _addrs.find(addr_str);
        if(pos == _addrs.end()) {
            pos = _addrs.emplace(to_string(addr_str), _make_addr()).first;
        }
        assert(pos != _addrs.end());
        return pos->second;
    }
};
//------------------------------------------------------------------------------
export auto make_direct_acceptor(main_ctx_parent parent)
  -> std::unique_ptr<direct_acceptor_intf> {
    return std::make_unique<direct_acceptor<std::mutex>>(parent);
}
//------------------------------------------------------------------------------
export auto make_direct_connection_factory(main_ctx_parent parent) {
    return std::make_unique<direct_connection_factory<std::mutex>>(parent);
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

