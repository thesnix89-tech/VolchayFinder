import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: root
    width: Screen.width
    height: Screen.height
    visible: true
    color: "transparent"
    title: "MacDockShell"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    Component.onCompleted: {
        root.showFullScreen()
        taskbarController.hideTaskbar()
        dockModel.refresh()
    }

    onClosing: function(close) {
        taskbarController.restoreShell()
    }

    Timer {
        interval: 2000
        repeat: true
        running: true
        onTriggered: dockModel.refresh()
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Rectangle {
            id: topBar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 8
            height: 34
            radius: 18
            color: "#F8F9FBE6"
            border.width: 1
            border.color: "#FFFFFFCC"

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

        Rectangle {
            id: dock
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            width: Math.min(dockRow.implicitWidth + 32, parent.width - 40)
            height: 92
            radius: 28
            color: "#F8F9FBEA"
            border.width: 1
            border.color: "#FFFFFFD8"

            Flickable {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                contentWidth: dockRow.implicitWidth
                contentHeight: dockRow.height
                clip: true
                interactive: contentWidth > width

                Row {
                    id: dockRow
                    y: 10
                    spacing: 14

                    Repeater {
                        model: dockModel

                        delegate: Item {
                            id: dockItemRoot
                            required property int index
                            required property string label
                            required property string windowTitle
                            required property string iconHint
                            required property bool running
                            required property bool active
                            required property bool pinned
                            required property bool minimized

                            width: mouseArea.containsMouse ? 72 : 54
                            height: 72

                            Behavior on width {
                                NumberAnimation {
                                    duration: 140
                                    easing.type: Easing.OutCubic
                                }
                            }

                            Rectangle {
                                id: iconBubble
                                width: parent.width
                                height: parent.width
                                radius: 18
                                anchors.bottom: parent.bottom
                                color: dockItemRoot.active ? "#EAF2FF" : (mouseArea.containsMouse ? "#FFFFFFF0" : "#FFFFFFCC")
                                border.width: 1
                                border.color: dockItemRoot.active ? "#BFD3FF" : (mouseArea.containsMouse ? "#D7DDE7" : "#E4E8EE")
                                y: mouseArea.containsMouse ? -8 : 0

                                Behavior on y {
                                    NumberAnimation {
                                        duration: 140
                                        easing.type: Easing.OutCubic
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: dockItemRoot.iconHint.length > 0 ? dockItemRoot.iconHint : dockItemRoot.label.slice(0, 1)
                                    color: "#1C222B"
                                    font.pixelSize: mouseArea.containsMouse ? 24 : 20
                                    font.weight: Font.DemiBold
                                }
                            }

                            Rectangle {
                                width: dockItemRoot.running ? 8 : (dockItemRoot.pinned ? 5 : 0)
                                height: 5
                                radius: 3
                                visible: width > 0
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 2
                                color: dockItemRoot.active ? "#7FAEFF" : (dockItemRoot.running ? "#9AA7BD" : "#C7CDD6")
                                opacity: 0.95
                            }

                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                anchors.bottom: iconBubble.top
                                anchors.bottomMargin: 10
                                width: tooltipText.implicitWidth + 18
                                height: 34
                                radius: 14
                                color: "#FFFFFFF7"
                                border.width: 1
                                border.color: "#DDE3EC"
                                opacity: mouseArea.containsMouse ? 1 : 0
                                visible: opacity > 0

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: 110
                                    }
                                }

                                Column {
                                    anchors.centerIn: parent
                                    spacing: 1

                                    Text {
                                        id: tooltipText
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: dockItemRoot.label
                                        color: "#1C222B"
                                        font.pixelSize: 11
                                    }

                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: dockItemRoot.windowTitle
                                        color: "#6B7380"
                                        font.pixelSize: 9
                                        visible: dockItemRoot.windowTitle.length > 0
                                    }
                                }
                            }

                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                cursorShape: Qt.PointingHandCursor
                                onClicked: function(mouse) {
                                    if (mouse.button === Qt.RightButton) {
                                        dockModel.minimizeIndex(dockItemRoot.index)
                                    } else {
                                        dockModel.toggleIndex(dockItemRoot.index)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            id: exitButton
            width: 140
            height: 38
            radius: 19
            anchors.right: parent.right
            anchors.bottom: dock.top
            anchors.rightMargin: 20
            anchors.bottomMargin: 18
            color: closeArea.containsMouse ? "#B63A3ACC" : "#8A2E2ECC"
            border.width: 1
            border.color: "#FFFFFF22"

            Text {
                anchors.centerIn: parent
                text: "Exit shell"
                color: "#F5F7FA"
                font.pixelSize: 13
                font.weight: Font.Medium
            }

            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: taskbarController.quitApplication()
            }
        }
    }
}
