import QtQuick 2.12
import QtCharts 2.1
import QtQuick.Controls 2.4
import QtQuick.Layouts 1.5
import "qrc:///views"

Pane {
	id: helperContributionView
	property var model: null

	ColumnLayout {
		anchors.fill: parent

		ChartView {
			id: updateCountChart

			Layout.fillWidth: true
			Layout.fillHeight: true

			property real axisYMax: helperContributionView.model
				? helperContributionView.model.maxUpdatedCount
				: 1

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
					max: updateCountChart.axisYMax
				}

				BarSet {
					label: qsTr("count of boards by helper")
					values: helperContributionView.model
						? helperContributionView.model.updatedCounts
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


		ChartView {
			id: solutionCountChart

			Layout.fillWidth: true
			Layout.fillHeight: true

			property real axisYMax: helperContributionView.model
				? helperContributionView.model.maxSolvedCount
				: 1

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
					max: solutionCountChart.axisYMax
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
	}
}
