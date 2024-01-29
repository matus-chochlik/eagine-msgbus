/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
/// https://www.boost.org/LICENSE_1_0.txt
///
export module eagine.msgbus.core:future;

import std;
import eagine.core.types;
import eagine.core.container;
import eagine.core.utility;
import :types;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export template <typename T>
struct future_state {
    timeout too_late{adjusted_duration(std::chrono::seconds{1})};
    std::function<void(T)> success_handler{};
    std::function<void()> timeout_handler{};
};
//------------------------------------------------------------------------------
/// @brief Message bus promise class.
/// @ingroup msgbus
/// @see future
/// @see pending_promises
export template <typename T>
class promise {
public:
    /// @brief Default constructor.
    promise() noexcept = default;

    promise(std::shared_ptr<future_state<T>>& state) noexcept
      : _state{state} {}

    auto should_be_removed() noexcept -> bool {
        if(const auto state{_state.lock()}) {
            if(state->too_late) {
                if(state->timeout_handler) {
                    state->timeout_handler();
                    _state.reset();
                }
            } else {
                return false;
            }
        }
        return true;
    }

    /// @brief Fulfills the promise and the corresponding future.
    void fulfill(T value) noexcept {
        if(const auto state{_state.lock()}) {
            _state.reset();
            if(state->too_late) {
                if(state->timeout_handler) {
                    state->timeout_handler();
                }
            } else {
                if(state->success_handler) {
                    state->success_handler(std::move(value));
                }
            }
        }
    }

private:
    std::weak_ptr<future_state<T>> _state{};
};
//------------------------------------------------------------------------------
/// @brief Message bus future class.
/// @ingroup msgbus
/// @see promise
/// @see pending_promises
export template <typename T>
class future {
public:
    /// @brief Default constructor.
    future() = default;

    /// @brief Constructs empty stateless future.
    future(const nothing_t) noexcept
      : _state{} {}

    /// @brief Checks if the future has state (is associated with a promise).
    explicit operator bool() const noexcept {
        return bool(_state);
    }

    /// @brief Sets the timeout for this future if there is shared state.
    template <typename R, typename P>
    auto set_timeout(const std::chrono::duration<R, P> dur) -> future<T>& {
        if(_state) {
            _state->too_late.reset(dur);
        }
        return *this;
    }

    /// @brief Sets the on-success handler.
    /// @see then
    /// @see on_timeout
    auto on_success(const std::function<void(T)>& handler) noexcept
      -> future<T>& {
        if(_state) {
            _state->success_handler = handler;
        }
        return *this;
    }

    /// @brief Sets the on-timeout handler.
    /// @see on_success
    auto on_timeout(const std::function<void()>& handler) noexcept
      -> future<T>& {
        if(_state) {
            _state->timeout_handler = handler;
        }
        return *this;
    }

    /// @brief Wraps the given handler object and sets it as the on-success handler.
    /// @see on_success
    /// @see otherwise
    template <typename Handler>
    auto then(Handler handler) noexcept
      -> future<T>& requires(std::is_invocable_v<Handler, T>) {
          if(_state) {
              _state->success_handler = std::function<void(T)>(
                [state{_state}, handler{std::move(handler)}](T value) {
                    handler(value);
                });
          }
          return *this;
      }

    /// @brief Wraps the given handler object and sets it as the on-timeout handler.
    /// @see on_timeout
    /// @see then
    template <typename Handler>
    auto otherwise(Handler handler) noexcept -> future<T>&
        requires(std::is_invocable_v<Handler>)
    {
        if(_state) {
            _state->timeout_handler = std::function<void()>(
              [state{_state}, handler{std::move(handler)}]() { handler(); });
        }
        return *this;
    }

    /// @brief Returns the associated promise it there is shared state.
    auto get_promise() noexcept -> promise<T> {
        return {_state};
    }

private:
    std::shared_ptr<future_state<T>> _state{
      std::make_shared<future_state<T>>()};
};
//------------------------------------------------------------------------------
/// @brief Class that makes new and tracks existing pending message bus promises.
/// @ingroup msgbus
/// @see promise
/// @see future
export template <typename T>
class pending_promises {
public:
    /// @brief Constructs and returns a new message bus future and its unique id.
    ///
    /// The returned future can be used to retrieve the promise.
    auto make() noexcept -> std::tuple<message_sequence_t, future<T>> {
        future<T> result{};
        const auto id{++_id_seq};
        _promises[id] = result.get_promise();
        return {id, result};
    }

    /// @brief Fulfills the promise/future pair identified by id with the given value.
    void fulfill(const message_sequence_t id, T value) noexcept {
        if(const auto found{find(_promises, id)}) {
            found->fulfill(std::move(value));
            _promises.erase(found.position());
        }
        update();
    }

    /// @brief Update the internal state of this promise/future tracker.
    auto update() noexcept -> bool {
        return _promises.erase_if(
                 [](auto& p) { return p.second.should_be_removed(); }) > 0;
    }

    /// @brief Indicates if there are any unfulfilled pending promises.
    /// @see has_none
    auto has_some() const noexcept -> bool {
        return not _promises.empty();
    }

    /// @brief Indicates if there are no pending promises.
    /// @see has_some
    auto has_none() const noexcept -> bool {
        return _promises.empty();
    }

private:
    message_sequence_t _id_seq{0};
    flat_map<message_sequence_t, promise<T>> _promises{};
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
