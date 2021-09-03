///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#include "SolutionIntervalViewModel.hpp"
#include <eagine/iterator.hpp>

//------------------------------------------------------------------------------
SolutionIntervalViewModel::SolutionIntervalViewModel(
  eagine::main_ctx_parent parent)
  : QObject{nullptr}
  , eagine::main_ctx_object{EAGINE_ID(IntvlModel), parent} {
    for(const auto& interval : eagine::reverse(_intervals)) {
        _intervalList.append(interval.count());
    }
    _intervalList.append(0.F);
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
    for(const auto& interval : eagine::reverse(_intervals)) {
        _maxInterval = std::max(_maxInterval, interval);
        _intervalList.append(interval.count());
    }
    _intervalList.append(0.F);
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
    const std::chrono::duration<float> current{now - _previousSolutionTime};
    EAGINE_ASSERT(!_intervalList.empty());
    _maxInterval = std::max(_maxInterval, current);
    _intervalList.back() = current.count();
    emit dataChanged();
}
//------------------------------------------------------------------------------
auto SolutionIntervalViewModel::getIntervals() const -> const QVariantList& {
    return _intervalList;
}
//------------------------------------------------------------------------------
auto SolutionIntervalViewModel::getMaxInterval() const -> qreal {
    return _maxInterval.count();
}
//------------------------------------------------------------------------------
