import QtQuick 2.12
import QtCharts 2.1
import "qrc:///views"

ChartView {
    id: solutionIntervalView
    property var model: null
	property real axisYMax: model ? model.maxInterval : 1

    theme: backend.theme.light
        ? ChartView.ChartThemeLight
        : ChartView.ChartThemeDark
    antialiasing: true

    BarSeries {
		axisX: ValueAxis {
			tickCount: 0
			titleVisible: false
			labelsVisible: false
		}

        axisY: ValueAxis {
			titleText: qsTr("duration (seconds)")
            min: 0
            max: axisYMax
        }

        BarSet {
            label: qsTr("interval between solutions")
            values: solutionIntervalView.model
                ? solutionIntervalView.model.intervals
                : []
        }
    }

	Behavior on axisYMax {
		NumberAnimation {
			duration: 500
			easing.type: Easing.InOutBack
			easing.overshoot: 0
		}
	}
}
