///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#ifndef EAGINE_MSGBUS_HELPER_CONTRIBUTION_VIEW_MODEL
#define EAGINE_MSGBUS_HELPER_CONTRIBUTION_VIEW_MODEL

#include <eagine/flat_set.hpp>
#include <eagine/main_ctx_object.hpp>
#include <eagine/timeout.hpp>
#include <QObject>
#include <QVariant>
#include <tuple>

class TilingBackend;
//------------------------------------------------------------------------------
class HelperContributionViewModel
  : public QObject
  , public eagine::main_ctx_object {
    Q_OBJECT

    Q_PROPERTY(QStringList helperIds READ getHelperIds NOTIFY helpersChanged)
    Q_PROPERTY(QVariantList updatedCounts READ getUpdatedCounts NOTIFY solved)
    Q_PROPERTY(qreal maxUpdatedCount READ getMaxUpdatedCount NOTIFY solved)
    Q_PROPERTY(QVariantList solvedCounts READ getSolvedCounts NOTIFY solved)
    Q_PROPERTY(qreal maxSolvedCount READ getMaxSolvedCount NOTIFY solved)
public:
    HelperContributionViewModel(TilingBackend&);

    void helperAppeared(eagine::identifier_t helperId);
    void helperContributed(eagine::identifier_t helperId);

    auto getHelperIds() const -> const QStringList&;
    auto getUpdatedCounts() const -> const QVariantList&;
    auto getMaxUpdatedCount() const -> qreal;
    auto getSolvedCounts() const -> const QVariantList&;
    auto getMaxSolvedCount() const -> qreal;
signals:
    void helpersChanged();
    void solved();

private:
    void _cacheHelpers();
    void _cacheCounts();

    TilingBackend& _backend;
    eagine::flat_set<eagine::identifier_t> _helpers;
    QStringList _helperIds;
    QVariantList _updatedCounts;
    QVariantList _solvedCounts;
    qlonglong _maxUpdatedCount{1};
    qlonglong _maxSolvedCount{1};
};
//------------------------------------------------------------------------------
#endif
