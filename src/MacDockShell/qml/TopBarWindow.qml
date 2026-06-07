import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: topBarWindow
    width: Screen.width
    height: 34
    x: 0
    y: 0
    visible: taskbarController.shellActive && taskbarController.showTopBar
    color: "transparent"
    title: "MacDockShellTopBar"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    Rectangle {
        anchors.fill: parent
        color: "#F6F3F4F6"
        border.width: 1
        border.color: "#E2E4E9"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 18

            RowLayout {
                spacing: 12

                Text {
                    text: ""
                    color: "#1A1F26"
                    font.pixelSize: 16
                    font.weight: Font.Medium

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: taskbarController.settingsVisible = !taskbarController.settingsVisible
                    }
                }

                Repeater {
                    model: ["Finder", "File", "Edit", "View", "Go", "Window", "Help"]
                    delegate: Text {
                        text: modelData
                        color: "#1B1F27"
                        font.pixelSize: 12
                        opacity: 0.92
                    }
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "Windows Dock Bridge"
                color: "#5F6773"
                font.pixelSize: 12
            }

            RowLayout {
                spacing: 10

                Repeater {
                    model: ["⌃", "◌", "◎", Qt.formatTime(new Date(), "hh:mm")]
                    delegate: Text {
                        text: modelData
                        color: "#1B1F27"
                        font.pixelSize: 12
                        opacity: 0.92
                    }
                }
            }
        }
    }
}
