///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#include "TilingViewModel.hpp"
#include "TilingBackend.hpp"
#include "TilingModel.hpp"
#include <QDir>
#include <QFile>
//------------------------------------------------------------------------------
TilingViewModel::TilingViewModel(TilingBackend& backend)
  : QAbstractTableModel{nullptr}
  , eagine::main_ctx_object{"TilingVM", backend}
  , _backend{backend} {
    std::string filePath;
    if(app_config().fetch("msgbus.sudoku.solver.output_path", filePath)) {
        _filePath = QUrl::fromLocalFile(filePath.c_str());
    }
    connect(
      _backend.getTilingTheme(),
      &TilingTheme::tileSizeChanged,
      this,
      &TilingViewModel::onTilingChanged);
    connect(
      &_backend,
      &TilingBackend::tilingModelChanged,
      this,
      &TilingViewModel::onTilingModelChanged);
}
//------------------------------------------------------------------------------
void TilingViewModel::reinitialize() {
    if(auto tilingModel{_backend.getTilingModel()}) {
        extract(tilingModel).reinitialize();
        emit reinitialized();
    }
}
//------------------------------------------------------------------------------
void TilingViewModel::reinitialize(int w, int h) {
    if(auto tilingModel{_backend.getTilingModel()}) {
        extract(tilingModel).reinitialize(w, h);
        emit reinitialized();
    }
}
//------------------------------------------------------------------------------
void TilingViewModel::resetTimeout() {
    if(auto tilingModel{_backend.getTilingModel()}) {
        extract(tilingModel).resetTimeout();
    }
}
//------------------------------------------------------------------------------
void TilingViewModel::doSaveAs(const QUrl& filePath) {
    if(auto optTilingModel{_backend.getTilingModel()}) {
        QFile tilingFile(QDir::toNativeSeparators(filePath.toLocalFile()));
        if(tilingFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            auto& tilingModel = extract(optTilingModel);
            for(const auto row :
                eagine::integer_range(tilingModel.getHeight())) {
                for(const auto column :
                    eagine::integer_range(tilingModel.getWidth())) {
                    tilingFile.putChar(tilingModel.getCellChar(row, column));
                }
                tilingFile.putChar('\n');
            }
        }
    }
}
//------------------------------------------------------------------------------
void TilingViewModel::saveAs(const QUrl& filePath) {
    if(auto optTilingModel{_backend.getTilingModel()}) {
        auto& tilingModel = extract(optTilingModel);
        if(tilingModel.isComplete()) {
            doSaveAs(filePath);
            _filePath.clear();
            emit filePathChanged();
            return;
        }
    }
    _filePath = filePath;
    emit filePathChanged();
}
//------------------------------------------------------------------------------
auto TilingViewModel::rowCount(const QModelIndex&) const -> int {
    if(auto tilingModel{_backend.getTilingModel()}) {
        return extract(tilingModel).getHeight();
    }
    return 0;
}
//------------------------------------------------------------------------------
auto TilingViewModel::columnCount(const QModelIndex&) const -> int {
    if(auto tilingModel{_backend.getTilingModel()}) {
        return extract(tilingModel).getWidth();
    }
    return 0;
}
//------------------------------------------------------------------------------
auto TilingViewModel::data(const QModelIndex& index, int role) const
  -> QVariant {
    if(role == Qt::DisplayRole) {
        if(auto tilingModel{_backend.getTilingModel()}) {
            return extract(tilingModel).getCell(index.row(), index.column());
        }
    }
    return {};
}
//------------------------------------------------------------------------------
auto TilingViewModel::roleNames() const -> QHash<int, QByteArray> {
    return {{Qt::DisplayRole, "tile"}};
}
//------------------------------------------------------------------------------
auto TilingViewModel::getFilePath() const -> QVariant {
    return _filePath.isEmpty() ? QVariant{} : QVariant{_filePath};
}
//------------------------------------------------------------------------------
auto TilingViewModel::getResetCount() const -> QVariant {
    if(auto tilingModel{_backend.getTilingModel()}) {
        return extract(tilingModel).getResetCount();
    }
    return {};
}
//------------------------------------------------------------------------------
auto TilingViewModel::getProgress() const -> QVariant {
    if(auto tilingModel{_backend.getTilingModel()}) {
        return extract(tilingModel).getProgress();
    }
    return {};
}
//------------------------------------------------------------------------------
auto TilingViewModel::getKeyCount() const -> QVariant {
    if(auto tilingModel{_backend.getTilingModel()}) {
        return extract(tilingModel).getKeyCount();
    }
    return {};
}
//------------------------------------------------------------------------------
auto TilingViewModel::getBoardCount() const -> QVariant {
    if(auto tilingModel{_backend.getTilingModel()}) {
        return extract(tilingModel).getBoardCount();
    }
    return {};
}
//------------------------------------------------------------------------------
auto TilingViewModel::isComplete() const -> bool {
    if(auto tilingModel{_backend.getTilingModel()}) {
        return extract(tilingModel).isComplete();
    }
    return false;
}
//------------------------------------------------------------------------------
void TilingViewModel::onTilingModelChanged() {
    connect(
      _backend.getTilingModel(),
      &TilingModel::reinitialized,
      this,
      &TilingViewModel::onTilingReset);
    connect(
      _backend.getTilingModel(),
      &TilingModel::fragmentAdded,
      this,
      &TilingViewModel::onTilesAdded);
    connect(
      _backend.getTilingModel(),
      &TilingModel::queueLengthChanged,
      this,
      &TilingViewModel::onQueueLengthChanged);
    emit modelReset({});
    emit reinitialized();
    emit progressChanged();
}
//------------------------------------------------------------------------------
void TilingViewModel::onTilingReset() {
    emit modelReset({});
    emit reinitialized();
    emit progressChanged();
}
//------------------------------------------------------------------------------
void TilingViewModel::onTilingChanged() {
    emit modelReset({});
    emit progressChanged();
}
//------------------------------------------------------------------------------
void TilingViewModel::onTilesAdded(int rmin, int cmin, int rmax, int cmax) {
    emit dataChanged(createIndex(rmin, cmin), createIndex(rmax + 1, cmax + 1));
    emit progressChanged();
    if(not _filePath.isEmpty()) {
        if(auto optTilingModel{_backend.getTilingModel()}) {
            if(extract(optTilingModel).isComplete()) {
                doSaveAs(_filePath);
                _filePath.clear();
                emit filePathChanged();
            }
        }
    }
}
//------------------------------------------------------------------------------
void TilingViewModel::onQueueLengthChanged() {
    emit queueLengthChanged();
}
//------------------------------------------------------------------------------
