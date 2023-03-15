///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#include "TilingBackend.hpp"
#include "TilingModel.hpp"
//------------------------------------------------------------------------------
TilingBackend::TilingBackend(eagine::main_ctx_parent parent)
  : QObject{nullptr}
  , eagine::main_ctx_object{"Backend", parent}
  , _tilingModel{std::make_shared<TilingModel>(*this)}
  , _tilingTheme{*this}
  , _tilingViewModel{*this}
  , _solutionProgressViewModel{*this}
  , _helperContributionViewModel{*this}
  , _solutionIntervalViewModel{*this} {
    _tilingModel->initialize();
    _timerId = startTimer(1);
    emit tilingModelChanged();
}
//------------------------------------------------------------------------------
TilingBackend::~TilingBackend() {
    killTimer(_timerId);
}
//------------------------------------------------------------------------------
void TilingBackend::timerEvent(QTimerEvent*) {
    if(_tilingModel) {
        _tilingModel->update();
    }
}
//------------------------------------------------------------------------------
void TilingBackend::onTilingReset() {
    _solutionProgressViewModel.tilingReset();
    _solutionIntervalViewModel.tilingReset();
}
//------------------------------------------------------------------------------
void TilingBackend::onHelperAppeared(eagine::identifier_t helperId) {
    _helperContributionViewModel.helperAppeared(helperId);
}
//------------------------------------------------------------------------------
void TilingBackend::onHelperContributed(eagine::identifier_t helperId) {
    _solutionIntervalViewModel.helperContributed(helperId);
    _helperContributionViewModel.helperContributed(helperId);
}
//------------------------------------------------------------------------------
void TilingBackend::onCellSolved(int x, int y) {
    _solutionProgressViewModel.cellSolved(x, y);
}
//------------------------------------------------------------------------------
auto TilingBackend::lightTheme() const noexcept -> bool {
    return _tilingTheme.getLight();
}
//------------------------------------------------------------------------------
auto TilingBackend::getTilingModel() noexcept -> TilingModel* {
    return _tilingModel.get();
}
//------------------------------------------------------------------------------
auto TilingBackend::getTilingTheme() noexcept -> TilingTheme* {
    return &_tilingTheme;
}
//------------------------------------------------------------------------------
auto TilingBackend::getTilingViewModel() noexcept -> TilingViewModel* {
    return &_tilingViewModel;
}
//------------------------------------------------------------------------------
auto TilingBackend::getSolutionProgressViewModel() noexcept
  -> SolutionProgressViewModel* {
    return &_solutionProgressViewModel;
}
//------------------------------------------------------------------------------
auto TilingBackend::getHelperContributionViewModel() noexcept
  -> HelperContributionViewModel* {
    return &_helperContributionViewModel;
}
//------------------------------------------------------------------------------
auto TilingBackend::getSolutionIntervalViewModel() noexcept
  -> SolutionIntervalViewModel* {
    return &_solutionIntervalViewModel;
}
//------------------------------------------------------------------------------
