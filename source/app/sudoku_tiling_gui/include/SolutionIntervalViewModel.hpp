///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#ifndef EAGINE_MSGBUS_SOLUTION_INTERVAL_VIEW_MODEL
#define EAGINE_MSGBUS_SOLUTION_INTERVAL_VIEW_MODEL

import eagine.core;
import std;
#include <QObject>
#include <QVariant>

class TilingBackend;
//------------------------------------------------------------------------------
class SolutionIntervalViewModel final
  : public QObject
  , public eagine::main_ctx_object {
    Q_OBJECT

    Q_PROPERTY(QVariantList intervals READ getIntervals NOTIFY dataChanged)
    Q_PROPERTY(qreal maxInterval READ getMaxInterval NOTIFY dataChanged)
    Q_PROPERTY(
      QVariantList fixedIntervals READ getFixedIntervals NOTIFY dataChanged)
    Q_PROPERTY(
      qreal maxFixedInterval READ getMaxFixedInterval NOTIFY dataChanged)
public:
    SolutionIntervalViewModel(TilingBackend&);
    ~SolutionIntervalViewModel() final;

    void tilingReset();
    void helperContributed(eagine::identifier_t helperId);

    auto getIntervals() const -> const QVariantList&;
    auto getFixedIntervals() const -> const QVariantList&;
    auto getMaxInterval() const -> qreal;
    auto getMaxFixedInterval() const -> qreal;
signals:
    void dataChanged();

private:
    void timerEvent(QTimerEvent*) final;
    void addInterval();
    auto fixInterval(float) const noexcept;

    TilingBackend& _backend;
    int _timerId{0};
    std::chrono::steady_clock::time_point _previousSolutionTime{
      std::chrono::steady_clock::now()};
    std::chrono::duration<float> _maxInterval{1.F};
    eagine::variable_with_history<std::chrono::duration<float>, 128> _intervals;
    QVariantList _intervalList;
    QVariantList _fixedIntervalList;
};
//------------------------------------------------------------------------------
#endif
