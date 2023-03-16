///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///
#include "PaintedImage.hpp"
#include <QPainter>

//------------------------------------------------------------------------------
PaintedImage::PaintedImage(QQuickItem* parent)
  : QQuickPaintedItem{parent} {}
//------------------------------------------------------------------------------
auto PaintedImage::getImage() -> const QImage* {
    return _image;
}
//------------------------------------------------------------------------------
void PaintedImage::setImage(const QImage* image) {
    _image = image;
    update();
}
//------------------------------------------------------------------------------
void PaintedImage::paint(QPainter* painter) {
    if(_image) {
        painter->scale(2.F, 2.F);
        painter->drawImage(0, 0, *_image);
    }
}
//------------------------------------------------------------------------------
