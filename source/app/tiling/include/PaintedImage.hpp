///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#ifndef EAGINE_MSGBUS_PAINTED_IMAGE
#define EAGINE_MSGBUS_PAINTED_IMAGE

import std;
#include <QImage>
#include <QObject>
#include <QQuickPaintedItem>

//------------------------------------------------------------------------------
class PaintedImage : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(
      const QImage* image READ getImage WRITE setImage NOTIFY imageChanged)
public:
    PaintedImage(QQuickItem* parent = nullptr);

    auto getImage() -> const QImage*;
    void setImage(const QImage*);
    void paint(QPainter*) final;
signals:
    void imageChanged();

private:
    const QImage* _image{nullptr};
};
//------------------------------------------------------------------------------
#endif
