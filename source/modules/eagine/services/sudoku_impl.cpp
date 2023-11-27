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

import std;
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

namespace eagine::msgbus {
//------------------------------------------------------------------------------
template <template <unsigned> class U>
struct sudoku_rank_tuple
  : std::tuple<nothing_t, nothing_t, nothing_t, U<3>, U<4>, U<5>, U<6>> {
    using base =
      std::tuple<nothing_t, nothing_t, nothing_t, U<3>, U<4>, U<5>, U<6>>;

    sudoku_rank_tuple() = default;

    template <typename... Args>
    sudoku_rank_tuple(const Args&... args) noexcept
      : base{
          nothing,
          nothing,
          nothing,
          {args...},
          {args...},
          {args...},
          {args...}} {}

    template <unsigned S>
    auto get(const unsigned_constant<S>) noexcept -> auto& {
        return std::get<S>(*this);
    }

    template <unsigned S>
    auto get(const unsigned_constant<S>) const noexcept -> const auto& {
        return std::get<S>(*this);
    }
};
//------------------------------------------------------------------------------
template <typename Function, typename... RankTuple>
void for_each_sudoku_rank_unit(Function func, RankTuple&... t) {
    func(std::get<3>(t)...);
    func(std::get<4>(t)...);
    func(std::get<5>(t)...);
    func(std::get<6>(t)...);
}
//------------------------------------------------------------------------------
template <typename Function, typename... RankTuple>
void apply_to_sudoku_rank_unit(unsigned rank, Function func, RankTuple&... t) {
    switch(rank) {
        case 3:
            func(std::get<3>(t)...);
            break;
        case 4:
            func(std::get<4>(t)...);
            break;
        case 5:
            func(std::get<5>(t)...);
            break;
        case 6:
            func(std::get<6>(t)...);
            break;
        default:
            break;
    }
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_search_msg(const unsigned_constant<S>) noexcept {
    if constexpr(S == 3) {
        return message_id{"eagiSudoku", "search3"};
    }
    if constexpr(S == 4) {
        return message_id{"eagiSudoku", "search4"};
    }
    if constexpr(S == 5) {
        return message_id{"eagiSudoku", "search5"};
    }
    if constexpr(S == 6) {
        return message_id{"eagiSudoku", "search6"};
    }
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_alive_msg(const unsigned_constant<S>) noexcept {
    if constexpr(S == 3) {
        return message_id{"eagiSudoku", "alive3"};
    }
    if constexpr(S == 4) {
        return message_id{"eagiSudoku", "alive4"};
    }
    if constexpr(S == 5) {
        return message_id{"eagiSudoku", "alive5"};
    }
    if constexpr(S == 6) {
        return message_id{"eagiSudoku", "alive6"};
    }
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_query_msg(const unsigned_constant<S>) noexcept {
    if constexpr(S == 3) {
        return message_id{"eagiSudoku", "query3"};
    }
    if constexpr(S == 4) {
        return message_id{"eagiSudoku", "query4"};
    }
    if constexpr(S == 5) {
        return message_id{"eagiSudoku", "query5"};
    }
    if constexpr(S == 6) {
        return message_id{"eagiSudoku", "query6"};
    }
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_solved_msg(const unsigned_constant<S>) noexcept -> message_id {
    if constexpr(S == 3) {
        return message_id{"eagiSudoku", "solved3"};
    }
    if constexpr(S == 4) {
        return message_id{"eagiSudoku", "solved4"};
    }
    if constexpr(S == 5) {
        return message_id{"eagiSudoku", "solved5"};
    }
    if constexpr(S == 6) {
        return message_id{"eagiSudoku", "solved6"};
    }
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_candidate_msg(const unsigned_constant<S>) noexcept -> message_id {
    if constexpr(S == 3) {
        return message_id{"eagiSudoku", "candidate3"};
    }
    if constexpr(S == 4) {
        return message_id{"eagiSudoku", "candidate4"};
    }
    if constexpr(S == 5) {
        return message_id{"eagiSudoku", "candidate5"};
    }
    if constexpr(S == 6) {
        return message_id{"eagiSudoku", "candidate6"};
    }
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_done_msg(const unsigned_constant<S>) noexcept -> message_id {
    if constexpr(S == 3) {
        return message_id{"eagiSudoku", "done3"};
    }
    if constexpr(S == 4) {
        return message_id{"eagiSudoku", "done4"};
    }
    if constexpr(S == 5) {
        return message_id{"eagiSudoku", "done5"};
    }
    if constexpr(S == 6) {
        return message_id{"eagiSudoku", "done6"};
    }
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_response_msg(
  const unsigned_constant<S> rank,
  const bool is_solved) noexcept -> message_id {
    return is_solved ? sudoku_solved_msg(rank) : sudoku_candidate_msg(rank);
}
//------------------------------------------------------------------------------
// sudoku_helper_rank_info
//------------------------------------------------------------------------------
template <unsigned S>
struct sudoku_helper_rank_info {
    default_sudoku_board_traits<S> traits;
    memory::buffer serialize_buffer;
    int max_recursion{1};

    std::vector<
      std::tuple<endpoint_id_t, message_sequence_t, basic_sudoku_board<S>>>
      boards;

    flat_set<endpoint_id_t> searches;

    sudoku_helper_rank_info() noexcept = default;

    void on_search(const endpoint_id_t source_id) noexcept {
        searches.insert(source_id);
    }

    void add_board(
      endpoint& bus,
      const endpoint_id_t source_id,
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
  const endpoint_id_t source_id,
  const message_sequence_t sequence_no,
  const basic_sudoku_board<S> board) noexcept {
    if(boards.size() < 20) [[likely]] {
        searches.insert(source_id);
        boards.emplace_back(source_id, sequence_no, std::move(board));
    } else {
        bus.log_warning("too many boards (${count}) in backlog")
          .tag("tooMnyBrds")
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
    message_view response{*serialized};
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
    const auto send_board{[&, this](auto& board, bool is_solved) {
        do_send_board(
          bus, compressor, target_id, sequence_no, board, is_solved);
    }};
    const auto process_recursive{[&, this](auto& board) {
        process_board(
          bus, compressor, target_id, sequence_no, board, done, levels - 1);
    }};

    candidate.for_each_alternative(
      candidate.find_unsolved(), [&](const auto& intermediate) {
          if(intermediate.is_solved()) {
              send_board(intermediate, true);
              done = true;
          } else if(not done) {
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

    if(not boards.empty()) {
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
    sudoku_helper_impl(subscriber& sub) noexcept
      : base{sub}
      , _compressor{base.bus_node().main_context().buffers()} {}

    void add_methods() noexcept final;
    void init() noexcept final;
    auto update() noexcept -> work_done final;

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

    subscriber& base;

    data_compressor _compressor;

    sudoku_rank_tuple<sudoku_helper_rank_info> _infos;

    std::chrono::steady_clock::time_point _activity_time{
      std::chrono::steady_clock::now()};
};
//------------------------------------------------------------------------------
auto make_sudoku_helper_impl(subscriber& base)
  -> unique_holder<sudoku_helper_intf> {
    return {hold<sudoku_helper_impl>, base};
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
void sudoku_helper_impl::add_methods() noexcept {
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
void sudoku_helper_impl::init() noexcept {
    if(const auto max_recursion{base.app_config().get(
         "msgbus.sudoku.helper.max_recursion", std::type_identity<int>{})}) {
        if(max_recursion >= 0) {
            base.bus_node()
              .log_info("setting maximum recursion to ${recursion}")
              .tag("sdkuMaxRec")
              .arg("recursion", *max_recursion);
            for_each_sudoku_rank_unit(
              [&](auto& info) { info.max_recursion = *max_recursion; }, _infos);
        }
    }
}
//------------------------------------------------------------------------------
auto sudoku_helper_impl::update() noexcept -> work_done {
    some_true something_done{};

    for_each_sudoku_rank_unit(
      [&](auto& info) {
          if(info.update(base.bus_node(), _compressor)) {
              something_done();
          }
      },
      _infos);

    return something_done;
}
//------------------------------------------------------------------------------
template <unsigned S>
struct sudoku_solver_rank_info {
    message_sequence_t query_sequence{0};
    default_sudoku_board_traits<S> traits;
    memory::buffer serialize_buffer;

    timeout search_timeout{std::chrono::seconds(3), nothing};

    constexpr auto default_solution_timeout() noexcept {
        return adjusted_duration(std::chrono::seconds{S * S * S * S * S});
    }

    timeout solution_timeout{default_solution_timeout()};

    using board_set = std::vector<basic_sudoku_board<S>>;

    object_pool<board_set, 8> board_set_pool;

    flat_map<sudoku_solver_key, pool_object<board_set, 8>> key_boards;

    flat_map<sudoku_solver_key, std::chrono::steady_clock::time_point>
      key_starts;

    struct pending_info {
        pending_info(basic_sudoku_board<S> b)
          : board{std::move(b)} {}

        basic_sudoku_board<S> board;
        endpoint_id_t used_helper{0U};
        message_sequence_t sequence_no{0U};
        sudoku_solver_key key{0};
        timeout too_late{};
    };
    std::vector<pending_info> pending;
    std::vector<pending_info> remaining;

    flat_set<endpoint_id_t> known_helpers;
    flat_set<endpoint_id_t> ready_helpers;
    flat_map<endpoint_id_t, std::intmax_t> updated_by_helper;
    flat_map<endpoint_id_t, std::intmax_t> solved_by_helper;
    std::vector<endpoint_id_t> found_helpers;

    std::default_random_engine randeng{std::random_device{}()};

    sudoku_solver_rank_info() noexcept = default;

    auto has_work() const noexcept {
        return not key_boards.empty() or not pending.empty();
    }

    void queue_length_changed(auto& solver) const noexcept;

    void add_board(
      auto& solver,
      const sudoku_solver_key key,
      basic_sudoku_board<S> board) noexcept;

    auto search_helpers(endpoint& bus) noexcept -> work_done;

    auto handle_timeouted(subscriber& base, auto& solver) noexcept -> work_done;

    void process_solved(
      auto& solver,
      const message_context& msg_ctx,
      const message_info& message,
      pending_info& done,
      basic_sudoku_board<S>& board) noexcept;

    void process_unsolved(
      auto& solver,
      pending_info& done,
      basic_sudoku_board<S>& board) noexcept;

    auto process_pending_entry(
      auto& solver,
      const message_context& msg_ctx,
      const message_info& message,
      pending_info& done,
      basic_sudoku_board<S>& board) noexcept -> bool;

    void handle_response(
      auto& solver,
      const message_context& msg_ctx,
      const stored_message& message) noexcept;

    void do_send_board_to(
      auto& solver,
      endpoint& bus,
      data_compressor& compressor,
      const endpoint_id_t helper_id,
      auto key,
      auto& boards) noexcept;

    auto send_board_to(
      auto& solver,
      endpoint& bus,
      data_compressor& compressor,
      const endpoint_id_t helper_id) noexcept -> bool;

    auto find_helpers(span<endpoint_id_t> dst) const noexcept
      -> span<endpoint_id_t>;

    auto send_boards(
      auto& solver,
      endpoint& bus,
      data_compressor& compressor) noexcept -> work_done;

    void pending_done(
      auto& solver,
      const message_sequence_t sequence_no) noexcept;

    void helper_alive(
      auto& solver,
      const message_context& msg_ctx,
      const stored_message& message) noexcept;

    auto has_enqueued(const sudoku_solver_key& key) noexcept -> bool;

    void reset(auto& solver) noexcept;

    auto updated_by_helper_count(const endpoint_id_t helper_id) const noexcept
      -> std::intmax_t {
        return eagine::find(updated_by_helper, helper_id).value_or(0);
    }

    auto updated_count() const noexcept -> std::intmax_t;

    auto solved_by_helper_count(const endpoint_id_t helper_id) const noexcept
      -> std::intmax_t;

    auto solved_count() const noexcept -> std::intmax_t;
};
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_solver_rank_info<S>::queue_length_changed(
  auto& solver) const noexcept {
    if(solver.signals.queue_length_changed) {
        sudoku_board_queue_change change{.rank = S};
        for(const auto& key_and_boards : key_boards) {
            change.key_count++;
            change.board_count += std::get<1>(key_and_boards)->size();
        }
        solver.signals.queue_length_changed(change);
    }
}
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_solver_rank_info<S>::add_board(
  auto& solver,
  const sudoku_solver_key key,
  basic_sudoku_board<S> board) noexcept {
    if(not key_starts.contains(key)) {
        key_starts[key] = std::chrono::steady_clock::now();
    }
    auto spos{key_boards.find(key)};
    if(spos == key_boards.end()) {
        spos = key_boards.emplace(key, board_set_pool).first;
        std::get<1>(*spos)->clear();
    }
    auto& boards{std::get<1>(*spos)};
    const auto bpos{std::upper_bound(
      boards->begin(),
      boards->end(),
      board.alternative_count(),
      [](const auto v, const auto& e) { return v > e.alternative_count(); })};
    boards->emplace(bpos, std::move(board));
    queue_length_changed(solver);
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_solver_rank_info<S>::search_helpers(endpoint& bus) noexcept
  -> work_done {
    some_true something_done;
    if(search_timeout) [[unlikely]] {
        bus.broadcast(sudoku_search_msg(unsigned_constant<S>{}));
        search_timeout.reset();
        something_done();
    }
    return something_done;
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_solver_rank_info<S>::handle_timeouted(
  subscriber& base,
  auto& solver) noexcept -> work_done {
    std::size_t count = 0;
    std::erase_if(pending, [&](auto& entry) {
        if(entry.too_late) {
            const unsigned_constant<S> rank{};
            if(not solver.driver().already_done(entry.key, rank)) {
                add_board(solver, std::move(entry.key), std::move(entry.board));
                ++count;
            }
            known_helpers.erase(entry.used_helper);
            return true;
        }
        return false;
    });
    if(count > 0U) [[unlikely]] {
        base.bus_node()
          .log_warning(
            "replacing ${count} timeouted boards, ${pending} pending")
          .tag("rplcdBords")
          .arg("count", count)
          .arg("enqueued", key_boards.size())
          .arg("pending", pending.size())
          .arg("ready", ready_helpers.size())
          .arg("rank", S);
        sudoku_board_timeout event;
        event.rank = S;
        event.replaced_board_count = count;
        event.enqueued_board_count = key_boards.size();
        event.pending_board_count = pending.size();
        event.ready_helper_count = ready_helpers.size();
        solver.signals.board_timeouted(event);
    }
    return count > 0;
}
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_solver_rank_info<S>::process_solved(
  auto& solver,
  const message_context& msg_ctx,
  const message_info& message,
  pending_info& done,
  basic_sudoku_board<S>& board) noexcept {
    assert(board.is_solved());
    key_boards.erase_if(
      [&](const auto& entry) { return done.key == std::get<0>(entry); });
    queue_length_changed(solver);

    auto helper{eagine::find(solved_by_helper, done.used_helper)};
    if(not helper) {
        helper.reset(solved_by_helper.emplace(done.used_helper, 0L));
    }
    ++(*helper);
    const auto duration{
      std::chrono::steady_clock::now() - key_starts[done.key]};
    key_starts.erase(done.key);
    solver.signals.solved_signal(unsigned_constant<S>{})(
      result_context{msg_ctx, message},
      solved_sudoku_board<S>{
        .helper_id = done.used_helper,
        .key = done.key,
        .elapsed_time = duration,
        .board = board});
    solution_timeout.reset();
}
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_solver_rank_info<S>::process_unsolved(
  auto& solver,
  pending_info& done,
  basic_sudoku_board<S>& board) noexcept {
    add_board(solver, done.key, std::move(board));
    auto helper{eagine::find(updated_by_helper, done.used_helper)};
    if(not helper) {
        helper.reset(updated_by_helper.emplace(done.used_helper, 0L));
    }
    ++(*helper);
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_solver_rank_info<S>::process_pending_entry(
  auto& solver,
  const message_context& msg_ctx,
  const message_info& message,
  pending_info& done,
  basic_sudoku_board<S>& board) noexcept -> bool {
    const unsigned_constant<S> rank{};
    const bool is_solved{msg_ctx.msg_id() == sudoku_solved_msg(rank)};

    if(is_solved) {
        process_solved(solver, msg_ctx, message, done, board);
    } else {
        process_unsolved(solver, done, board);
    }
    done.too_late.reset();
    return is_solved;
}
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_solver_rank_info<S>::handle_response(
  auto& solver,
  const message_context& msg_ctx,
  const stored_message& message) noexcept {
    basic_sudoku_board<S> board{traits};

    const auto deserialized{
      (S >= 4) ? default_deserialize_packed(
                   board, message.content(), solver.compressor)
               : default_deserialize(board, message.content())};

    if(deserialized) [[likely]] {
        const auto predicate{[&](const auto& entry) {
            return entry.sequence_no == message.sequence_no;
        }};
        auto pos = std::find_if(pending.begin(), pending.end(), predicate);

        if(pos != pending.end()) {
            process_pending_entry(solver, msg_ctx, message, *pos, board);
        } else {
            pos = std::find_if(remaining.begin(), remaining.end(), predicate);
            if(pos != remaining.end()) {
                if(process_pending_entry(
                     solver, msg_ctx, message, *pos, board)) {
                    remaining.erase(pos);
                }
            }
        }
    }
}
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_solver_rank_info<S>::do_send_board_to(
  auto& solver,
  endpoint& bus,
  data_compressor& compressor,
  const endpoint_id_t helper_id,
  auto key,
  auto& boards) noexcept {

    assert(not boards->empty());
    auto& board = boards->back();

    serialize_buffer.ensure(default_serialize_buffer_size_for(board));
    const auto serialized{
      (S >= 4)
        ? default_serialize_packed(board, cover(serialize_buffer), compressor)
        : default_serialize(board, cover(serialize_buffer))};
    assert(serialized);

    message_view response{*serialized};
    response.set_target_id(helper_id);
    response.set_sequence_no(query_sequence);
    bus.post(sudoku_query_msg(unsigned_constant<S>{}), response);

    pending.emplace_back(std::move(board));
    auto& query = pending.back();
    query.used_helper = helper_id;
    query.sequence_no = query_sequence;
    query.key = std::move(key);
    query.too_late.reset(default_solution_timeout());
    boards->pop_back();
    queue_length_changed(solver);

    ready_helpers.erase(helper_id);
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_solver_rank_info<S>::send_board_to(
  auto& solver,
  endpoint& bus,
  data_compressor& compressor,
  const endpoint_id_t helper_id) noexcept -> bool {
    if(const auto key_board_count{key_boards.size()}) {
        const auto offs{query_sequence++ % key_board_count};
        const auto kbpos{std::next(key_boards.begin(), offs)};

        auto& [key, boards] = *kbpos;
        if(boards->empty()) {
            key_boards.erase(kbpos);
        } else {
            do_send_board_to(
              solver, bus, compressor, helper_id, std::move(key), boards);
            return true;
        }
    }
    return false;
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_solver_rank_info<S>::find_helpers(
  span<endpoint_id_t> dst) const noexcept -> span<endpoint_id_t> {
    span_size_t done = 0;
    for(const auto helper_id : ready_helpers) {
        if(done < dst.size()) {
            dst[done++] = helper_id;
        } else {
            break;
        }
    }
    return head(dst, done);
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_solver_rank_info<S>::send_boards(
  auto& solver,
  endpoint& bus,
  data_compressor& compressor) noexcept -> work_done {
    some_true something_done;

    if(found_helpers.size() < ready_helpers.size()) {
        found_helpers.resize(ready_helpers.size());
    }

    for(const auto helper_id :
        head(shuffle(find_helpers(cover(found_helpers)), randeng), 8)) {
        if(not send_board_to(solver, bus, compressor, helper_id)) {
            break;
        }
        something_done();
    }

    return something_done;
}
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_solver_rank_info<S>::pending_done(
  auto& solver,
  const message_sequence_t sequence_no) noexcept {
    const auto pos =
      std::find_if(pending.begin(), pending.end(), [&](const auto& entry) {
          return entry.sequence_no == sequence_no;
      });

    if(pos != pending.end()) {
        ready_helpers.insert(pos->used_helper);
        const unsigned_constant<S> rank{};
        if(solver.driver().already_done(pos->key, rank)) {
            std::erase_if(remaining, [&](const auto& entry) {
                return entry.key == pos->key;
            });
        } else {
            remaining.emplace_back(std::move(*pos));
        }
        pending.erase(pos);
    }
}
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_solver_rank_info<S>::helper_alive(
  auto& solver,
  const message_context& msg_ctx,
  const stored_message& message) noexcept {
    if(std::get<1>(known_helpers.insert(message.source_id))) {
        solver.signals.helper_appeared(
          result_context{msg_ctx, message},
          sudoku_helper_appeared{.helper_id = message.source_id});
    }
    ready_helpers.insert(message.source_id);
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_solver_rank_info<S>::has_enqueued(
  const sudoku_solver_key& key) noexcept -> bool {
    return std::find_if(
             key_boards.begin(),
             key_boards.end(),
             [&](const auto& entry) { return std::get<0>(entry) == key; }) !=
             key_boards.end() or
           std::find_if(pending.begin(), pending.end(), [&](const auto& entry) {
               return entry.key == key;
           }) != pending.end();
}
//------------------------------------------------------------------------------
template <unsigned S>
void sudoku_solver_rank_info<S>::reset(auto& solver) noexcept {
    key_starts.clear();
    key_boards.clear();
    pending.clear();
    remaining.clear();
    solution_timeout.reset();

    queue_length_changed(solver);
    solver.base.bus_node().log_info("reset Sudoku solution").arg("rank", S);
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_solver_rank_info<S>::updated_count() const noexcept
  -> std::intmax_t {
    return std::accumulate(
      updated_by_helper.begin(),
      updated_by_helper.end(),
      static_cast<std::intmax_t>(0),
      [](const auto s, const auto& e) { return s + e.second; });
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_solver_rank_info<S>::solved_by_helper_count(
  const endpoint_id_t helper_id) const noexcept -> std::intmax_t {
    return eagine::find(solved_by_helper, helper_id).value_or(0);
}
//------------------------------------------------------------------------------
template <unsigned S>
auto sudoku_solver_rank_info<S>::solved_count() const noexcept
  -> std::intmax_t {
    return std::accumulate(
      solved_by_helper.begin(),
      solved_by_helper.end(),
      static_cast<std::intmax_t>(0),
      [](const auto s, const auto& e) { return s + e.second; });
}
//------------------------------------------------------------------------------
// sudoku_solver_impl
//------------------------------------------------------------------------------
class sudoku_solver_impl : public sudoku_solver_intf {
    using This = sudoku_solver_impl;

    sudoku_solver_driver _default_driver;

public:
    sudoku_solver_impl(subscriber& sub, sudoku_solver_signals& sigs) noexcept
      : base{sub}
      , signals{sigs}
      , compressor{base.bus_node().main_context().buffers()} {}

    auto driver() const noexcept -> sudoku_solver_driver& {
        return *_pdriver;
    }
    void assign_driver(sudoku_solver_driver&) noexcept final;
    void add_methods() noexcept final;
    void init() noexcept final;
    auto update() noexcept -> work_done final;

    void enqueue(sudoku_solver_key key, basic_sudoku_board<3> board) noexcept
      final {
        _infos.get(unsigned_constant<3>{})
          .add_board(*this, std::move(key), std::move(board));
    }
    void enqueue(sudoku_solver_key key, basic_sudoku_board<4> board) noexcept
      final {
        _infos.get(unsigned_constant<4>{})
          .add_board(*this, std::move(key), std::move(board));
    }
    void enqueue(sudoku_solver_key key, basic_sudoku_board<5> board) noexcept
      final {
        _infos.get(unsigned_constant<5>{})
          .add_board(*this, std::move(key), std::move(board));
    }
    void enqueue(sudoku_solver_key key, basic_sudoku_board<6> board) noexcept
      final {
        _infos.get(unsigned_constant<6>{})
          .add_board(*this, std::move(key), std::move(board));
    }

    auto has_work() const noexcept -> bool final;
    void reset(unsigned) noexcept override;
    auto has_enqueued(const sudoku_solver_key&, unsigned) noexcept
      -> bool final;

    void set_solution_timeout(
      unsigned rank,
      const std::chrono::seconds sec) noexcept final;

    void reset_solution_timeout(unsigned rank) noexcept final;

    auto solution_timeouted(unsigned rank) const noexcept -> bool final;

    auto updated_by_helper(const endpoint_id_t helper_id, const unsigned rank)
      const noexcept -> std::intmax_t final;

    auto updated_count(const unsigned rank) const noexcept
      -> std::intmax_t final;

    auto solved_by_helper(const endpoint_id_t helper_id, const unsigned rank)
      const noexcept -> std::intmax_t final;

    auto solved_count(const unsigned rank) const noexcept
      -> std::intmax_t final;

    subscriber& base;
    sudoku_solver_signals& signals;
    data_compressor compressor;

private:
    void _on_id_assigned(const endpoint_id_t) noexcept {
        _can_work = true;
    }

    void _on_connection_established(const bool usable) noexcept {
        _can_work = usable;
    }

    void _on_connection_lost() noexcept {
        _can_work = false;
    }

    template <unsigned S>
    auto _handle_alive(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool {
        _infos.get(unsigned_constant<S>{}).helper_alive(*this, msg_ctx, message);
        return true;
    }

    template <unsigned S>
    static constexpr auto _bind_handle_alive(
      const unsigned_constant<S> rank) noexcept {
        return message_handler_map<member_function_constant<
          bool (This::*)(const message_context&, const stored_message&) noexcept,
          &This::_handle_alive<S>>>{sudoku_alive_msg(rank)};
    }

    template <unsigned S>
    auto _handle_board(
      const message_context& msg_ctx,
      const stored_message& message) noexcept -> bool {

        _infos.get(unsigned_constant<S>{})
          .handle_response(*this, msg_ctx, message);
        return true;
    }

    template <unsigned S>
    static constexpr auto _bind_handle_candidate(
      const unsigned_constant<S> rank) noexcept {
        return message_handler_map<member_function_constant<
          bool (This::*)(const message_context&, const stored_message&) noexcept,
          &This::_handle_board<S>>>{sudoku_candidate_msg(rank)};
    }

    template <unsigned S>
    static constexpr auto _bind_handle_solved(
      const unsigned_constant<S> rank) noexcept {
        return message_handler_map<member_function_constant<
          bool (This::*)(const message_context&, const stored_message&) noexcept,
          &This::_handle_board<S>>>{sudoku_solved_msg(rank)};
    }

    template <unsigned S>
    auto _handle_done(
      const message_context&,
      const stored_message& message) noexcept -> bool {
        _infos.get(unsigned_constant<S>{})
          .pending_done(*this, message.sequence_no);
        return true;
    }

    template <unsigned S>
    static constexpr auto _bind_handle_done(
      const unsigned_constant<S> rank) noexcept {
        return message_handler_map<member_function_constant<
          bool (This::*)(const message_context&, const stored_message&) noexcept,
          &This::_handle_done<S>>>{sudoku_done_msg(rank)};
    }

    sudoku_rank_tuple<sudoku_solver_rank_info> _infos;
    sudoku_solver_driver* _pdriver{&_default_driver};
    bool _can_work{false};
};
//------------------------------------------------------------------------------
auto make_sudoku_solver_impl(subscriber& base, sudoku_solver_signals& signals)
  -> unique_holder<sudoku_solver_intf> {
    return {hold<sudoku_solver_impl>, base, signals};
}
//------------------------------------------------------------------------------
void sudoku_solver_impl::assign_driver(sudoku_solver_driver& drvr) noexcept {
    _pdriver = &drvr;
}
//------------------------------------------------------------------------------
void sudoku_solver_impl::add_methods() noexcept {
    sudoku_rank_tuple<unsigned_constant> ranks;
    for_each_sudoku_rank_unit(
      [&](auto rank) {
          base.add_method(this, _bind_handle_alive(rank));
          base.add_method(this, _bind_handle_candidate(rank));
          base.add_method(this, _bind_handle_solved(rank));
          base.add_method(this, _bind_handle_done(rank));
      },
      ranks);
}
//------------------------------------------------------------------------------
void sudoku_solver_impl::init() noexcept {
    connect<&This::_on_id_assigned>(this, base.bus_node().id_assigned);
    connect<&This::_on_connection_established>(
      this, base.bus_node().connection_established);
    connect<&This::_on_connection_lost>(this, base.bus_node().connection_lost);

    if(const auto solution_timeout{base.app_config().get(
         "msgbus.sudoku.solver.solution_timeout",
         std::type_identity<std::chrono::seconds>{})}) {
        base.bus_node()
          .log_info("setting solution timeout to ${timeout}")
          .tag("sdkuSolTmt")
          .arg("timeout", *solution_timeout);
        for_each_sudoku_rank_unit(
          [&](auto& info) { info.solution_timeout.reset(*solution_timeout); },
          _infos);
    }
}
//------------------------------------------------------------------------------
auto sudoku_solver_impl::update() noexcept -> work_done {
    some_true something_done{};

    for_each_sudoku_rank_unit(
      [&](auto& info) {
          something_done(info.handle_timeouted(base, *this));
          if(_can_work) [[likely]] {
              if(driver().should_send_boards()) [[likely]] {
                  something_done(
                    info.send_boards(*this, base.bus_node(), compressor));
              }
              something_done(info.search_helpers(base.bus_node()));
          }
      },
      _infos);

    return something_done;
}
//------------------------------------------------------------------------------
auto sudoku_solver_impl::has_work() const noexcept -> bool {
    bool result = false;
    for_each_sudoku_rank_unit(
      [&](const auto& info) { result |= info.has_work(); }, _infos);

    return result;
}
//------------------------------------------------------------------------------
void sudoku_solver_impl::reset(unsigned rank) noexcept {
    apply_to_sudoku_rank_unit(
      rank, [rank, this](auto& info) { info.reset(*this); }, _infos);
}
//------------------------------------------------------------------------------
auto sudoku_solver_impl::has_enqueued(
  const sudoku_solver_key& key,
  unsigned rank) noexcept -> bool {
    bool result = false;
    apply_to_sudoku_rank_unit(
      rank, [&](auto& info) { result |= info.has_enqueued(key); }, _infos);
    return result;
}
//------------------------------------------------------------------------------
void sudoku_solver_impl::set_solution_timeout(
  unsigned rank,
  const std::chrono::seconds sec) noexcept {
    apply_to_sudoku_rank_unit(
      rank, [=](auto& info) { info.solution_timeout.reset(sec); }, _infos);
}
//------------------------------------------------------------------------------
void sudoku_solver_impl::reset_solution_timeout(unsigned rank) noexcept {
    apply_to_sudoku_rank_unit(
      rank, [](auto& info) { info.solution_timeout.reset(); }, _infos);
}
//------------------------------------------------------------------------------
auto sudoku_solver_impl::solution_timeouted(unsigned rank) const noexcept
  -> bool {
    bool result = false;
    apply_to_sudoku_rank_unit(
      rank,
      [&](const auto& info) { result |= info.solution_timeout.is_expired(); },
      _infos);
    return result;
}
//------------------------------------------------------------------------------
auto sudoku_solver_impl::updated_by_helper(
  const endpoint_id_t helper_id,
  const unsigned rank) const noexcept -> std::intmax_t {
    std::intmax_t result{0};
    apply_to_sudoku_rank_unit(
      rank,
      [&](const auto& info) {
          result += info.updated_by_helper_count(helper_id);
      },
      _infos);
    return result;
}
//------------------------------------------------------------------------------
auto sudoku_solver_impl::updated_count(const unsigned rank) const noexcept
  -> std::intmax_t {
    std::intmax_t result{0};
    apply_to_sudoku_rank_unit(
      rank, [&](const auto& info) { result += info.updated_count(); }, _infos);
    return result;
}
//------------------------------------------------------------------------------
auto sudoku_solver_impl::solved_by_helper(
  const endpoint_id_t helper_id,
  const unsigned rank) const noexcept -> std::intmax_t {
    std::intmax_t result{0};
    apply_to_sudoku_rank_unit(
      rank,
      [&](const auto& info) {
          result += info.solved_by_helper_count(helper_id);
      },
      _infos);
    return result;
}
//------------------------------------------------------------------------------
auto sudoku_solver_impl::solved_count(const unsigned rank) const noexcept
  -> std::intmax_t {
    std::intmax_t result{0};
    apply_to_sudoku_rank_unit(
      rank, [&](const auto& info) { result += info.solved_count(); }, _infos);
    return result;
}
//------------------------------------------------------------------------------
// sudoku_tiling_rank_info
//------------------------------------------------------------------------------
template <unsigned S>
struct sudoku_tiling_rank_info : sudoku_tiles<S> {
    using Coord = std::tuple<int, int>;

    void initialize(
      auto& tiling,
      const int x,
      const int y,
      basic_sudoku_board<S> board) noexcept {
        tiling.solver.enqueue(std::make_tuple(x, y), std::move(board));
        tiling.solver.base.bus_node()
          .log_debug("enqueuing initial board (${x}, ${y})")
          .arg("x", x)
          .arg("y", y)
          .arg("rank", S)
          .arg("progress", "MainPrgrss", 0.F, 0.F, float(this->cell_count()));
        cells_done = 0;
    }

    void do_enqueue(auto& tiling, const int x, const int y) noexcept {
        auto board{this->new_board()};
        bool should_enqueue = false;
        if(y > 0) {
            if(x > 0) {
                const auto left{this->get_board(x - 1, y)};
                const auto down{this->get_board(x, y - 1)};
                if(left and down) {
                    for(const auto by : integer_range(S - 1U)) {
                        board.set_block(0U, by, left->get_block(S - 1U, by));
                    }
                    for(const auto bx : integer_range(1U, S)) {
                        board.set_block(bx, S - 1U, down->get_block(bx, 0U));
                    }
                    should_enqueue = true;
                }
            } else if(x < 0) {
                const auto right{this->get_board(x + 1, y)};
                const auto down{this->get_board(x, y - 1)};
                if(right and down) {
                    for(const auto by : integer_range(S - 1U)) {
                        board.set_block(S - 1U, by, right->get_block(0U, by));
                    }
                    for(const auto bx : integer_range(S - 1U)) {
                        board.set_block(bx, S - 1U, down->get_block(bx, 0U));
                    }
                    should_enqueue = true;
                }
            } else {
                const auto down{this->get_board(x, y - 1)};
                if(down) {
                    for(const auto bx : integer_range(S)) {
                        board.set_block(bx, S - 1U, down->get_block(bx, 0U));
                    }
                    should_enqueue = true;
                }
            }
        } else if(y < 0) {
            if(x > 0) {
                const auto left{this->get_board(x - 1, y)};
                const auto up{this->get_board(x, y + 1)};
                if(left and up) {
                    for(const auto by : integer_range(1U, S)) {
                        board.set_block(0U, by, left->get_block(S - 1U, by));
                    }
                    for(const auto bx : integer_range(1U, S)) {
                        board.set_block(bx, 0U, up->get_block(bx, S - 1U));
                    }
                    should_enqueue = true;
                }
            } else if(x < 0) {
                const auto right{this->get_board(x + 1, y)};
                const auto up{this->get_board(x, y + 1)};
                if(right and up) {
                    for(const auto by : integer_range(1U, S)) {
                        board.set_block(S - 1U, by, right->get_block(0U, by));
                    }
                    for(const auto bx : integer_range(S - 1U)) {
                        board.set_block(bx, 0U, up->get_block(bx, S - 1U));
                    }
                    should_enqueue = true;
                }
            } else {
                const auto up{this->get_board(x, y + 1)};
                if(up) {
                    for(const auto bx : integer_range(S)) {
                        board.set_block(bx, 0U, up->get_block(bx, S - 1U));
                    }
                    should_enqueue = true;
                }
            }
        } else {
            if(x > 0) {
                const auto left{this->get_board(x - 1, y)};
                if(left) {
                    for(const auto by : integer_range(S)) {
                        board.set_block(0U, by, left->get_block(S - 1U, by));
                    }
                    should_enqueue = true;
                }
            } else if(x < 0) {
                const auto right{this->get_board(x + 1, y)};
                if(right) {
                    for(const auto by : integer_range(S)) {
                        board.set_block(S - 1U, by, right->get_block(0U, by));
                    }
                    should_enqueue = true;
                }
            }
        }
        if(should_enqueue) {
            tiling.solver.enqueue(Coord{x, y}, board.calculate_alternatives());
            tiling.solver.base.bus_node()
              .log_debug("enqueuing board (${x}, ${y})")
              .arg("x", x)
              .arg("y", y)
              .arg("rank", S);
        }
    }

    void enqueue_incomplete(auto& tiling) noexcept {
        const unsigned_constant<S> rank{};
        const auto [xmin, ymin, xmax, ymax] = this->boards_extent();
        for(const auto y : integer_range(ymin, ymax)) {
            for(const auto x : integer_range(xmin, xmax)) {
                if(not this->get_board(x, y)) {
                    if(not tiling.solver.has_enqueued(Coord{x, y}, rank)) {
                        do_enqueue(tiling, x, y);
                    }
                }
            }
        }
    }

    void handle_solved(
      auto& tiling,
      const solved_sudoku_board<S>& sol) noexcept {

        const auto coord{std::get<Coord>(sol.key)};
        if(this->set_board(coord, sol.board)) {
            cells_done += this->cells_per_tile(coord);
            tiling.solver.base.bus_node()
              .log_info("solved board [${x}, ${y}], ${progress} done")
              .tag("solvdBoard")
              .arg("rank", S)
              .arg("x", std::get<0>(coord))
              .arg("y", std::get<1>(coord))
              .arg("width", this->width())
              .arg("height", this->height())
              .arg("time", sol.elapsed_time)
              .arg("helper", sol.helper_id)
              .arg(
                "progress",
                "MainPrgrss",
                0.F,
                float(cells_done),
                float(this->cell_count()));

            auto helper{eagine::find(helper_contrib, sol.helper_id)};
            if(not helper) {
                helper.reset(helper_contrib.emplace(sol.helper_id, 0));
            }
            ++(*helper);

            const unsigned_constant<S> rank{};
            tiling.signals.tiles_generated_signal(rank)(
              sol.helper_id, *this, coord);
        }

        enqueue_incomplete(tiling);
    }

    void log_contribution_histogram(auto& tiling) noexcept {
        span_size_t max_count = 0;
        for(const auto& p : helper_contrib) {
            max_count = std::max(max_count, std::get<1>(p));
        }
        tiling.solver.base.bus_node()
          .log_stat("solution contributions by helpers")
          .tag("hlprContrb")
          .arg("rank", S)
          .arg_func([this, max_count](logger_backend& backend) {
              for(const auto& [helper_id, count] : helper_contrib) {
                  backend.add_float(
                    "helper",
                    "Histogram",
                    float(0),
                    float(count),
                    float(max_count));
              }
          });
    }

    auto solution_progress() const noexcept -> float {
        return float(cells_done) / float(this->cell_count());
    }

    auto tiling_size() const noexcept -> std::tuple<int, int> {
        return {this->x_tiles_count(), this->y_tiles_count()};
    }

    flat_map<endpoint_id_t, span_size_t> helper_contrib;
    int cells_done{0};
};
//------------------------------------------------------------------------------
// sudoku_tiling_impl
//------------------------------------------------------------------------------
class sudoku_tiling_impl
  : public sudoku_solver_driver
  , public sudoku_tiling_intf {
    using This = sudoku_tiling_impl;

public:
    using Coord = std::tuple<int, int>;

    sudoku_tiling_impl(
      sudoku_solver_impl& impl,
      sudoku_tiling_signals& sigs) noexcept
      : solver{impl}
      , signals{sigs} {
        connect<&This::handle_solved<3>>(this, solver.signals.solved_3);
        connect<&This::handle_solved<4>>(this, solver.signals.solved_4);
        connect<&This::handle_solved<5>>(this, solver.signals.solved_5);
        connect<&This::handle_solved<6>>(this, solver.signals.solved_6);
    }

    auto already_done(
      const sudoku_solver_key& coord,
      const unsigned_constant<3> rank) noexcept -> bool final {
        return _is_already_done(coord, rank);
    }
    auto already_done(
      const sudoku_solver_key& coord,
      const unsigned_constant<4> rank) noexcept -> bool final {
        return _is_already_done(coord, rank);
    }
    auto already_done(
      const sudoku_solver_key& coord,
      const unsigned_constant<5> rank) noexcept -> bool final {
        return _is_already_done(coord, rank);
    }
    auto already_done(
      const sudoku_solver_key& coord,
      const unsigned_constant<6> rank) noexcept -> bool final {
        return _is_already_done(coord, rank);
    }

    auto should_send_boards() noexcept -> bool final {
        const bool is_suspended{not _suspended.is_expired()};
        if(is_suspended) {
            if(not _was_suspended) {
                solver.base.bus_node()
                  .log_change("suspending sending of boards for ${interval}")
                  .tag("suspndSend")
                  .arg("interval", _suspended.period());
            }
        } else {
            if(_was_suspended) {
                solver.base.bus_node()
                  .log_change("resuming sending of boards to helpers")
                  .tag("rsumedSend");
            }
        }
        _was_suspended = is_suspended;
        return not is_suspended;
    }

    template <unsigned S>
    auto _is_already_done(
      const sudoku_solver_key& coord,
      const unsigned_constant<S> rank) const noexcept -> bool {
        return bool(_infos.get(rank).get_board(std::get<Coord>(coord)));
    }

    template <unsigned S>
    void handle_solved(
      const result_context&,
      const solved_sudoku_board<S>& sol) noexcept {
        auto& info = _infos.get(unsigned_constant<S>{});
        info.handle_solved(*this, sol);
    }

    auto driver() noexcept -> sudoku_solver_driver& final {
        return *this;
    }

    template <unsigned S>
    void do_initialize(
      const sudoku_solver_key min,
      const sudoku_solver_key max,
      const sudoku_solver_key coord,
      basic_sudoku_board<S> board) noexcept {
        const auto [x, y] = std::get<Coord>(coord);
        auto& info = _infos.get(unsigned_constant<S>{});
        info.set_extent(std::get<Coord>(min), std::get<Coord>(max));
        info.initialize(*this, x, y, std::move(board));
    }

    void initialize(
      const sudoku_solver_key min,
      const sudoku_solver_key max,
      const sudoku_solver_key coord,
      basic_sudoku_board<3> board) noexcept final {
        do_initialize(min, max, coord, board);
    }

    void initialize(
      const sudoku_solver_key min,
      const sudoku_solver_key max,
      const sudoku_solver_key coord,
      basic_sudoku_board<4> board) noexcept final {
        do_initialize(min, max, coord, board);
    }

    void initialize(
      const sudoku_solver_key min,
      const sudoku_solver_key max,
      const sudoku_solver_key coord,
      basic_sudoku_board<5> board) noexcept final {
        do_initialize(min, max, coord, board);
    }

    void initialize(
      const sudoku_solver_key min,
      const sudoku_solver_key max,
      const sudoku_solver_key coord,
      basic_sudoku_board<6> board) noexcept final {
        do_initialize(min, max, coord, board);
    }

    void suspend_send_for(std::chrono::milliseconds interval) noexcept final {
        _suspended.reset(interval);
    }

    void reset(unsigned rank) noexcept final {
        apply_to_sudoku_rank_unit(
          rank, [](auto& info) { info.reset(); }, _infos);
    }

    auto are_complete() const noexcept -> bool {
        bool result = true;
        for_each_sudoku_rank_unit(
          [&](auto& info) { result &= info.are_complete(); }, _infos);
        return result;
    }

    auto are_complete(unsigned rank) const noexcept -> bool final {
        bool result = false;
        apply_to_sudoku_rank_unit(
          rank, [&](auto& info) { result |= info.are_complete(); }, _infos);
        return result;
    }

    auto solution_progress(unsigned rank) const noexcept -> float final {
        float result{0.F};
        apply_to_sudoku_rank_unit(
          rank, [&](auto& info) { result = info.solution_progress(); }, _infos);
        return result;
    }

    auto tiling_size(unsigned rank) const noexcept
      -> std::tuple<int, int> final {
        std::tuple<int, int> result{0, 0};
        apply_to_sudoku_rank_unit(
          rank, [&](auto& info) { result = info.tiling_size(); }, _infos);
        return result;
    }

    void log_contribution_histogram(unsigned rank) noexcept {
        apply_to_sudoku_rank_unit(
          rank,
          [this](auto& info) { info.log_contribution_histogram(*this); },
          _infos);
    }

    sudoku_solver_impl& solver;
    sudoku_tiling_signals& signals;

private:
    sudoku_rank_tuple<sudoku_tiling_rank_info> _infos;
    timeout _suspended{std::chrono::seconds{0}, nothing};
    bool _was_suspended{false};
};
//------------------------------------------------------------------------------
auto make_sudoku_tiling_impl(
  sudoku_solver_intf& solver,
  sudoku_tiling_signals& tsigs) -> unique_holder<sudoku_tiling_intf> {
    return {
      hold<sudoku_tiling_impl>,
      static_cast<sudoku_solver_impl&>(solver),
      tsigs};
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
