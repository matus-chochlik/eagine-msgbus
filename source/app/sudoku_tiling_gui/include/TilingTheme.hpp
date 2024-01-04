///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#ifndef EAGINE_MSGBUS_TILING_THEME
#define EAGINE_MSGBUS_TILING_THEME

import eagine.core;
#include <QObject>

//------------------------------------------------------------------------------
class TilingTheme
  : public QObject
  , public eagine::main_ctx_object {
    Q_OBJECT

    Q_PROPERTY(bool light READ getLight WRITE setLight NOTIFY lightChanged)
    Q_PROPERTY(QString tileset READ getTileset NOTIFY tilesetChanged)
    Q_PROPERTY(int tileWidth READ getTileWidth NOTIFY tileSizeChanged)
    Q_PROPERTY(int tileHeight READ getTileHeight NOTIFY tileSizeChanged)

public:
    TilingTheme(eagine::main_ctx_parent);

    void setLight(bool);
    auto getLight() const -> bool;

    auto getTileset() const -> QString;
    auto getTileWidth() const -> int;
    auto getTileHeight() const -> int;

    Q_INVOKABLE void setTileset(QString);
    Q_INVOKABLE void setTileSize(int);

signals:
    void lightChanged();
    void tilesetChanged();
    void tileSizeChanged();
public slots:
private:
    QString _tileset{"b16"};
    int _tileSize{16};
    bool _light{false};
};
//------------------------------------------------------------------------------
#endif
