import QtQuick 2.12
import "qrc:///views"
import com.github.matus_chochlik.eagine.msgbus.tiling 1.0

PaintedImage {
	id: solutionProgressView
	property var model: null

	image: model ? model.image : null
}
