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
    visible: true
    color: "transparent"
    title: "MacDockShellTopBar"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    Rectangle {
        anchors.fill: parent
        color: "#3CFFFFFF" // Translucent white (approx. 24% opacity)
        border.width: 1
        border.color: "#66FFFFFF" // Semi-transparent border (40% opacity)

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Math.max(0, parent.radius - 1)
            color: "transparent"
            border.width: 1
            border.color: "#1AFFFFFF" // Subtle inner specular highlight (10% opacity)
        }

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
