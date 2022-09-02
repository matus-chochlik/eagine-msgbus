/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

module eagine.msgbus.services;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.serialization;
import eagine.core.valid_if;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.logging;
import eagine.core.math;
import eagine.core.main_ctx;
import eagine.msgbus.core;
import <algorithm>;
import <array>;
import <chrono>;
import <cmath>;
import <ostream>;
import <random>;
import <tuple>;
import <vector>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
// sudoku_helper_rank_info
//------------------------------------------------------------------------------
template <unsigned S>
struct sudoku_helper_rank_info {
    default_sudoku_board_traits<S> traits;
    memory::buffer serialize_buffer;
    int max_recursion{1};

    std::vector<
      std::tuple<identifier_t, message_sequence_t, basic_sudoku_board<S>>>
      boards;

    flat_set<identifier_t> searches;

    sudoku_helper_rank_info() noexcept = default;

    void on_search(const identifier_t source_id) noexcept {
        searches.insert(source_id);
    }

    void add_board(
      endpoint& bus,
      const identifier_t source_id,
      const message_sequence_t sequence_no,
      const basic_sudoku_board<S> board) noexcept;

    auto do_send_board(
      endpoint& bus,
      const data_compressor& compressor,
      const auto target_id,
      const auto sequence_no,
      const auto& candidate,
      const bool is_solved);

    auto process_board(
      endpoint& bus,
      const data_compressor& compressor,
      const auto target_id,
      const auto sequence_no,
      const auto& candidate,
      bool& done,
      int levels) noexcept;

    auto update(endpoint& bus, const data_compressor&) noexcept -> work_done;
};
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_helper_rank_info<S>::add_board(
  endpoint& bus,
  const identifier_t source_id,
  const message_sequence_t sequence_no,
  const basic_sudoku_board<S> board) noexcept {
    if(boards.size() <= 8) [[likely]] {
        searches.insert(source_id);
        boards.emplace_back(source_id, sequence_no, std::move(board));
    } else {
        bus.log_warning("too many boards in backlog")
          .arg("rank", S)
          .arg("count", boards.size());
    }
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_helper_rank_info<S>::do_send_board(
  endpoint& bus,
  const data_compressor& compressor,
  const auto target_id,
  const auto sequence_no,
  const auto& candidate,
  const bool is_solved) {
    serialize_buffer.ensure(default_serialize_buffer_size_for(candidate));
    const auto serialized{
      (S >= 4) ? default_serialize_packed(
                   candidate, cover(serialize_buffer), compressor)
               : default_serialize(candidate, cover(serialize_buffer))};
    assert(serialized);

    const unsigned_constant<S> rank{};
    message_view response{extract(serialized)};
    response.set_target_id(target_id);
    response.set_sequence_no(sequence_no);
    bus.post(sudoku_response_msg(rank, is_solved), response);
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_helper_rank_info<S>::process_board(
  endpoint& bus,
  const data_compressor& compressor,
  const auto target_id,
  const auto sequence_no,
  const auto& candidate,
  bool& done,
  int levels) noexcept {
    const auto send_board = [&, this](auto& board, bool is_solved) {
        do_send_board(
          bus, compressor, target_id, sequence_no, board, is_solved);
    };
    const auto process_recursive = [&, this](auto& board) {
        process_board(
          bus, compressor, target_id, sequence_no, board, done, levels - 1);
    };

    candidate.for_each_alternative(
      candidate.find_unsolved(), [&](const auto& intermediate) {
          if(intermediate.is_solved()) {
              send_board(intermediate, true);
              done = true;
          } else if(!done) {
              if(levels > 0) {
                  process_recursive(intermediate);
              } else {
                  send_board(intermediate, false);
              }
          }
      });
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_helper_rank_info<S>::update(
  endpoint& bus,
  const data_compressor& compressor) noexcept -> work_done {
    const unsigned_constant<S> rank{};
    some_true something_done;

    if(boards.size() < 6) {
        for(auto target_id : searches) {
            message_view response{};
            response.set_target_id(target_id);
            bus.post(sudoku_alive_msg(rank), response);
            something_done();
        }
    }
    searches.clear();

    if(!boards.empty()) {
        const auto target_id = std::get<0>(boards.back());
        const auto sequence_no = std::get<1>(boards.back());
        auto board = std::get<2>(boards.back());
        boards.pop_back();

        bool done{false};
        process_board(
          bus, compressor, target_id, sequence_no, board, done, max_recursion);

        message_view response{};
        response.set_target_id(target_id);
        response.set_sequence_no(sequence_no);
        bus.post(sudoku_done_msg(rank), response);
        something_done();
    }
    return something_done;
}
//------------------------------------------------------------------------------
// sudoku_helper_impl
//------------------------------------------------------------------------------
class sudoku_helper_impl : public sudoku_helper_intf {
    using This = sudoku_helper_impl;

public:
    void add_methods(subscriber&) noexcept final;
    void init(subscriber&) noexcept final;
    auto update(endpoint& bus) noexcept -> work_done final;

    void mark_activity() noexcept final {
        _activity_time = std::chrono::steady_clock::now();
    }

    /// @brief Returns current idle time interval.
    auto idle_time() const noexcept
      -> std::chrono::steady_clock::duration final {
        return std::chrono::steady_clock::now() - _activity_time;
    }

private:
    template <unsigned S>
    auto _handle_search(
      const message_context&,
      const stored_message& message) noexcept -> bool;

    template <unsigned S>
    static constexpr auto _bind_handle_search(
      const unsigned_constant<S> rank) noexcept;

    template <unsigned S>
    auto _handle_board(
      const message_context&,
      const stored_message& message) noexcept -> bool;

    template <unsigned S>
    static constexpr auto _bind_handle_board(
      const unsigned_constant<S> rank) noexcept;

    data_compressor _compressor{};

    sudoku_rank_tuple<sudoku_helper_rank_info> _infos;

    std::chrono::steady_clock::time_point _activity_time{
      std::chrono::steady_clock::now()};
};
//------------------------------------------------------------------------------
auto make_sudoku_helper_impl() -> std::unique_ptr<sudoku_helper_intf> {
    return std::make_unique<sudoku_helper_impl>();
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_helper_impl::_handle_search(
  const message_context&,
  const stored_message& message) noexcept -> bool {
    _infos.get(unsigned_constant<S>{}).on_search(message.source_id);
    mark_activity();
    return true;
}
//------------------------------------------------------------------------------
template <unsigned S>
constexpr auto sudoku_helper_impl::_bind_handle_search(
  const unsigned_constant<S> rank) noexcept {
    return message_handler_map<member_function_constant<
      bool (This::*)(const message_context&, const stored_message&) noexcept,
      &This::_handle_search<S>>>{sudoku_search_msg(rank)};
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_helper_impl::_handle_board(
  const message_context& ctx,
  const stored_message& message) noexcept -> bool {
    const unsigned_constant<S> rank{};
    auto& info = _infos.get(rank);
    basic_sudoku_board<S> board{info.traits};

    const auto deserialized{
      (S >= 4)
        ? default_deserialize_packed(board, message.content(), _compressor)
        : default_deserialize(board, message.content())};

    if(deserialized) [[likely]] {
        info.add_board(
          ctx.bus_node(),
          message.source_id,
          message.sequence_no,
          std::move(board));
        mark_activity();
    }
    return true;
}
//------------------------------------------------------------------------------
template <unsigned S>
constexpr auto sudoku_helper_impl::_bind_handle_board(
  const unsigned_constant<S> rank) noexcept {
    return message_handler_map<member_function_constant<
      bool (This::*)(const message_context&, const stored_message&) noexcept,
      &This::_handle_board<S>>>{sudoku_query_msg(rank)};
}
//------------------------------------------------------------------------------
void sudoku_helper_impl::add_methods(subscriber& base) noexcept {
    sudoku_rank_tuple<unsigned_constant> ranks{};
    for_each_sudoku_rank_unit(
      [&](auto rank) {
          base.add_method(this, _bind_handle_search(rank));
          base.add_method(this, _bind_handle_board(rank));
      },
      ranks);

    mark_activity();
}
//------------------------------------------------------------------------------
void sudoku_helper_impl::init(subscriber& base) noexcept {
    if(const auto max_recursion{base.app_config().get(
         "msgbus.sudoku.helper.max_recursion", std::type_identity<int>{})}) {
        if(max_recursion >= 0) {
            base.bus_node()
              .log_info("setting maximum recursion to ${recursion}")
              .tag("sdkuMaxRec")
              .arg("recursion", extract(max_recursion));
            for_each_sudoku_rank_unit(
              [&](auto& info) { info.max_recursion = extract(max_recursion); },
              _infos);
        }
    }
}
//------------------------------------------------------------------------------
auto sudoku_helper_impl::update(endpoint& bus) noexcept -> work_done {
    some_true something_done{};

    for_each_sudoku_rank_unit(
      [&](auto& info) {
          if(info.update(bus, _compressor)) {
              something_done();
          }
      },
      _infos);

    return something_done;
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
