///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#ifndef EAGINE_MSGBUS_SOLUTION_PROGRESS_VIEW_MODEL
#define EAGINE_MSGBUS_SOLUTION_PROGRESS_VIEW_MODEL

import eagine.core;
import std;
#include <QImage>
#include <QObject>
#include <QSize>
#include <QVariant>

class TilingBackend;
//------------------------------------------------------------------------------
class SolutionProgressViewModel final
  : public QObject
  , public eagine::main_ctx_object {
    Q_OBJECT

    Q_PROPERTY(const QImage* image READ getImage NOTIFY imageChanged)
    Q_PROPERTY(QSize size READ getSize NOTIFY sizeChanged)
public:
    SolutionProgressViewModel(TilingBackend&);

    void tilingReset();

    auto getImage() const -> const QImage*;
    auto getSize() const -> QSize;
    void tileSolved(int x, int y);
signals:
    void sizeChanged();
    void imageChanged();
private slots:
    void onThemeChanged();

private:
    TilingBackend& _backend;
    QImage _image;
    QSize _size{1, 1};
};
//------------------------------------------------------------------------------
#endif
