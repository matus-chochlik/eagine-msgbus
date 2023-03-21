///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

import eagine.core;
import std;
#include "SolutionProgressViewModel.hpp"
#include "TilingBackend.hpp"
#include "TilingModel.hpp"
#include <cassert>

//------------------------------------------------------------------------------
SolutionProgressViewModel::SolutionProgressViewModel(TilingBackend& backend)
  : QObject{nullptr}
  , eagine::main_ctx_object{"PrgrsModel", backend}
  , _backend{backend}
  , _imageDir{QDir::tempPath() + "/eagine-tiling-XXXXXX"}
  , _imagePathFormat{_imageDir.filePath("%1.png")} {
    connect(
      _backend.getTilingTheme(),
      &TilingTheme::lightChanged,
      this,
      &SolutionProgressViewModel::onThemeChanged);
}
//------------------------------------------------------------------------------
void SolutionProgressViewModel::tilingReset() {
    if(auto tilingModel{_backend.getTilingModel()}) {
        emit sizeChanged();

        _image = QImage{_backend.getTilingSize(), QImage::Format_Mono};
        if(_backend.lightTheme()) {
            _image.fill(Qt::color1);
        } else {
            _image.fill(Qt::color0);
        }
        _imageIndex = 0;
        emit imageChanged();
    }
}
//------------------------------------------------------------------------------
void SolutionProgressViewModel::onThemeChanged() {
    _image.invertPixels();
    emit imageChanged();
}
//------------------------------------------------------------------------------
auto SolutionProgressViewModel::getImage() const -> const QImage* {
    return &_image;
}
//------------------------------------------------------------------------------
auto SolutionProgressViewModel::getSize() const -> QSize {
    return _backend.getTilingSize();
}
//------------------------------------------------------------------------------
void SolutionProgressViewModel::saveImage() {
    if(_doSaveImage) {
        _image.save(_imagePathFormat.arg(_imageIndex, 7, 10, QChar('0')));
        ++_imageIndex;
    }
}
//------------------------------------------------------------------------------
void SolutionProgressViewModel::tileSolved(int x, int y) {
    if(_backend.lightTheme()) {
        _image.setPixel(x, y, Qt::color0);
    } else {
        _image.setPixel(x, y, Qt::color1);
    }
    saveImage();
    emit imageChanged();
}
//------------------------------------------------------------------------------
