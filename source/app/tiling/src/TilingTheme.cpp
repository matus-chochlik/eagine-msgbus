///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///

#include "TilingTheme.hpp"
#include <eagine/app_config.hpp>
//------------------------------------------------------------------------------
TilingTheme::TilingTheme(eagine::main_ctx_parent parent)
  : QObject{nullptr}
  , eagine::main_ctx_object{EAGINE_ID(Theme), parent} {
    setTileSize(extract_or(
      app_config().get<int>("msgbus.sudoku.solver.gui.tile_size"), 16));
}
//------------------------------------------------------------------------------
void TilingTheme::setLight(bool value) {
    _light = value;
    emit lightChanged();
}
//------------------------------------------------------------------------------
auto TilingTheme::getLight() const -> bool {
    return _light;
}
//------------------------------------------------------------------------------
auto TilingTheme::getTileset() const -> QString {
    return _tileset;
}
//------------------------------------------------------------------------------
auto TilingTheme::getTileWidth() const -> int {
    return _tileSize;
}
//------------------------------------------------------------------------------
auto TilingTheme::getTileHeight() const -> int {
    return _tileSize;
}
//------------------------------------------------------------------------------
void TilingTheme::setTileset(QString tileset) {
    _tileset = std::move(tileset);
    emit tilesetChanged();
}
//------------------------------------------------------------------------------
void TilingTheme::setTileSize(int size) {
    _tileSize = size;
    emit tileSizeChanged();
}
//------------------------------------------------------------------------------
