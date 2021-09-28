import QtQuick 2.12
import QtCharts 2.1
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.5
import "qrc:///views"

Pane {
	id: solutionIntervalView
	property var model: null

	ColumnLayout {
		anchors.fill: parent

		ChartView {
	 		id: fixedIntervalChart

			Layout.fillWidth: true
			Layout.fillHeight: true

			property real axisYMax: solutionIntervalView.model
				? solutionIntervalView.model.maxFixedInterval
				: 1

			theme: backend.theme.light
				? ChartView.ChartThemeLight
				: ChartView.ChartThemeDark
			antialiasing: true
			legend.visible: false

			BarSeries {
				axisX: ValueAxis {
					tickCount: 0
					titleVisible: false
					labelsVisible: false
				}

				axisY: ValueAxis {
					titleText: qsTr("~log(seconds)")
					visible: false
					min: 0
					max: fixedIntervalChart.axisYMax
					labelsVisible: false
				}

				BarSet {
					label: qsTr("interval between solutions")
					values: solutionIntervalView.model
						? solutionIntervalView.model.fixedIntervals
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

		ChartView {
			id: intervalChart

			Layout.fillWidth: true
			Layout.fillHeight: true

			property real axisYMax: solutionIntervalView.model
				? solutionIntervalView.model.maxInterval
				: 1

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
					titleText: qsTr("seconds")
					min: 0
					max: intervalChart.axisYMax
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
	}
}
