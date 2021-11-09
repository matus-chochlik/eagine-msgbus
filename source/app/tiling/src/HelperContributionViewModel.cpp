///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#include "HelperContributionViewModel.hpp"
#include "TilingBackend.hpp"
#include "TilingModel.hpp"

//------------------------------------------------------------------------------
HelperContributionViewModel::HelperContributionViewModel(TilingBackend& backend)
  : QObject{nullptr}
  , eagine::main_ctx_object{EAGINE_ID(CntrbModel), backend}
  , _backend{backend}
  , _timerId{startTimer(2000)} {}
//------------------------------------------------------------------------------
HelperContributionViewModel::~HelperContributionViewModel() {
    killTimer(_timerId);
}
//------------------------------------------------------------------------------
void HelperContributionViewModel::_cacheHelpers() {
    _helperIds.clear();
    for(const auto id : _helpers) {
        _helperIds.append(QString::number(id));
    }
}
//------------------------------------------------------------------------------
void HelperContributionViewModel::_cacheCounts() {
    _updatedCounts.clear();
    _solvedCounts.clear();
    if(auto tilingModel{_backend.getTilingModel()}) {
        for(const auto helperId : _helpers) {
            const auto updatedByHelper =
              extract(tilingModel).getUpdatedByHelper(helperId);
            _maxUpdatedCount = std::max(_maxUpdatedCount, updatedByHelper);
            _updatedCounts.append(updatedByHelper);

            const auto solvedByHelper =
              extract(tilingModel).getSolvedByHelper(helperId);
            _maxSolvedCount = std::max(_maxSolvedCount, solvedByHelper);
            _solvedCounts.append(solvedByHelper);
        }
    }
}
//------------------------------------------------------------------------------
void HelperContributionViewModel::timerEvent(QTimerEvent*) {
    _cacheCounts();
    emit solved();
}
//------------------------------------------------------------------------------
void HelperContributionViewModel::helperAppeared(eagine::identifier_t helperId) {
    if(_helpers.insert(helperId).second) {
        _cacheHelpers();
        _cacheCounts();
        emit helpersChanged();
    }
}
//------------------------------------------------------------------------------
void HelperContributionViewModel::helperContributed(
  eagine::identifier_t helperId) {
    _helpers.insert(helperId);
    _cacheCounts();
    emit solved();
}
//------------------------------------------------------------------------------
auto HelperContributionViewModel::getHelperIds() const -> const QStringList& {
    return _helperIds;
}
//------------------------------------------------------------------------------
auto HelperContributionViewModel::getUpdatedCounts() const
  -> const QVariantList& {
    return _updatedCounts;
}
//------------------------------------------------------------------------------
auto HelperContributionViewModel::getMaxUpdatedCount() const -> qreal {
    return static_cast<qreal>(_maxUpdatedCount);
}
//------------------------------------------------------------------------------
auto HelperContributionViewModel::getSolvedCounts() const
  -> const QVariantList& {
    return _solvedCounts;
}
//------------------------------------------------------------------------------
auto HelperContributionViewModel::getMaxSolvedCount() const -> qreal {
    return static_cast<qreal>(_maxSolvedCount);
}
//------------------------------------------------------------------------------
