///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

import eagine.core;
import <cmath>;
#include "SolutionIntervalViewModel.hpp"
#include "TilingBackend.hpp"
#include "TilingModel.hpp"
#include <cassert>

//------------------------------------------------------------------------------
auto SolutionIntervalViewModel::fixInterval(float i) const noexcept {
    return i > 1.F ? 1 + std::log10(i) : std::sqrt(i);
}
//------------------------------------------------------------------------------
SolutionIntervalViewModel::SolutionIntervalViewModel(TilingBackend& backend)
  : QObject{nullptr}
  , eagine::main_ctx_object{"IntvlModel", backend}
  , _backend{backend} {
    for(const auto& interval : eagine::reverse(_intervals)) {
        _intervalList.append(interval.count());
        _fixedIntervalList.append(fixInterval(interval.count()));
    }
    _intervalList.append(0.F);
    _fixedIntervalList.append(0.F);
    _maxInterval = std::chrono::duration<float>{1.F};
    _timerId = startTimer(500);
}
//------------------------------------------------------------------------------
SolutionIntervalViewModel::~SolutionIntervalViewModel() {
    killTimer(_timerId);
}
//------------------------------------------------------------------------------
void SolutionIntervalViewModel::addInterval() {
    const auto now = std::chrono::steady_clock::now();
    _intervals.assign(now - _previousSolutionTime);
    _previousSolutionTime = now;
    _maxInterval = std::chrono::duration<float>{1.F};
    _intervalList.clear();
    _fixedIntervalList.clear();
    for(const auto& interval : eagine::reverse(_intervals)) {
        _maxInterval = std::max(_maxInterval, interval);
        _intervalList.append(interval.count());
        _fixedIntervalList.append(fixInterval(interval.count()));
    }
    _intervalList.append(0.F);
    _fixedIntervalList.append(0.F);
}
//------------------------------------------------------------------------------
void SolutionIntervalViewModel::tilingReset() {
    addInterval();
}
//------------------------------------------------------------------------------
void SolutionIntervalViewModel::helperContributed(eagine::identifier_t) {
    addInterval();
    emit dataChanged();
}
//------------------------------------------------------------------------------
void SolutionIntervalViewModel::timerEvent(QTimerEvent*) {
    const auto now = std::chrono::steady_clock::now();
    if(const auto tilingModel{_backend.getTilingModel()}) {
        if(extract(tilingModel).isComplete()) {
            _previousSolutionTime = now;
        }
    }
    const std::chrono::duration<float> current{now - _previousSolutionTime};
    assert(!_intervalList.empty());
    _maxInterval = std::max(_maxInterval, current);
    _intervalList.back() = current.count();
    _fixedIntervalList.back() = fixInterval(current.count());
    emit dataChanged();
}
//------------------------------------------------------------------------------
auto SolutionIntervalViewModel::getIntervals() const -> const QVariantList& {
    return _intervalList;
}
//------------------------------------------------------------------------------
auto SolutionIntervalViewModel::getFixedIntervals() const
  -> const QVariantList& {
    return _fixedIntervalList;
}
//------------------------------------------------------------------------------
auto SolutionIntervalViewModel::getMaxInterval() const -> qreal {
    return _maxInterval.count();
}
//------------------------------------------------------------------------------
auto SolutionIntervalViewModel::getMaxFixedInterval() const -> qreal {
    using std::ceil;
    return ceil(fixInterval(_maxInterval.count()));
}
//------------------------------------------------------------------------------
