import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import MacDockShell.Components

Window {
    id: root
    width: 1440
    height: 900
    visible: true
    color: "transparent"
    title: "MacDockShell"
    flags: Qt.FramelessWindowHint | Qt.Window

    Rectangle {
        anchors.fill: parent
        color: "#0B0B0D"

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#111318" }
                GradientStop { position: 0.55; color: "#0C0E12" }
                GradientStop { position: 1.0; color: "#090A0D" }
            }
        }

        Rectangle {
            width: 540
            height: 540
            radius: width / 2
            color: "#1C5DFF20"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 80
            opacity: 0.9
        }

        Rectangle {
            width: 360
            height: 360
            radius: width / 2
            color: "#7BE7FF14"
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 110
            anchors.topMargin: 60
        }

        TopBar {
            id: topBar
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 12
            width: parent.width - 28
            height: 34
        }

        Rectangle {
            id: launcherCard
            width: 460
            height: 72
            radius: 24
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            color: "#F5F7FA10"
            border.width: 1
            border.color: "#FFFFFF20"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                spacing: 16

                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    color: "#FFFFFF10"
                    border.width: 1
                    border.color: "#FFFFFF18"

                    Text {
                        anchors.centerIn: parent
                        text: "⌘"
                        color: "#F5F7FA"
                        font.pixelSize: 18
                        font.weight: Font.Medium
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "Search apps, files, actions"
                        color: "#F5F7FA"
                        font.pixelSize: 17
                        font.weight: Font.Medium
                    }

                    Text {
                        text: "Prototype launcher panel"
                        color: "#B7BDC9"
                        font.pixelSize: 12
                    }
                }

                Rectangle {
                    width: 72
                    height: 30
                    radius: 15
                    color: "#7AA2FF22"
                    border.width: 1
                    border.color: "#7AA2FF55"

                    Text {
                        anchors.centerIn: parent
                        text: "Ctrl+Space"
                        color: "#DCE8FF"
                        font.pixelSize: 11
                    }
                }
            }
        }

        Dock {
            id: dock
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 22
            modelData: [
                { symbol: "F", label: "Finder" },
                { symbol: "L", label: "Launchpad" },
                { symbol: "S", label: "Safari" },
                { symbol: "M", label: "Mail" },
                { symbol: "N", label: "Notes" },
                { symbol: "T", label: "Terminal" },
                { symbol: "P", label: "Photos" },
                { symbol: "⚙", label: "Settings" }
            ]
        }
    }
}
