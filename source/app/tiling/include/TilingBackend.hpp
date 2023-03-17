///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#ifndef EAGINE_MSGBUS_TILING_BACKEND
#define EAGINE_MSGBUS_TILING_BACKEND

import eagine.core;
import std;
#include "HelperContributionViewModel.hpp"
#include "SolutionIntervalViewModel.hpp"
#include "SolutionProgressViewModel.hpp"
#include "TilingTheme.hpp"
#include "TilingViewModel.hpp"
#include <QObject>

class TilingModel;
//------------------------------------------------------------------------------
class TilingBackend final
  : public QObject
  , public eagine::main_ctx_object {
    Q_OBJECT

    Q_PROPERTY(TilingTheme* theme READ getTilingTheme CONSTANT)
    Q_PROPERTY(TilingViewModel* tiling READ getTilingViewModel CONSTANT)
    Q_PROPERTY(SolutionProgressViewModel* solutionProgress READ
                 getSolutionProgressViewModel CONSTANT)
    Q_PROPERTY(HelperContributionViewModel* helperContributions READ
                 getHelperContributionViewModel CONSTANT)
    Q_PROPERTY(SolutionIntervalViewModel* solutionIntervals READ
                 getSolutionIntervalViewModel CONSTANT)
public:
    TilingBackend(eagine::main_ctx_parent);
    ~TilingBackend() final;

    auto lightTheme() const noexcept -> bool;

    auto getTilingSize() const noexcept -> QSize;
    auto getTilingModel() noexcept -> TilingModel*;
    auto getTilingTheme() noexcept -> TilingTheme*;
    auto getTilingViewModel() noexcept -> TilingViewModel*;
    auto getSolutionProgressViewModel() noexcept -> SolutionProgressViewModel*;
    auto getHelperContributionViewModel() noexcept
      -> HelperContributionViewModel*;
    auto getSolutionIntervalViewModel() noexcept -> SolutionIntervalViewModel*;
signals:
    void tilingModelChanged();
public slots:
    void onTilingReset();
    void onHelperAppeared(eagine::identifier_t helperId);
    void onHelperContributed(eagine::identifier_t helperId);
    void onTileSolved(int x, int y);

private:
    void timerEvent(QTimerEvent*) final;

    int _timerId{0};
    std::shared_ptr<TilingModel> _tilingModel;
    TilingTheme _tilingTheme;
    TilingViewModel _tilingViewModel;
    SolutionProgressViewModel _solutionProgressViewModel;
    HelperContributionViewModel _helperContributionViewModel;
    SolutionIntervalViewModel _solutionIntervalViewModel;
};
//------------------------------------------------------------------------------
#endif
