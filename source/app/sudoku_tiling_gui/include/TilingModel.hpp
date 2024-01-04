///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#ifndef EAGINE_MSGBUS_TILING_MODEL
#define EAGINE_MSGBUS_TILING_MODEL

import eagine.core;
import eagine.msgbus;
#include <QObject>

class TilingBackend;
//------------------------------------------------------------------------------
class TilingModel
  : public QObject
  , public eagine::main_ctx_object {
    Q_OBJECT

public:
    TilingModel(TilingBackend&);

    void initialize();
    void reinitialize();
    void reinitialize(int w, int h);
    void update();
    void resetTimeout();

    auto getWidth() const noexcept -> int;
    auto getHeight() const noexcept -> int;
    auto getTilingSize() const noexcept -> QSize;
    auto getCellChar(int row, int column) const noexcept -> char;
    auto getCell(int row, int column) const noexcept -> QVariant;

    auto getResetCount() const noexcept -> QVariant;
    auto getProgress() const noexcept -> QVariant;
    auto getKeyCount() const noexcept -> QVariant;
    auto getBoardCount() const noexcept -> QVariant;
    auto isComplete() const noexcept -> bool;
    auto getUpdatedByHelper(eagine::identifier_t helperId) const noexcept
      -> qlonglong;
    auto getSolvedByHelper(eagine::identifier_t helperId) const noexcept
      -> qlonglong;

signals:
    void reinitialized();
    void queueLengthChanged();
    void fragmentAdded(int rmin, int cmin, int rmax, int cmax);

private:
    TilingBackend& _backend;

    void onHelperAppeared(
      const eagine::msgbus::result_context&,
      const eagine::msgbus::sudoku_helper_appeared&) noexcept;

    void onFragmentAdded(
      eagine::identifier_t,
      const eagine::msgbus::sudoku_tiles<4>&,
      const eagine::msgbus::sudoku_solver_key&) noexcept;

    void onQueueLengthChanged(
      const eagine::msgbus::sudoku_board_queue_change&) noexcept;

    eagine::msgbus::endpoint _bus;

    eagine::msgbus::service_composition<eagine::msgbus::pingable<
      eagine::msgbus::common_info_providers<eagine::msgbus::sudoku_tiling<>>>>
      _tiling;

    eagine::default_sudoku_board_traits<4> _traits_4;

    std::vector<char> _cellCache;
    int _width{0};
    int _height{0};
    int _resetCount{0};
    std::size_t _keyCount{0};
    std::size_t _boardCount{0};
};
//------------------------------------------------------------------------------
#endif
