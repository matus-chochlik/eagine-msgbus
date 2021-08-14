///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#ifndef EAGINE_MSGBUS_TILING_TILING_VIEW_MODEL
#define EAGINE_MSGBUS_TILING_TILING_VIEW_MODEL

#include <eagine/main_ctx_object.hpp>
#include <QAbstractTableModel>
#include <QUrl>

class TilingBackend;
//------------------------------------------------------------------------------
class TilingViewModel
  : public QAbstractTableModel
  , public eagine::main_ctx_object {
    Q_OBJECT

    Q_PROPERTY(QVariant resetCount READ getResetCount NOTIFY reinitialized)
    Q_PROPERTY(QVariant progress READ getProgress NOTIFY progressChanged)
public:
    TilingViewModel(TilingBackend&);

    auto rowCount(const QModelIndex&) const -> int final;
    auto columnCount(const QModelIndex&) const -> int final;
    auto data(const QModelIndex& index, int role) const -> QVariant final;
    auto roleNames() const -> QHash<int, QByteArray> final;

    auto getResetCount() const -> QVariant;
    auto getProgress() const -> QVariant;

    Q_INVOKABLE void reinitialize(int w, int h);
    Q_INVOKABLE void saveAs(const QUrl& filePath);
signals:
    void reinitialized();
    void progressChanged();
private slots:
    void onTilingModelChanged();
    void onTilingReset();
    void onTilingChanged();
    void onTilesAdded(int rmin, int cmin, int rmax, int cmax);

private:
    TilingBackend& _backend;
};
//------------------------------------------------------------------------------
#endif
