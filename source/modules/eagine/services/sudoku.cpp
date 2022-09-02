/// @file
///
/// Copyright Matus Chochlik.
/// Distributed under the Boost Software License, Version 1.0.
/// See accompanying file LICENSE_1_0.txt or copy at
///  http://www.boost.org/LICENSE_1_0.txt
///
module;

#include <cassert>

export module eagine.msgbus.services:sudoku;

import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.serialization;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.logging;
import eagine.core.math;
import eagine.msgbus.core;
import <algorithm>;
import <array>;
import <chrono>;
import <cmath>;
import <ostream>;
import <random>;
import <tuple>;
import <variant>;
import <vector>;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export template <template <unsigned> class U>
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
export template <typename Function, typename... RankTuple>
void for_each_sudoku_rank_unit(Function func, RankTuple&... t) {
    func(std::get<3>(t)...);
    func(std::get<4>(t)...);
    func(std::get<5>(t)...);
    func(std::get<6>(t)...);
}
//------------------------------------------------------------------------------
export template <unsigned S>
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
export template <unsigned S>
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
export template <unsigned S>
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
export template <unsigned S>
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
export template <unsigned S>
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
export template <unsigned S>
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
export template <unsigned S>
auto sudoku_response_msg(
  const unsigned_constant<S> rank,
  const bool is_solved) noexcept -> message_id {
    return is_solved ? sudoku_solved_msg(rank) : sudoku_candidate_msg(rank);
}
//------------------------------------------------------------------------------
export struct sudoku_helper_intf : interface<sudoku_helper_intf> {
    virtual void add_methods(subscriber&) noexcept = 0;
    virtual void init(subscriber&) noexcept = 0;
    virtual auto update(endpoint& bus) noexcept -> work_done = 0;

    virtual void mark_activity() noexcept = 0;

    /// @brief Returns current idle time interval.
    virtual auto idle_time() const noexcept
      -> std::chrono::steady_clock::duration = 0;
};
//------------------------------------------------------------------------------
export auto make_sudoku_helper_impl() -> std::unique_ptr<sudoku_helper_intf>;
//------------------------------------------------------------------------------
/// @brief Service helping to partially solve sudoku boards sent by sudoku_solver.
/// @ingroup msgbus
/// @see service_composition
/// @see sudoku_solver
/// @see sudoku_tiling
export template <typename Base = subscriber>
class sudoku_helper : public Base {
    using This = sudoku_helper;

public:
    auto update() noexcept -> work_done {
        some_true something_done{Base::update()};
        something_done(_impl->update(this->bus_node()));

        return something_done;
    }

    /// @brief Returns current idle time interval.
    auto idle_time() const noexcept {
        return _impl->idle_time();
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();

        _impl->add_methods(*this);
    }

    void init() noexcept {
        Base::init();
        _impl->init(*this);
    }

private:
    std::unique_ptr<sudoku_helper_intf> _impl{make_sudoku_helper_impl()};
};
//------------------------------------------------------------------------------
export using sudoku_solver_key =
  std::variant<std::monostate, int, std::tuple<int, int>>;
//------------------------------------------------------------------------------
struct sudoku_solver_intf : interface<sudoku_solver_intf> {

    virtual void add_methods() noexcept = 0;
    virtual void init() noexcept = 0;
    virtual auto update() noexcept -> work_done = 0;

    virtual void enqueue(sudoku_solver_key, basic_sudoku_board<3>) noexcept = 0;
    virtual void enqueue(sudoku_solver_key, basic_sudoku_board<4>) noexcept = 0;
    virtual void enqueue(sudoku_solver_key, basic_sudoku_board<5>) noexcept = 0;
    virtual void enqueue(sudoku_solver_key, basic_sudoku_board<6>) noexcept = 0;

    virtual auto has_work() const noexcept -> bool = 0;
    virtual void reset(unsigned rank) noexcept = 0;
    virtual auto has_enqueued(
      const sudoku_solver_key& key,
      unsigned rank) noexcept -> bool = 0;

    virtual void set_solution_timeout(
      unsigned rank,
      const std::chrono::seconds sec) noexcept = 0;

    virtual auto solution_timeouted(unsigned rank) const noexcept -> bool = 0;

    virtual auto updated_by_helper(
      const identifier_t helper_id,
      const unsigned rank) const noexcept -> std::intmax_t = 0;

    virtual auto updated_count(const unsigned rank) const noexcept
      -> std::intmax_t = 0;

    virtual auto solved_by_helper(
      const identifier_t helper_id,
      const unsigned rank) const noexcept -> std::intmax_t = 0;

    virtual auto solved_count(const unsigned rank) const noexcept
      -> std::intmax_t = 0;
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the sudoku_solver service
/// @ingroup msgbus
/// @see sudoku_solver
export struct sudoku_solver_signals {

    /// @brief Triggered when a helper service appears.
    signal<void(const identifier_t) noexcept> helper_appeared;

    /// @brief Triggered when the board with the specified key is solved.
    signal<void(
      const identifier_t,
      const sudoku_solver_key&,
      basic_sudoku_board<3>&) noexcept>
      solved_3;
    /// @brief Triggered when the board with the specified key is solved.
    signal<void(
      const identifier_t,
      const sudoku_solver_key&,
      basic_sudoku_board<4>&) noexcept>
      solved_4;
    /// @brief Triggered when the board with the specified key is solved.
    signal<void(
      const identifier_t,
      const sudoku_solver_key&,
      basic_sudoku_board<5>&) noexcept>
      solved_5;
    /// @brief Triggered when the board with the specified key is solved.
    signal<void(
      const identifier_t,
      const sudoku_solver_key&,
      basic_sudoku_board<6>&) noexcept>
      solved_6;

    /// @brief Returns a reference to the solved_3 signal.
    /// @see solved_3
    auto solved_signal(const unsigned_constant<3> = {}) noexcept -> auto& {
        return solved_3;
    }

    /// @brief Returns a reference to the solved_4 signal.
    /// @see solved_4
    auto solved_signal(const unsigned_constant<4> = {}) noexcept -> auto& {
        return solved_4;
    }

    /// @brief Returns a reference to the solved_5 signal.
    /// @see solved_5
    auto solved_signal(const unsigned_constant<5> = {}) noexcept -> auto& {
        return solved_5;
    }

    /// @brief Returns a reference to the solved_6 signal.
    /// @see solved_6
    auto solved_signal(const unsigned_constant<6> = {}) noexcept -> auto& {
        return solved_6;
    }

    /// @brief Triggered when the length of the queue of boards change.
    signal<void(const unsigned, const std::size_t, const std::size_t) noexcept>
      queue_length_changed;
};
//------------------------------------------------------------------------------
export struct sudoku_solver_driver : interface<sudoku_solver_driver> {

    /// @brief Indicates if board with the specified rank is already solved.
    virtual auto already_done(
      const sudoku_solver_key&,
      const unsigned_constant<3>) noexcept -> bool {
        return false;
    }
    /// @brief Indicates if board with the specified rank is already solved.
    virtual auto already_done(
      const sudoku_solver_key&,
      const unsigned_constant<4>) noexcept -> bool {
        return false;
    }
    /// @brief Indicates if board with the specified rank is already solved.
    virtual auto already_done(
      const sudoku_solver_key&,
      const unsigned_constant<5>) noexcept -> bool {
        return false;
    }
    /// @brief Indicates if board with the specified rank is already solved.
    virtual auto already_done(
      const sudoku_solver_key&,
      const unsigned_constant<6>) noexcept -> bool {
        return false;
    }
};
//------------------------------------------------------------------------------
export auto make_sudoku_solver_impl(
  subscriber& base,
  sudoku_solver_driver&,
  sudoku_solver_signals&) -> std::unique_ptr<sudoku_solver_intf>;
//------------------------------------------------------------------------------
/// @brief Service solving sudoku boards with the help of helper service on message bus.
/// @ingroup msgbus
/// @see service_composition
/// @see sudoku_helper
/// @see sudoku_tiling
export template <typename Base = subscriber, typename Key = int>
class sudoku_solver
  : public Base
  , public sudoku_solver_driver
  , public sudoku_solver_signals {
    using This = sudoku_solver;

public:
    /// @brief Enqueues a Sudoku board for solution under the specified unique key.
    /// @see has_enqueued
    /// @see has_work
    /// @see is_done
    template <unsigned S>
    auto enqueue(Key key, basic_sudoku_board<S> board) noexcept -> auto& {
        _impl->enqueue(std::move(key), std::move(board));
        return *this;
    }

    /// @brief Indicates if there are pending boards being solved.
    /// @see enqueue
    /// @see is_done
    auto has_work() const noexcept -> bool {
        return _impl->has_work();
    }

    /// @brief Indicates if there is not work being done. Opposite of has_work.
    /// @see enqueue
    /// @see has_work.
    auto is_done() const noexcept -> bool {
        return !has_work();
    }

    void init() noexcept {
        Base::init();
        _impl->init();
    }

    auto update() noexcept -> work_done {
        some_true something_done{Base::update()};
        something_done(_impl->update());

        return something_done;
    }

    /// @brief Resets all boards with the given rank.
    template <unsigned S>
    auto reset(const unsigned_constant<S>) noexcept -> auto& {
        _impl->reset(S);
        return *this;
    }

    /// @brief Indicates if a board with the given rank and key is enqueued.
    /// @see enqueue
    template <unsigned S>
    auto has_enqueued(const Key& key, const unsigned_constant<S>) noexcept
      -> bool {
        return _impl->has_enqueued(key, S);
    }

    /// @brief Sets the solution timeout for the specified rank.
    template <unsigned S>
    void set_solution_timeout(
      const unsigned_constant<S>,
      const std::chrono::seconds sec) noexcept {
        _impl->set_solution_timeout(S, sec);
    }

    /// @brief Indicates if the solution of board with the specified rank timeouted.
    template <unsigned S>
    auto solution_timeouted(const unsigned_constant<S>) const noexcept -> bool {
        return _impl->solution_timeouted(S);
    }

    /// @brief Returns the number of boards updated by the specified helper.
    /// @see solved_by_helper
    /// @see updated_count
    template <unsigned S>
    auto updated_by_helper(
      const identifier_t helper_id,
      const unsigned_constant<S>) const noexcept -> std::intmax_t {
        return _impl->updated_by_helper(helper_id, S);
    }

    /// @brief Returns the number of boards updated by the specified helper.
    /// @see solved_count
    /// @see updated_by_helper
    template <unsigned S>
    auto updated_count(const unsigned_constant<S>) const noexcept
      -> std::intmax_t {
        return _impl->updated_count(S);
    }

    /// @brief Returns the number of boards solved by the specified helper.
    /// @see updated_by_helper
    /// @see solved_count
    template <unsigned S>
    auto solved_by_helper(
      const identifier_t helper_id,
      const unsigned_constant<S>) const noexcept -> std::intmax_t {
        return _impl->solved_by_helper(helper_id, S);
    }

    /// @brief Returns the number of boards updated by the specified helper.
    /// @see updated_count
    /// @see solved_by_helper
    template <unsigned S>
    auto solved_count(const unsigned_constant<S>) const noexcept
      -> std::intmax_t {
        return _impl->solved_count(S);
    }

protected:
    using Base::Base;

    void add_methods() noexcept {
        Base::add_methods();
        _impl->add_methods();
    }

private:
    std::unique_ptr<sudoku_solver_intf> _impl{
      make_sudoku_solver_impl(*this, *this, *this)};
};
//------------------------------------------------------------------------------
export template <unsigned S>
class sudoku_tiles;
//------------------------------------------------------------------------------
/// @brief Class providing view to a solved fragment in sudoku_tiles.
/// @ingroup msgbus
/// @see sudoku_tiles
export template <unsigned S>
class sudoku_fragment_view {
public:
    /// @brief The board/cell coordinate type.
    using Coord = std::tuple<int, int>;

    /// @brief Returns the width (in cells) of the tile.
    constexpr auto width() const noexcept -> int {
        return limit_cast<int>(S * (S - 2));
    }

    /// @brief Returns the height (in cells) of the tile.
    constexpr auto height() const noexcept -> int {
        return limit_cast<int>(S * (S - 2));
    }

    /// @brief Calls the specified function for each cell in the fragment.
    /// The function should have the following arguments:
    /// - std::tuple<int, int> the fragment coordinate (in cell units),
    /// - std::tuple<int, int> the cell offset within the fragment,
    /// - basic_sudoku_glyph<S> the glyph at the cell.
    template <typename Function>
    void for_each_cell(Function function) const;

private:
    friend class sudoku_tiles<S>;

    sudoku_fragment_view(
      const sudoku_tiles<S>& tiles,
      Coord board_coord) noexcept
      : _tiles{tiles}
      , _board_coord{std::move(board_coord)} {}

    const sudoku_tiles<S>& _tiles;
    Coord _board_coord;
};
//------------------------------------------------------------------------------
/// @brie Class representing a set of related Sudoku tiles.
/// @ingroup msgbus
/// @see sudoku_tiling
export template <unsigned S>
class sudoku_tiles {
public:
    /// @brief The board coordinate/key type.
    using Coord = std::tuple<int, int>;

    /// @brief Returns the width (in cells) of the tiling.
    /// @see height
    /// @see cell_count
    auto width() const noexcept -> int {
        return _maxu - _minu;
    }

    /// @brief Returns the height (in cells) of the tiling.
    /// @see width
    /// @see cell_count
    auto height() const noexcept -> int {
        return _maxv - _minv;
    }

    /// @brief Total count of tiles in this tiling.
    /// @see width
    /// @see height
    /// @see cells_per_tile
    auto cell_count() const noexcept -> int {
        return width() * height();
    }

    /// @brief Returns how many cells are on the side of a single tile.
    /// @see cells_per_tile
    constexpr auto cells_per_tile_side() const noexcept -> int {
        return S * (S - 2);
    }

    /// @brief Returns how many cells are in a single tile.
    /// @see cells_per_tile_side
    constexpr auto cells_per_tile() const noexcept -> int {
        return cells_per_tile_side() * cells_per_tile_side();
    }

    /// @brief Get the board at the specified coordinate if it is solved.
    auto get_board(const Coord coord) const noexcept
      -> const basic_sudoku_board<S>* {
        const auto pos = _boards.find(coord);
        if(pos != _boards.end()) {
            return &pos->second;
        }
        return nullptr;
    }

    /// @brief Get the board at the specified coordinate if it is solved.
    auto get_board(const int x, const int y) const noexcept {
        return get_board({x, y});
    }

    /// @brief Sets the board at the specified coordinate.
    auto set_board(Coord coord, basic_sudoku_board<S> board) noexcept -> bool {
        return _boards.try_emplace(std::move(coord), std::move(board)).second;
    }

    /// @brief Return a view of the fragment at the specified board coordinate.
    auto get_fragment(Coord coord) const noexcept -> sudoku_fragment_view<S> {
        return {*this, coord};
    }

    /// @brief Sets the extent (number of tiles in x and y dimension) of the tiling.
    void set_extent(const Coord min, const Coord max) noexcept {
        _minu = std::get<0>(min);
        _minv = std::get<1>(min);
        _maxu = std::get<0>(max);
        _maxv = std::get<1>(max);
    }

    /// @brief Sets the extent (number of tiles in x and y dimension) of the tiling.
    /// @see is_in_extent
    void set_extent(const Coord max) noexcept {
        set_extent({0, 0}, max);
    }

    /// @brief Indicates in the specified coordinate is in the extent of this tiling.
    /// @see set_extent
    auto is_in_extent(const int x, const int y) const noexcept -> bool {
        const int u = x * S * (S - 2);
        const int v = y * S * (S - 2);
        return (u >= _minu) && (u < _maxu) && (v >= _minv) && (v < _maxv);
    }

    /// @brief Returns the extent between min and max in units of boards.
    auto boards_extent(const Coord min, const Coord max) const noexcept
      -> std::tuple<int, int, int, int> {
        const auto conv = [](int c) {
            const auto mult = S * (S - 2);
            if(c < 0) {
                return c / mult - ((-c % mult) ? 1 : 0);
            }
            return c / mult + (c % mult ? 1 : 0);
        };
        return {
          conv(std::get<0>(min)),
          conv(std::get<1>(min)),
          conv(std::get<0>(max)),
          conv(std::get<1>(max))};
    }

    /// @brief Returns the extent of this tiling in units of boards.
    auto boards_extent() const noexcept {
        return boards_extent({_minu, _minv}, {_maxu, _maxv});
    }

    /// @brief Indicates if the boards between the min and max coordinates are solved.
    auto are_complete(const Coord min, const Coord max) const noexcept -> bool {
        const auto [xmin, ymin, xmax, ymax] = boards_extent(min, max);
        for(const auto y : integer_range(ymin, ymax)) {
            for(const auto x : integer_range(xmin, xmax)) {
                if(!get_board(x, y)) {
                    return false;
                }
            }
        }
        return true;
    }

    /// @brief Indicates if the boards in this tiling's extent are solved.
    auto are_complete() const noexcept -> bool {
        return are_complete({_minu, _minv}, {_maxu, _maxv});
    }

    /// @brief Prints the current tiling using the specified sudoku board traits.
    auto print(
      std::ostream& out,
      const Coord min,
      const Coord max,
      const basic_sudoku_board_traits<S>& traits) const noexcept
      -> std::ostream& {
        const auto [xmin, ymin, xmax, ymax] = boards_extent(min, max);

        for(const auto y : integer_range(ymin, ymax)) {
            for(const auto by : integer_range(1U, S - 1U)) {
                for(const auto cy : integer_range(S)) {
                    for(const auto x : integer_range(xmin, xmax)) {
                        auto board = get_board(x, y);
                        for(const auto bx : integer_range(1U, S - 1U)) {
                            for(const auto cx : integer_range(S)) {
                                if(board) {
                                    traits.print(
                                      out,
                                      extract(board).get({bx, by, cx, cy}));
                                } else {
                                    traits.print_empty(out);
                                }
                            }
                        }
                    }
                    out << '\n';
                }
            }
        }
        return out;
    }

    /// @brief Shows which tiles are solved and which unsolved.
    auto print_progress(std::ostream& out, const Coord min, const Coord max)
      const noexcept -> std::ostream& {
        const auto [xmin, ymin, xmax, ymax] = boards_extent(min, max);

        for(const auto y : integer_range(ymin, ymax)) {
            for(const auto x : integer_range(xmin, xmax)) {
                if(get_board(x, y)) {
                    out << "██";
                } else {
                    out << "▒▒";
                }
            }
            out << '\n';
        }
        return out;
    }

    /// @brief Prints the current tiling using the specified sudoku board traits.
    auto print(std::ostream& out, const Coord min, const Coord max)
      const noexcept -> std::ostream& {
        return print(out, min, max, _traits);
    }

    /// @brief Prints the current tiling using the specified sudoku board traits.
    auto print(std::ostream& out, const basic_sudoku_board_traits<S>& traits)
      const noexcept -> auto& {
        return print(out, {_minu, _minv}, {_maxu, _maxv}, traits);
    }

    /// @brief Prints the current tiling using the specified sudoku board traits.
    auto print(std::ostream& out) const noexcept -> auto& {
        return print(out, {_minu, _minv}, {_maxu, _maxv});
    }

    /// @brief Shows which tiles are solved and which unsolved.
    auto print_progress(std::ostream& out) const noexcept -> auto& {
        return print_progress(out, {_minu, _minv}, {_maxu, _maxv});
    }

    /// @brief Resets all pending tilings.
    auto reset() noexcept -> auto& {
        _boards.clear();
        return *this;
    }

protected:
    auto new_board() noexcept -> basic_sudoku_board<S> {
        return {_traits};
    }

private:
    int _minu{0};
    int _minv{0};
    int _maxu{0};
    int _maxv{0};
    flat_map<Coord, basic_sudoku_board<S>> _boards;
    default_sudoku_board_traits<S> _traits;
};
//------------------------------------------------------------------------------
template <unsigned S>
template <typename Function>
void sudoku_fragment_view<S>::for_each_cell(Function function) const {
    if(const auto board{_tiles.get_board(_board_coord)}) {
        const auto [bx, by] = _board_coord;
        const Coord frag_coord{
          limit_cast<int>(bx * width()), limit_cast<int>(by * height())};
        for(const auto y : integer_range(height())) {
            for(const auto x : integer_range(width())) {
                const Coord cell_offset{x, y};
                std::array<unsigned, 4> cell_coord{
                  limit_cast<unsigned>(1 + x / S),
                  limit_cast<unsigned>(1 + y / S),
                  limit_cast<unsigned>(x % S),
                  limit_cast<unsigned>(y % S)};
                function(
                  frag_coord, cell_offset, extract(board).get(cell_coord));
            }
        }
    }
}
//------------------------------------------------------------------------------
/// @brief Service generating a sudoku tiling using helper message bus nodes.
/// @ingroup msgbus
/// @see service_composition
/// @see sudoku_helper
/// @see sudoku_solver
export template <typename Base = subscriber>
class sudoku_tiling : public sudoku_solver<Base, std::tuple<int, int>> {
    using base = sudoku_solver<Base, std::tuple<int, int>>;
    using This = sudoku_tiling;
    using Coord = std::tuple<int, int>;

public:
    /// @brief Initializes the tiling to be generated with initial board.
    template <unsigned S>
    auto initialize(
      const Coord min,
      const Coord max,
      const Coord coord,
      basic_sudoku_board<S> board) noexcept -> auto& {
        const auto [x, y] = coord;
        auto& info = _infos.get(unsigned_constant<S>{});
        info.set_extent(min, max);
        info.initialize(*this, x, y, std::move(board));
        return *this;
    }

    /// @brief Initializes the tiling to be generated with initial board.
    template <unsigned S>
    auto initialize(const Coord max, basic_sudoku_board<S> board) noexcept
      -> auto& {
        return initialize({0, 0}, max, {0, 0}, std::move(board));
    }

    /// @brief Resets the tiling with the specified rank.
    template <unsigned S>
    auto reset(unsigned_constant<S> rank) noexcept -> auto& {
        base::reset(rank);
        _infos.get(rank).reset();
        return *this;
    }

    /// @brief Re-initializes the tiling with the specified board.
    template <unsigned S>
    auto reinitialize(const Coord max, basic_sudoku_board<S> board) noexcept
      -> auto& {
        reset(unsigned_constant<S>{});
        return initialize(max, board);
    }

    /// @brief Indicates that pending tiling with the specified rank is complete.
    template <unsigned S>
    auto tiling_complete(const unsigned_constant<S> rank) const noexcept
      -> bool {
        return _infos.get(rank).are_complete();
    }

    /// @brief Indicates that all pending tilings are complete.
    auto tiling_complete() const noexcept -> bool {
        bool result = true;
        sudoku_rank_tuple<unsigned_constant> ranks;
        for_each_sudoku_rank_unit(
          [&](auto rank) { result &= tiling_complete(rank); }, ranks);
        return result;
    }

    /// @brief Triggered then all tiles with rank 3 are generated.
    signal<void(
      const identifier_t,
      const sudoku_tiles<3>&,
      const sudoku_solver_key&) noexcept>
      tiles_generated_3;
    /// @brief Triggered then all tiles with rank 4 are generated.
    signal<void(
      const identifier_t,
      const sudoku_tiles<4>&,
      const sudoku_solver_key&) noexcept>
      tiles_generated_4;
    /// @brief Triggered then all tiles with rank 5 are generated.
    signal<void(
      const identifier_t,
      const sudoku_tiles<5>&,
      const sudoku_solver_key&) noexcept>
      tiles_generated_5;
    /// @brief Triggered then all tiles with rank 6 are generated.
    signal<void(
      const identifier_t,
      const sudoku_tiles<6>&,
      const sudoku_solver_key&) noexcept>
      tiles_generated_6;

    /// @brief Returns a reference to the tiles_generated_3 signal.
    /// @see tiles_generated_3
    auto tiles_generated_signal(const unsigned_constant<3>) noexcept -> auto& {
        return tiles_generated_3;
    }

    /// @brief Returns a reference to the tiles_generated_4 signal.
    /// @see tiles_generated_4
    auto tiles_generated_signal(const unsigned_constant<4>) noexcept -> auto& {
        return tiles_generated_4;
    }

    /// @brief Returns a reference to the tiles_generated_5 signal.
    /// @see tiles_generated_5
    auto tiles_generated_signal(const unsigned_constant<5>) noexcept -> auto& {
        return tiles_generated_5;
    }

    /// @brief Returns a reference to the tiles_generated_6 signal.
    /// @see tiles_generated_6
    auto tiles_generated_signal(const unsigned_constant<6>) noexcept -> auto& {
        return tiles_generated_6;
    }

    /// @brief Logs the contributions of the helpers to the solution.
    template <unsigned S>
    auto log_contribution_histogram(const unsigned_constant<S> rank) noexcept
      -> auto& {
        _infos.get(rank).log_contribution_histogram(*this);
        return *this;
    }

    /// @brief Returns the fraction <0, 1> indicating how many tiles are solved.
    template <unsigned S>
    auto solution_progress(const unsigned_constant<S> rank) const noexcept
      -> float {
        return _infos.get(rank).solution_progress();
    }

protected:
    sudoku_tiling(endpoint& bus) noexcept
      : base{bus} {
        connect<&sudoku_tiling::_handle_solved<3>>(this, this->solved_3);
        connect<&sudoku_tiling::_handle_solved<4>>(this, this->solved_4);
        connect<&sudoku_tiling::_handle_solved<5>>(this, this->solved_5);
        connect<&sudoku_tiling::_handle_solved<6>>(this, this->solved_6);
    }

private:
    template <unsigned S>
    struct rank_info : sudoku_tiles<S> {

        void initialize(
          This& solver,
          const int x,
          const int y,
          basic_sudoku_board<S> board) noexcept {
            solver.enqueue({x, y}, std::move(board));
            solver.bus_node()
              .log_debug("enqueuing initial board (${x}, ${y})")
              .arg("x", x)
              .arg("y", y)
              .arg("rank", S);
            cells_done = 0;
        }

        void do_enqueue(This& solver, const int x, const int y) noexcept {
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
                solver.enqueue({x, y}, board.calculate_alternatives());
                solver.bus_node()
                  .log_debug("enqueuing board (${x}, ${y})")
                  .arg("x", x)
                  .arg("y", y)
                  .arg("rank", S);
            }
        }

        void enqueue_incomplete(This& solver) noexcept {
            const unsigned_constant<S> rank{};
            const auto [xmin, ymin, xmax, ymax] = this->boards_extent();
            for(const auto y : integer_range(ymin, ymax)) {
                for(const auto x : integer_range(xmin, xmax)) {
                    if(!this->get_board(x, y)) {
                        if(!solver.has_enqueued({x, y}, rank)) {
                            do_enqueue(solver, x, y);
                        }
                    }
                }
            }
        }

        void handle_solved(
          This& solver,
          const identifier_t helper_id,
          const Coord coord,
          basic_sudoku_board<S> board) noexcept {

            if(this->set_board(coord, std::move(board))) {
                cells_done += this->cells_per_tile();
                solver.bus_node()
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
                solver.tiles_generated_signal(rank)(helper_id, *this, coord);
            }

            enqueue_incomplete(solver);
        }

        void log_contribution_histogram(This& solver) noexcept {
            span_size_t max_count = 0;
            for(const auto& p : helper_contrib) {
                max_count = std::max(max_count, std::get<1>(p));
            }
            solver.bus_node()
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

    sudoku_rank_tuple<rank_info> _infos;

    auto already_done(
      const sudoku_solver_key& coord,
      const unsigned_constant<3> rank) noexcept -> bool final {
        return _is_already_done(std::get<Coord>(coord), rank);
    }
    auto already_done(
      const sudoku_solver_key& coord,
      const unsigned_constant<4> rank) noexcept -> bool final {
        return _is_already_done(std::get<Coord>(coord), rank);
    }
    auto already_done(
      const sudoku_solver_key& coord,
      const unsigned_constant<5> rank) noexcept -> bool final {
        return _is_already_done(std::get<Coord>(coord), rank);
    }
    auto already_done(
      const sudoku_solver_key& coord,
      const unsigned_constant<6> rank) noexcept -> bool final {
        return _is_already_done(std::get<Coord>(coord), rank);
    }

    template <unsigned S>
    auto _is_already_done(const Coord& coord, const unsigned_constant<S> rank)
      const noexcept -> bool {
        return _infos.get(rank).get_board(coord);
    }

    template <unsigned S>
    void _handle_solved(
      const identifier_t helper_id,
      const sudoku_solver_key& coord,
      basic_sudoku_board<S>& board) noexcept {
        auto& info = _infos.get(unsigned_constant<S>{});
        info.handle_solved(
          *this, helper_id, std::get<Coord>(coord), std::move(board));
    }
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

