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

import std;
import eagine.core.types;
import eagine.core.memory;
import eagine.core.identifier;
import eagine.core.container;
import eagine.core.serialization;
import eagine.core.utility;
import eagine.core.runtime;
import eagine.core.math;
import eagine.msgbus.core;

namespace eagine::msgbus {
//------------------------------------------------------------------------------
export struct sudoku_helper_intf : interface<sudoku_helper_intf> {
    virtual void add_methods() noexcept = 0;
    virtual void init() noexcept = 0;
    virtual auto update() noexcept -> work_done = 0;

    virtual void mark_activity() noexcept = 0;

    /// @brief Returns current idle time interval.
    virtual auto idle_time() const noexcept
      -> std::chrono::steady_clock::duration = 0;
};
//------------------------------------------------------------------------------
export auto make_sudoku_helper_impl(subscriber&)
  -> std::unique_ptr<sudoku_helper_intf>;
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
        something_done(_impl->update());

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
        _impl->add_methods();
    }

    void init() noexcept {
        Base::init();
        _impl->init();
    }

private:
    const std::unique_ptr<sudoku_helper_intf> _impl{
      make_sudoku_helper_impl(*this)};
};
//------------------------------------------------------------------------------
export using sudoku_solver_key = std::variant<int, std::tuple<int, int>>;
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
/// @brief Type storing information about (partially) solved Sudoku board.
/// @ingroup msgbus
/// @see sudoku_solver_signals
export template <unsigned S>
struct solved_sudoku_board {
    /// @brief Id of the helper that provided the solution.
    identifier_t helper_id{invalid_endpoint_id()};
    /// @brief Key, identifiying the board.
    sudoku_solver_key key{0};
    /// @brief Elapsed time.
    std::chrono::steady_clock::duration elapsed_time{};
    /// @brief The Sudoku board.
    basic_sudoku_board<S> board;
};
//------------------------------------------------------------------------------
struct sudoku_solver_intf : interface<sudoku_solver_intf> {

    virtual void assign_driver(sudoku_solver_driver&) noexcept = 0;
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

    virtual void reset_solution_timeout(unsigned rank) noexcept = 0;

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
/// @brief Type containing information about a Sudoku solver helper service.
/// @ingroup msgbus
/// @see sudoku_solver_signals
export struct sudoku_helper_appeared {
    /// @brief Id of the helper endpoint.
    identifier_t helper_id{invalid_endpoint_id()};
};

/// @brief Type containing information about Sudoku solver queue changes.
/// @see sudoku_solver_signals
/// @ingroup msgbus
/// @see sudoku_solver_signals
export struct sudoku_board_queue_change {
    /// @brief The rank of the boards in queue.
    unsigned rank{0};
    /// @brief Number of distinct keys in the queue.
    std::size_t key_count{0};
    /// @brief Number of boards in the queue.
    std::size_t board_count{0};
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the sudoku_solver service
/// @ingroup msgbus
/// @see sudoku_solver
export struct sudoku_solver_signals {

    /// @brief Triggered when a helper service appears.
    signal<void(const result_context&, const sudoku_helper_appeared&) noexcept>
      helper_appeared;

    /// @brief Triggered when the board with the specified key is solved.
    signal<void(const result_context&, const solved_sudoku_board<3>&) noexcept>
      solved_3;
    /// @brief Triggered when the board with the specified key is solved.
    signal<void(const result_context&, const solved_sudoku_board<4>&) noexcept>
      solved_4;
    /// @brief Triggered when the board with the specified key is solved.
    signal<void(const result_context&, const solved_sudoku_board<5>&) noexcept>
      solved_5;
    /// @brief Triggered when the board with the specified key is solved.
    signal<void(const result_context&, const solved_sudoku_board<6>&) noexcept>
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
    signal<void(const sudoku_board_queue_change&) noexcept> queue_length_changed;
};
//------------------------------------------------------------------------------
export auto make_sudoku_solver_impl(subscriber& base, sudoku_solver_signals&)
  -> std::unique_ptr<sudoku_solver_intf>;
//------------------------------------------------------------------------------
/// @brief Service solving sudoku boards with the help of helper service on message bus.
/// @ingroup msgbus
/// @see service_composition
/// @see sudoku_helper
/// @see sudoku_tiling
export template <typename Base = subscriber, typename Key = int>
class sudoku_solver
  : public Base
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
        return not has_work();
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

    /// @brief Resets the solution timeout for the specified rank.
    template <unsigned S>
    void reset_solution_timeout(const unsigned_constant<S>) noexcept {
        _impl->reset_solution_timeout(S);
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
    sudoku_solver(
      endpoint& bus,
      std::unique_ptr<sudoku_solver_intf> impl) noexcept
      : Base{bus}
      , _impl{std::move(impl)} {}

    sudoku_solver(endpoint& bus)
      : sudoku_solver{bus, make_sudoku_solver_impl(*this, *this)} {}

    void add_methods() noexcept {
        Base::add_methods();
        _impl->add_methods();
    }

    auto impl() noexcept -> sudoku_solver_intf& {
        return *_impl;
    }

private:
    const std::unique_ptr<sudoku_solver_intf> _impl;
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
        return limit_cast<int>(S * (S - 1));
    }

    /// @brief Returns the height (in cells) of the tile.
    constexpr auto height() const noexcept -> int {
        return limit_cast<int>(S * (S - 1));
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
        return S * (S - 1);
    }

    /// @brief Returns how many cells are in a single tile.
    /// @see cells_per_tile_side
    constexpr auto cells_per_tile() const noexcept -> int {
        return cells_per_tile_side() * cells_per_tile_side();
    }

    /// @brief Returns how many cells are in the specified (possibly clipped) tile.
    /// @see cells_per_tile
    auto cells_per_tile(Coord coord) const noexcept -> int {
        const auto [x, y] = coord;
        const int minu = std::max((x + 0) * cells_per_tile_side(), _minu);
        const int maxu = std::min((x + 1) * cells_per_tile_side(), _maxu);
        const int minv = std::max((y + 0) * cells_per_tile_side(), _minv);
        const int maxv = std::min((y + 1) * cells_per_tile_side(), _maxv);

        return (maxu - minu) * (maxv - minv);
    }

    /// @brief Number of tiles on the x-axis.
    /// @see y_tiles_count
    auto x_tiles_count() const noexcept -> int {
        return (width() / cells_per_tile_side()) +
               ((width() % cells_per_tile_side()) ? 1 : 0);
    }

    /// @brief Number of tiles on the x-axis.
    /// @see x_tiles_count
    auto y_tiles_count() const noexcept -> int {
        return (height() / cells_per_tile_side()) +
               ((height() % cells_per_tile_side()) ? 1 : 0);
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
        const int u = x * cells_per_tile_side();
        const int v = y * cells_per_tile_side();
        return (u >= _minu) and (u < _maxu) and (v >= _minv) and (v < _maxv);
    }

    /// @brief Returns the extent between min and max in units of boards.
    auto boards_extent(const Coord min, const Coord max) const noexcept
      -> std::tuple<int, int, int, int> {
        const auto conv{[this](int c) {
            const auto mult = cells_per_tile_side();
            if(c < 0) {
                return (c / mult) - ((-c % mult) ? 1 : 0);
            }
            return (c / mult) + (c % mult ? 1 : 0);
        }};
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
                if(not get_board(x, y)) {
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

        const int w{width()}, h{height()};
        int c{0}, r{0};
        for(const auto y : integer_range(ymin, ymax)) {
            for(const auto by : integer_range(1U, S)) {
                for(const auto cy : integer_range(S)) {
                    for(const auto x : integer_range(xmin, xmax)) {
                        auto board = get_board(x, y);
                        for(const auto bx : integer_range(1U, S)) {
                            for(const auto cx : integer_range(S)) {
                                if(c++ < w) {
                                    if(board) {
                                        traits.print(
                                          out, board->get({bx, by, cx, cy}));
                                    } else {
                                        traits.print_empty(out);
                                    }
                                }
                            }
                        }
                    }
                    out << '\n';
                    if(++r >= h) {
                        return out;
                    }
                    c = 0;
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
                function(frag_coord, cell_offset, board->get(cell_coord));
            }
        }
    }
}
//------------------------------------------------------------------------------
struct sudoku_tiling_intf : interface<sudoku_tiling_intf> {
public:
    virtual auto driver() noexcept -> sudoku_solver_driver& = 0;

    virtual void initialize(
      const sudoku_solver_key min,
      const sudoku_solver_key max,
      const sudoku_solver_key coord,
      basic_sudoku_board<3> board) noexcept = 0;

    virtual void initialize(
      const sudoku_solver_key min,
      const sudoku_solver_key max,
      const sudoku_solver_key coord,
      basic_sudoku_board<4> board) noexcept = 0;

    virtual void initialize(
      const sudoku_solver_key min,
      const sudoku_solver_key max,
      const sudoku_solver_key coord,
      basic_sudoku_board<5> board) noexcept = 0;

    virtual void initialize(
      const sudoku_solver_key min,
      const sudoku_solver_key max,
      const sudoku_solver_key coord,
      basic_sudoku_board<6> board) noexcept = 0;

    virtual void reset(unsigned rank) noexcept = 0;
    virtual auto are_complete() const noexcept -> bool = 0;
    virtual auto are_complete(unsigned rank) const noexcept -> bool = 0;
    virtual auto tiling_size(unsigned rank) const noexcept
      -> std::tuple<int, int> = 0;
    virtual auto solution_progress(unsigned rank) const noexcept -> float = 0;
    virtual void log_contribution_histogram(unsigned rank) noexcept = 0;
};
//------------------------------------------------------------------------------
/// @brief Collection of signals emitted by the sudoku_tiling service
/// @ingroup msgbus
/// @see sudoku_tiling
export struct sudoku_tiling_signals {
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
};
//------------------------------------------------------------------------------
export auto make_sudoku_tiling_impl(sudoku_solver_intf&, sudoku_tiling_signals&)
  -> std::unique_ptr<sudoku_tiling_intf>;
//------------------------------------------------------------------------------
/// @brief Service generating a sudoku tiling using helper message bus nodes.
/// @ingroup msgbus
/// @see service_composition
/// @see sudoku_helper
/// @see sudoku_solver
export template <typename Base = subscriber>
class sudoku_tiling
  : public sudoku_solver<Base, std::tuple<int, int>>
  , public sudoku_tiling_signals {
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
        _impl->initialize(min, max, coord, std::move(board));
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
        _impl->reset(S);
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
    auto tiling_complete(const unsigned_constant<S>) const noexcept -> bool {
        return _impl->are_complete(S);
    }

    /// @brief Indicates that pending tilings with all ranks are complete.
    auto tiling_complete() const noexcept -> bool {
        return _impl->are_complete();
    }

    /// @brief Returns the number of tiles on the x and y axes.
    template <unsigned S>
    auto tiling_size(const unsigned_constant<S>) const noexcept
      -> std::tuple<int, int> {
        return _impl->tiling_size(S);
    }

    /// @brief Returns the fraction <0, 1> indicating how many tiles are solved.
    template <unsigned S>
    auto solution_progress(const unsigned_constant<S>) const noexcept -> float {
        return _impl->solution_progress(S);
    }

    /// @brief Logs the contributions of the helpers to the solution.
    template <unsigned S>
    auto log_contribution_histogram(const unsigned_constant<S>) noexcept
      -> auto& {
        _impl->log_contribution_histogram(S);
        return *this;
    }

protected:
    sudoku_tiling(endpoint& bus) noexcept
      : base{bus}
      , _impl{make_sudoku_tiling_impl(base::impl(), *this)} {
        base::impl().assign_driver(_impl->driver());
    }

private:
    const std::unique_ptr<sudoku_tiling_intf> _impl;
};
//------------------------------------------------------------------------------
} // namespace eagine::msgbus

