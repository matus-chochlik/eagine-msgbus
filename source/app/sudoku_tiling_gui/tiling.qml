///
/// Copyright Matus Chochlik.
/// Distributed under the GNU GENERAL PUBLIC LICENSE version 3.
/// See http://www.gnu.org/licenses/gpl-3.0.txt
///
import QtQuick 2.3
import QtQuick.Controls 2.4
import QtQuick.Dialogs 1.3
import QtQuick.Controls.Material 2.4
import QtQuick.Layouts 1.5
import "qrc:///views"
import "qrc:///scripts/Format.js" as Format

ApplicationWindow {
    id: root
    visible: true
    width: 1200
    height: 800
    Material.theme: backend.theme.light ? Material.Light : Material.Dark
    Material.accent: Material.Blue

    title: backend.tiling.filePath
        ? qsTr("Tiling - %1").arg(backend.tiling.filePath)
        : qsTr("Tiling")

    Action {
        id: restartTiling
        text: qsTr("&Restart tiling")
        shortcut: StandardKey.Refresh
        onTriggered: {
            backend.tiling.reinitialize()
        }
    }

    Action {
        id: resetTimeout
        text: qsTr("&Reset timeout")
        shortcut: StandardKey.Redo
        onTriggered: {
            backend.tiling.resetTimeout()
        }
    }

    Action {
        id: saveAction
        text: qsTr("&Save")
        shortcut: StandardKey.Save
        onTriggered: {
            saveDialog.open()
        }
    }

    Action {
        id: quitAction
        text: qsTr("&Quit")
        shortcut: StandardKey.Quit
        onTriggered: {
            Qt.callLater(Qt.quit)
        }
    }

    Action {
        id: lightThemeToggleAction
        text: qsTr("&Light")
        checkable: true
        checked: backend.theme.light
        onToggled: {
            backend.theme.light = checked
        }
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Menu {
                title: qsTr("&New tiling")
                Repeater {
                    model: [16, 32, 64, 128, 256, 512, 1024, 2048]
                    MenuItem {
                        text: qsTr("%1x%1").arg(modelData)
                        onTriggered: {
                            backend.tiling.reinitialize(modelData, modelData)
                        }
                    }
                }
            }
            MenuItem {
                action: restartTiling
            }
            MenuItem {
                action: resetTimeout
            }
            MenuItem {
                action: saveAction
            }
            MenuSeparator { }
            MenuItem {
                action: quitAction
            }
        }
        Menu {
            title: qsTr("&Window")
            Menu {
                title: qsTr("T&heme")
                MenuItem {
                    action: lightThemeToggleAction
                }
            }
            Menu {
                title: qsTr("Tile &size")
                Repeater {
                    model: [2, 4, 8, 16, 32]
                    MenuItem {
                        text: qsTr("%1x%1").arg(modelData)
                        onTriggered: {
                            backend.theme.setTileSize(modelData)
                        }
                    }
                }
            }
            Menu {
                title: qsTr("Tile se&t")
                Repeater {
                    model: [
                        "b16",
                        "grayscale",
                        "nodes",
                        "connections",
                        "bricks_large",
                        "bricks_small",
                        "thicket"
                    ]
                    MenuItem {
                        text: qsTr("tileset_%1").arg(modelData)
                        onTriggered: {
                            backend.theme.setTileset(modelData)
                        }
                    }
                }
            }
        }
    }

    contentData: ColumnLayout {
        anchors.fill: parent

        TabBar {
            id: mainTabBar
            Layout.fillWidth: true
            TabButton {
                text: qsTr("Tiling")
            }
            TabButton {
                text: qsTr("Solution progress")
            }
            TabButton {
                text: qsTr("Solution intervals")
            }
            TabButton {
                text: qsTr("Helper contributions")
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

            currentIndex: mainTabBar.currentIndex

            TilingView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                model: backend.tiling
            }

            SolutionProgressView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                model: backend.solutionProgress
            }

            SolutionIntervalView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                model: backend.solutionIntervals
            }

            HelperContributionView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                model: backend.helperContributions
            }
        }

        RowLayout {
            Label {
                Layout.preferredWidth: 75
                Layout.preferredHeight: 25

                text: qsTr("Resets: %1")
                    .arg(Format.integerStr(backend.tiling.resetCount))
            }
            ProgressBar {
                Layout.fillWidth: true
                Layout.preferredHeight: 25

                property real progress: backend.tiling.progress
                    ? backend.tiling.progress
                    : 0.0

                from: 0
                to: 1
                value: progress
                indeterminate: !backend.tiling.progress

                Behavior on progress {
                    NumberAnimation {
                        duration: 1000
                    }
                }
            }
            Label {
                Layout.preferredWidth: 65
                Layout.preferredHeight: 25

                text: Format.percentStr(backend.tiling.progress)
            }
        }
           RowLayout {
            Label {
                Layout.preferredWidth: 125
                Layout.preferredHeight: 25

                text: qsTr("Enqueued boards: %2 / %1")
                    .arg(Format.integerStr(backend.tiling.keyCount))
                    .arg(Format.integerStr(backend.tiling.boardCount))
            }
        }
 }

    FileDialog {
        id: saveDialog
        title: qsTr("Save tiling into file")
        folder: shortcuts.home
        selectExisting: false
        selectMultiple: false
        selectFolder: false
        onAccepted: {
            backend.tiling.saveAs(saveDialog.fileUrl)
        }

        Component.onCompleted: visible = false
    }
}
