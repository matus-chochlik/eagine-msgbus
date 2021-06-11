import QtQuick 2.12
import QtCharts 2.1
import "qrc:///views"

ChartView {
    id: helperContributionView
    property var model: null
	property real axisYMax: model ? model.maxSolvedCount : 1

    theme: backend.theme.light
        ? ChartView.ChartThemeLight
        : ChartView.ChartThemeDark
    antialiasing: true

    BarSeries {
        axisX: BarCategoryAxis {
            labelsAngle: 90
            categories: helperContributionView.model
                ? helperContributionView.model.helperIds
                : []
        }

        axisY: ValueAxis {
            min: 0
            max: axisYMax
        }

        BarSet {
            label: qsTr("count of solutions by helper")
            values: helperContributionView.model
                ? helperContributionView.model.solvedCounts
                : []
        }
    }

	Behavior on axisYMax {
		NumberAnimation {
			duration: 1000
			easing.type: Easing.InOutBack
			easing.overshoot: 0
		}
	}
}
