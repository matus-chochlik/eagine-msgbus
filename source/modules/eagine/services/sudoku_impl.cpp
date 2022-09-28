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
  -> std::unique_ptr<sudoku_helper_intf> {
    return std::make_unique<sudoku_helper_impl>(base);
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
              .arg("recursion", extract(max_recursion));
            for_each_sudoku_rank_unit(
              [&](auto& info) { info.max_recursion = extract(max_recursion); },
              _infos);
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
    timeout solution_timeout{
      adjusted_duration(std::chrono::seconds{S * S * S * S})};

    flat_map<sudoku_solver_key, std::vector<basic_sudoku_board<S>>> key_boards;

    struct pending_info {
        pending_info(basic_sudoku_board<S> b)
          : board{std::move(b)} {}

        basic_sudoku_board<S> board;
        identifier_t used_helper{0U};
        message_sequence_t sequence_no{0U};
        sudoku_solver_key key{0};
        timeout too_late{};
    };
    std::vector<pending_info> pending;
    std::vector<pending_info> remaining;

    flat_set<identifier_t> known_helpers;
    flat_set<identifier_t> ready_helpers;
    flat_map<identifier_t, timeout> used_helpers;
    flat_map<identifier_t, std::intmax_t> updated_by_helper;
    flat_map<identifier_t, std::intmax_t> solved_by_helper;
    std::vector<identifier_t> found_helpers;

    std::default_random_engine randeng{std::random_device{}()};

    sudoku_solver_rank_info() noexcept = default;

    auto has_work() const noexcept {
        return !key_boards.empty() || !pending.empty();
    }

    void queue_length_changed(auto& solver) const noexcept {
        std::size_t key_count{0};
        std::size_t board_count{0};
        for(const auto& entry : key_boards) {
            key_count++;
            board_count += std::get<1>(entry).size();
        }
        solver.signals.queue_length_changed(S, key_count, board_count);
    }

    void add_board(
      auto& solver,
      const sudoku_solver_key key,
      basic_sudoku_board<S> board) noexcept {
        const auto alternative_count = board.alternative_count();
        auto& boards = key_boards[key];
        auto pos = std::lower_bound(
          boards.begin(),
          boards.end(),
          alternative_count,
          [=](const auto& entry, auto value) {
              return entry.alternative_count() > value;
          });
        boards.emplace(pos, std::move(board));
        queue_length_changed(solver);
    }

    auto search_helpers(endpoint& bus) noexcept -> work_done {
        some_true something_done;
        if(search_timeout) [[unlikely]] {
            bus.broadcast(sudoku_search_msg(unsigned_constant<S>{}));
            search_timeout.reset();
            something_done();
        }
        return something_done;
    }

    auto handle_timeouted(subscriber& base, auto& solver) noexcept
      -> work_done {
        span_size_t count = 0;
        std::erase_if(pending, [&](auto& entry) {
            if(entry.too_late) {
                const unsigned_constant<S> rank{};
                if(!solver.driver().already_done(entry.key, rank)) {
                    entry.board.for_each_alternative(
                      entry.board.find_unsolved(), [&](auto& candidate) {
                          if(candidate.is_solved()) {
                              solver.signals.solved_signal(rank)(
                                entry.used_helper, entry.key, candidate);
                          } else {
                              add_board(
                                solver,
                                std::move(entry.key),
                                std::move(candidate));
                              ++count;
                          }
                      });
                }
                known_helpers.erase(entry.used_helper);
                used_helpers.erase(entry.used_helper);
                return true;
            }
            return false;
        });
        if(count > 0) [[unlikely]] {
            base.bus_node()
              .log_warning("replacing ${count} timeouted boards")
              .arg("count", count)
              .arg("enqueued", key_boards.size())
              .arg("pending", pending.size())
              .arg("ready", ready_helpers.size())
              .arg("rank", S);
        }
        return count > 0;
    }

    auto process_pending_entry(
      auto& solver,
      const message_id msg_id,
      pending_info& done,
      basic_sudoku_board<S>& board) noexcept -> bool {
        const unsigned_constant<S> rank{};
        const bool is_solved = msg_id == sudoku_solved_msg(rank);

        if(is_solved) {
            assert(board.is_solved());
            key_boards.erase_if([&](const auto& entry) {
                return done.key == std::get<0>(entry);
            });
            queue_length_changed(solver);

            auto spos = solved_by_helper.find(done.used_helper);
            if(spos == solved_by_helper.end()) {
                spos = solved_by_helper.emplace(done.used_helper, 0L).first;
            }
            spos->second++;
            solver.signals.solved_signal(rank)(
              done.used_helper, done.key, board);
            solution_timeout.reset();
        } else {
            add_board(solver, done.key, std::move(board));
            auto upos = updated_by_helper.find(done.used_helper);
            if(upos == updated_by_helper.end()) {
                upos = updated_by_helper.emplace(done.used_helper, 0L).first;
            }
            upos->second++;
        }
        done.too_late.reset();
        return is_solved;
    }

    void handle_response(
      auto& solver,
      const message_id msg_id,
      const stored_message& message) noexcept {
        basic_sudoku_board<S> board{traits};

        const auto deserialized{
          (S >= 4) ? default_deserialize_packed(
                       board, message.content(), solver.compressor)
                   : default_deserialize(board, message.content())};

        if(deserialized) [[likely]] {
            const auto predicate = [&](const auto& entry) {
                return entry.sequence_no == message.sequence_no;
            };
            auto pos = std::find_if(pending.begin(), pending.end(), predicate);

            if(pos != pending.end()) {
                process_pending_entry(solver, msg_id, *pos, board);
            } else {
                pos =
                  std::find_if(remaining.begin(), remaining.end(), predicate);
                if(pos != remaining.end()) {
                    if(process_pending_entry(solver, msg_id, *pos, board)) {
                        remaining.erase(pos);
                    }
                }
            }
        }
    }

    auto send_board_to(
      auto& solver,
      endpoint& bus,
      data_compressor& compressor,
      const identifier_t helper_id) noexcept -> bool {
        if(!key_boards.empty()) {
            const auto kbpos =
              std::next(key_boards.begin(), query_sequence % key_boards.size());
            assert(kbpos < key_boards.end());
            auto& [key, boards] = *kbpos;

            const auto pos = std::prev(boards.end(), 1);
            auto& board = *pos;
            serialize_buffer.ensure(default_serialize_buffer_size_for(board));
            const auto serialized{
              (S >= 4) ? default_serialize_packed(
                           board, cover(serialize_buffer), compressor)
                       : default_serialize(board, cover(serialize_buffer))};
            assert(serialized);

            const auto sequence_no = query_sequence++;
            message_view response{extract(serialized)};
            response.set_target_id(helper_id);
            response.set_sequence_no(sequence_no);
            bus.post(sudoku_query_msg(unsigned_constant<S>{}), response);

            pending.emplace_back(std::move(board));
            auto& query = pending.back();
            query.used_helper = helper_id;
            query.sequence_no = sequence_no;
            query.key = std::move(key);
            query.too_late.reset(
              adjusted_duration(std::chrono::seconds{S * S * S}));
            boards.erase(pos);
            if(boards.empty()) {
                key_boards.erase(kbpos);
            }
            queue_length_changed(solver);

            ready_helpers.erase(helper_id);
            used_helpers[helper_id].reset(
              adjusted_duration(std::chrono::seconds{S}));
            return true;
        }
        return false;
    }

    auto find_helpers(span<identifier_t> dst) const noexcept
      -> span<identifier_t> {
        span_size_t done = 0;
        for(const auto helper_id : ready_helpers) {
            if(done < dst.size()) {
                const auto upos = used_helpers.find(helper_id);
                const auto is_usable =
                  upos != used_helpers.end() ? upos->second.is_expired() : true;
                if(is_usable) {
                    dst[done++] = helper_id;
                }
            } else {
                break;
            }
        }
        return head(dst, done);
    }

    auto send_boards(
      auto& solver,
      endpoint& bus,
      data_compressor& compressor) noexcept -> work_done {
        some_true something_done;

        if(found_helpers.size() < ready_helpers.size()) {
            found_helpers.resize(ready_helpers.size());
        }

        for(const auto helper_id :
            head(shuffle(find_helpers(cover(found_helpers)), randeng), 8)) {
            if(!send_board_to(solver, bus, compressor, helper_id)) {
                break;
            }
            something_done();
        }

        return something_done;
    }

    void pending_done(
      auto& solver,
      const message_sequence_t sequence_no) noexcept {
        const auto pos =
          std::find_if(pending.begin(), pending.end(), [&](const auto& entry) {
              return entry.sequence_no == sequence_no;
          });

        if(pos != pending.end()) {
            ready_helpers.insert(pos->used_helper);
            used_helpers.erase(pos->used_helper);
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

    void helper_alive(auto& solver, const identifier_t id) noexcept {
        if(std::get<1>(known_helpers.insert(id))) {
            solver.signals.helper_appeared(id);
        }
        ready_helpers.insert(id);
    }

    auto has_enqueued(const sudoku_solver_key& key) noexcept -> bool {
        return std::find_if(
                 key_boards.begin(),
                 key_boards.end(),
                 [&](const auto& entry) {
                     return std::get<0>(entry) == key;
                 }) != key_boards.end() ||
               std::find_if(
                 pending.begin(), pending.end(), [&](const auto& entry) {
                     return entry.key == key;
                 }) != pending.end();
    }

    void reset(auto& solver) noexcept {
        key_boards.clear();
        pending.clear();
        remaining.clear();
        used_helpers.clear();
        solution_timeout.reset();

        queue_length_changed(solver);
        solver.base.bus_node().log_info("reset Sudoku solution").arg("rank", S);
    }

    auto updated_by_helper_count(const identifier_t helper_id) const noexcept
      -> std::intmax_t {
        const auto pos = updated_by_helper.find(helper_id);
        if(pos != updated_by_helper.end()) [[likely]] {
            return pos->second;
        }
        return 0;
    }

    auto updated_count() const noexcept -> std::intmax_t {
        return std::accumulate(
          updated_by_helper.begin(),
          updated_by_helper.end(),
          static_cast<std::intmax_t>(0),
          [](const auto s, const auto& e) { return s + e.second; });
    }

    auto solved_by_helper_count(const identifier_t helper_id) const noexcept
      -> std::intmax_t {
        const auto pos = solved_by_helper.find(helper_id);
        if(pos != solved_by_helper.end()) [[likely]] {
            return pos->second;
        }
        return 0;
    }

    auto solved_count() const noexcept -> std::intmax_t {
        return std::accumulate(
          solved_by_helper.begin(),
          solved_by_helper.end(),
          static_cast<std::intmax_t>(0),
          [](const auto s, const auto& e) { return s + e.second; });
    }
};
//------------------------------------------------------------------------------
// sudoku_solver_impl
//------------------------------------------------------------------------------
class sudoku_solver_impl : public sudoku_solver_intf {
    using This = sudoku_solver_impl;

    [[no_unique_address]] sudoku_solver_driver _default_driver;

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

    auto solution_timeouted(unsigned rank) const noexcept -> bool final;

    auto updated_by_helper(const identifier_t helper_id, const unsigned rank)
      const noexcept -> std::intmax_t final;

    auto updated_count(const unsigned rank) const noexcept
      -> std::intmax_t final;

    auto solved_by_helper(const identifier_t helper_id, const unsigned rank)
      const noexcept -> std::intmax_t final;

    auto solved_count(const unsigned rank) const noexcept
      -> std::intmax_t final;

    subscriber& base;
    sudoku_solver_signals& signals;
    data_compressor compressor;

private:
    void _on_id_assigned(const identifier_t) noexcept {
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
      const message_context&,
      const stored_message& message) noexcept -> bool {
        _infos.get(unsigned_constant<S>{})
          .helper_alive(*this, message.source_id);
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
          .handle_response(*this, msg_ctx.msg_id(), message);
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
  -> std::unique_ptr<sudoku_solver_intf> {
    return std::make_unique<sudoku_solver_impl>(base, signals);
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
          .arg("timeout", extract(solution_timeout));
        for_each_sudoku_rank_unit(
          [&](auto& info) {
              info.solution_timeout.reset(extract(solution_timeout));
          },
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
              something_done(
                info.send_boards(*this, base.bus_node(), compressor));
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
  const identifier_t helper_id,
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
  const identifier_t helper_id,
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
          .arg("rank", S);
        cells_done = 0;
    }

    void do_enqueue(auto& tiling, const int x, const int y) noexcept {
        auto board{this->new_board()};
        bool should_enqueue = false;
        if(y > 0) {
            if(x > 0) {
                const auto left = this->get_board(x - 1, y);
                const auto down = this->get_board(x, y - 1);
                if(left && down) {
                    for(const auto by : integer_range(S - 1U)) {
                        board.set_block(
                          0U, by, extract(left).get_block(S - 1U, by));
                    }
                    for(const auto bx : integer_range(1U, S)) {
                        board.set_block(
                          bx, S - 1U, extract(down).get_block(bx, 0U));
                    }
                    should_enqueue = true;
                }
            } else if(x < 0) {
                const auto right = this->get_board(x + 1, y);
                const auto down = this->get_board(x, y - 1);
                if(right && down) {
                    for(const auto by : integer_range(S - 1U)) {
                        board.set_block(
                          S - 1U, by, extract(right).get_block(0U, by));
                    }
                    for(const auto bx : integer_range(S - 1U)) {
                        board.set_block(
                          bx, S - 1U, extract(down).get_block(bx, 0U));
                    }
                    should_enqueue = true;
                }
            } else {
                const auto down = this->get_board(x, y - 1);
                if(down) {
                    for(const auto bx : integer_range(S)) {
                        board.set_block(
                          bx, S - 1U, extract(down).get_block(bx, 0U));
                    }
                    should_enqueue = true;
                }
            }
        } else if(y < 0) {
            if(x > 0) {
                const auto left = this->get_board(x - 1, y);
                const auto up = this->get_board(x, y + 1);
                if(left && up) {
                    for(const auto by : integer_range(1U, S)) {
                        board.set_block(
                          0U, by, extract(left).get_block(S - 1U, by));
                    }
                    for(const auto bx : integer_range(1U, S)) {
                        board.set_block(
                          bx, 0U, extract(up).get_block(bx, S - 1U));
                    }
                    should_enqueue = true;
                }
            } else if(x < 0) {
                const auto right = this->get_board(x + 1, y);
                const auto up = this->get_board(x, y + 1);
                if(right && up) {
                    for(const auto by : integer_range(1U, S)) {
                        board.set_block(
                          S - 1U, by, extract(right).get_block(0U, by));
                    }
                    for(const auto bx : integer_range(S - 1U)) {
                        board.set_block(
                          bx, 0U, extract(up).get_block(bx, S - 1U));
                    }
                    should_enqueue = true;
                }
            } else {
                const auto up = this->get_board(x, y + 1);
                if(up) {
                    for(const auto bx : integer_range(S)) {
                        board.set_block(
                          bx, 0U, extract(up).get_block(bx, S - 1U));
                    }
                    should_enqueue = true;
                }
            }
        } else {
            if(x > 0) {
                const auto left = this->get_board(x - 1, y);
                if(left) {
                    for(const auto by : integer_range(S)) {
                        board.set_block(
                          0U, by, extract(left).get_block(S - 1U, by));
                    }
                    should_enqueue = true;
                }
            } else if(x < 0) {
                const auto right = this->get_board(x + 1, y);
                if(right) {
                    for(const auto by : integer_range(S)) {
                        board.set_block(
                          S - 1U, by, extract(right).get_block(0U, by));
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
                if(!this->get_board(x, y)) {
                    if(!tiling.solver.has_enqueued(Coord{x, y}, rank)) {
                        do_enqueue(tiling, x, y);
                    }
                }
            }
        }
    }

    void handle_solved(
      auto& tiling,
      const identifier_t helper_id,
      const sudoku_solver_key key,
      basic_sudoku_board<S> board) noexcept {

        const auto coord{std::get<Coord>(key)};
        if(this->set_board(coord, std::move(board))) {
            cells_done += this->cells_per_tile();
            tiling.solver.base.bus_node()
              .log_info("solved board (${x}, ${y})")
              .arg("rank", S)
              .arg("x", std::get<0>(coord))
              .arg("y", std::get<1>(coord))
              .arg("helper", helper_id)
              .arg(
                "progress",
                "Progress",
                0.F,
                float(cells_done),
                float(this->cell_count()));

            auto helper_pos = helper_contrib.find(helper_id);
            if(helper_pos == helper_contrib.end()) {
                helper_pos = helper_contrib.emplace(helper_id, 0).first;
            }
            ++helper_pos->second;

            const unsigned_constant<S> rank{};
            tiling.signals.tiles_generated_signal(rank)(
              helper_id, *this, coord);
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

    flat_map<identifier_t, span_size_t> helper_contrib;
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

    template <unsigned S>
    auto _is_already_done(
      const sudoku_solver_key& coord,
      const unsigned_constant<S> rank) const noexcept -> bool {
        return _infos.get(rank).get_board(std::get<Coord>(coord));
    }

    template <unsigned S>
    void handle_solved(
      const identifier_t helper_id,
      const sudoku_solver_key& coord,
      basic_sudoku_board<S>& board) noexcept {
        auto& info = _infos.get(unsigned_constant<S>{});
        info.handle_solved(
          *this, helper_id, std::get<Coord>(coord), std::move(board));
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

    auto solution_progress(unsigned rank) const noexcept -> float {
        float result{0.F};
        apply_to_sudoku_rank_unit(
          rank, [&](auto& info) { result = info.solution_progress(); }, _infos);
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
};
//------------------------------------------------------------------------------
auto make_sudoku_tiling_impl(
  sudoku_solver_intf& solver,
  sudoku_tiling_signals& tsigs) -> std::unique_ptr<sudoku_tiling_intf> {
    return std::make_unique<sudoku_tiling_impl>(
      static_cast<sudoku_solver_impl&>(solver), tsigs);
}
//------------------------------------------------------------------------------
} // namespace eagine::msgbus
