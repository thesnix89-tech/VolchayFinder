import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: dockWindow
    width: Math.min(dockRow.implicitWidth + 32, Screen.width - 40)
    height: 92
    x: Math.round((Screen.width - width) / 2)
    y: Screen.height - height - 18
    visible: true
    color: "transparent"
    title: "MacDockShellDock"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    Timer {
        interval: 1500
        repeat: true
        running: false
        onTriggered: dockModel.refresh()
    }

    Rectangle {
        anchors.fill: parent
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
                        required property string iconUrl
                        required property bool running
                        required property bool active
                        required property bool pinned
                        required property bool minimized
                        required property bool clickable

                        width: mouseArea.containsMouse ? 72 : 54
                        height: 72

                        opacity: dockItemRoot.running || dockItemRoot.pinned ? 1.0 : 0.55

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

                            Item {
                                anchors.centerIn: parent
                                width: mouseArea.containsMouse ? 34 : 30
                                height: mouseArea.containsMouse ? 34 : 30

                                Image {
                                    id: dockIconImage
                                    anchors.fill: parent
                                    source: dockItemRoot.iconUrl
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    mipmap: true
                                    visible: status === Image.Ready
                                }

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 8
                                    color: "#D7DDE780"
                                    visible: dockIconImage.status !== Image.Ready
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: dockItemRoot.iconHint.length > 0 ? dockItemRoot.iconHint : dockItemRoot.label.slice(0, 1)
                                    color: "#1C222B"
                                    font.pixelSize: mouseArea.containsMouse ? 24 : 20
                                    font.weight: Font.DemiBold
                                    visible: dockIconImage.status !== Image.Ready
                                }
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
                            opacity: dockItemRoot.running ? 0.95 : 0.55
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
                                if (!dockItemRoot.clickable)
                                    return
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
}
