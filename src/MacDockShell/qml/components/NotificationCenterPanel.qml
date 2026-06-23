pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Item {
    id: root
    z: 900
    width: Math.max(330, Math.min(410, hostWidth - 28))
    height: Math.max(360, hostHeight - 54)
    x: opened ? openX : closedX
    y: 38
    opacity: opened ? 1.0 : 0.0
    visible: opened || closing
    focus: opened

    property bool darkTheme: false
    property bool transparentMode: false
    property bool opened: false
    property bool closing: false
    property var hostWindow: null

    readonly property int hostWidth: hostWindow ? hostWindow.width : Screen.width
    readonly property int hostHeight: hostWindow ? hostWindow.height : Screen.height
    readonly property int openX: Math.max(12, Math.round(hostWidth - width - 16))
    readonly property int closedX: Math.round(hostWidth + 12)
    readonly property string accessStatus: notificationCenterModel.accessStatus
    readonly property bool accessAllowed: accessStatus === "allowed"
    readonly property bool accessUnknown: accessStatus === "unknown" || accessStatus === "unspecified"
    readonly property bool emptyAllowed: accessAllowed && notificationCenterModel.count === 0
    readonly property var overlayRegions: visible ? [Qt.rect(Math.round(x), Math.round(y), Math.round(width), Math.round(height))] : []
    readonly property color panelColor: transparentMode ? "transparent" : "#FFFFFFF0"
    readonly property color panelBorder: transparentMode ? "transparent" : "#FFFFFFB8"
    readonly property color cardColor: transparentMode ? Qt.rgba(0, 0, 0, 0.42) : "#FFFFFFB8"
    readonly property color textColor: transparentMode ? "#F5F5F7" : "#1D1D1F"
    readonly property color mutedColor: transparentMode ? "#D1D1D6" : "#6E6E73"
    readonly property color iconCircleColor: transparentMode ? "#FFFFFF18" : "#00000010"

    signal overlayRegionRectsChanged(var regions)

    function openPanel(window) {
        root.hostWindow = window || root.parent || null
        closeFinishTimer.stop()
        root.closing = false
        if (root.accessUnknown) {
            notificationCenterModel.requestAccess()
        } else {
            notificationCenterModel.refresh()
        }
        root.opened = true
        root.forceActiveFocus()
        root.overlayRegionRectsChanged(root.overlayRegions)
    }

    function closePanel() {
        if (!root.opened && !root.closing)
            return
        root.opened = false
        root.closing = true
        closeFinishTimer.restart()
        root.overlayRegionRectsChanged(root.overlayRegions)
    }

    function togglePanel(window) {
        if (root.opened) {
            root.closePanel()
        } else {
            root.openPanel(window)
        }
    }

    Keys.onEscapePressed: event => {
        root.closePanel()
        event.accepted = true
    }

    onXChanged: overlayRegionRectsChanged(overlayRegions)
    onYChanged: overlayRegionRectsChanged(overlayRegions)
    onWidthChanged: overlayRegionRectsChanged(overlayRegions)
    onHeightChanged: overlayRegionRectsChanged(overlayRegions)
    onVisibleChanged: overlayRegionRectsChanged(overlayRegions)

    Behavior on x {
        NumberAnimation {
            duration: root.opened ? 260 : 190
            easing.type: root.opened ? Easing.OutCubic : Easing.InCubic
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: root.opened ? 220 : 150
            easing.type: root.opened ? Easing.OutCubic : Easing.InCubic
        }
    }

    Timer {
        id: closeFinishTimer
        interval: 210
        repeat: false
        onTriggered: {
            root.closing = false
            root.overlayRegionRectsChanged(root.overlayRegions)
        }
    }

    MouseArea {
        anchors.fill: parent
        z: -1
        acceptedButtons: Qt.AllButtons
    }

    Rectangle {
        id: panelChrome
        anchors.fill: parent
        visible: !root.transparentMode
        radius: 24
        color: root.panelColor
        border.width: 1
        border.color: root.panelBorder

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#5A000000"
            shadowBlur: 1.0
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 14
            autoPaddingEnabled: true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 74
            Layout.leftMargin: 20
            Layout.rightMargin: 16
            spacing: 12
            visible: !root.emptyAllowed

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    Layout.fillWidth: true
                    text: Qt.locale().toString(new Date(), "dddd")
                    color: root.textColor
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: Qt.locale().toString(new Date(), "d MMMM")
                    color: root.mutedColor
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                radius: 15
                color: root.iconCircleColor

                Text {
                    anchors.centerIn: parent
                    text: notificationCenterModel.busy ? "..." : notificationCenterModel.count
                    color: root.textColor
                    font.pixelSize: notificationCenterModel.busy ? 12 : 13
                    font.weight: Font.DemiBold
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.bottomMargin: 12

            ListView {
                id: notificationList
                anchors.fill: parent
                visible: root.accessAllowed && count > 0
                model: notificationCenterModel
                spacing: 10
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    id: card
                    required property int notificationId
                    required property string appName
                    required property string title
                    required property string body
                    required property string timeText
                    required property string iconUrl

                    width: ListView.view.width
                    implicitHeight: Math.max(78, cardContent.implicitHeight + 22)
                    radius: 18
                    color: root.cardColor
                    border.width: root.transparentMode ? 0 : 1
                    border.color: root.darkTheme ? "#FFFFFF12" : "#FFFFFFCC"

                    RowLayout {
                        id: cardContent
                        anchors.fill: parent
                        anchors.margins: 11
                        spacing: 11

                        Rectangle {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                            Layout.alignment: Qt.AlignTop
                            radius: 9
                            color: root.iconCircleColor

                            Image {
                                anchors.centerIn: parent
                                width: 25
                                height: 25
                                visible: card.iconUrl.length > 0
                                source: card.iconUrl
                                sourceSize: Qt.size(50, 50)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: card.iconUrl.length === 0
                                text: card.appName.length > 0 ? card.appName.charAt(0).toUpperCase() : "N"
                                color: root.textColor
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: card.appName
                                    color: root.mutedColor
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: card.timeText
                                    color: root.mutedColor
                                    font.pixelSize: 11
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: card.title
                                color: root.textColor
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: card.body
                                color: root.textColor
                                opacity: 0.82
                                font.pixelSize: 12
                                lineHeight: 1.12
                                wrapMode: Text.WordWrap
                                maximumLineCount: 4
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            Layout.alignment: Qt.AlignTop
                            radius: 12
                            color: closeMouse.containsMouse ? (root.darkTheme ? "#FFFFFF24" : "#00000012") : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: "x"
                                color: root.mutedColor
                                font.pixelSize: 13
                            }

                            MouseArea {
                                id: closeMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: notificationCenterModel.dismiss(card.notificationId)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 34
                spacing: 12
                visible: !notificationList.visible && !root.emptyAllowed

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 54
                    Layout.preferredHeight: 54
                    radius: 18
                    color: root.iconCircleColor

                    Canvas {
                        anchors.centerIn: parent
                        width: 27
                        height: 27
                        property color glyph: root.textColor
                        onGlyphChanged: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            ctx.strokeStyle = glyph
                            ctx.lineWidth = 1.6
                            ctx.lineCap = "round"
                            ctx.beginPath()
                            ctx.arc(13.5, 12, 7, Math.PI, 0)
                            ctx.lineTo(21, 19)
                            ctx.lineTo(6, 19)
                            ctx.closePath()
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(10, 22)
                            ctx.quadraticCurveTo(13.5, 25, 17, 22)
                            ctx.stroke()
                        }
                        Component.onCompleted: requestPaint()
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: root.accessAllowed ? qsTr("No Notifications") : qsTr("Notification Access")
                    color: root.textColor
                    font.pixelSize: 19
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    Layout.fillWidth: true
                    text: root.accessAllowed
                        ? qsTr("New Windows notifications will appear here.")
                        : qsTr("Allow Megushell to read Windows notifications to show them in this panel.")
                    color: root.mutedColor
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8
                    visible: !root.accessAllowed

                    Button {
                        text: root.accessUnknown ? qsTr("Allow") : qsTr("Retry")
                        onClicked: notificationCenterModel.requestAccess()
                    }

                    Button {
                        text: qsTr("Settings")
                        onClicked: notificationCenterModel.openWindowsNotificationSettings()
                    }
                }
            }

            Text {
                id: emptyRecentText
                anchors.horizontalCenter: parent.horizontalCenter
                y: 50
                width: parent.width - 34
                visible: root.emptyAllowed
                text: qsTr("No recent notifications")
                color: root.textColor
                font.pixelSize: 14
                font.family: "SF Pro Text"
                font.bold: true
                font.weight: Font.Black
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                anchors.left: emptyRecentText.left
                anchors.top: emptyRecentText.top
                width: emptyRecentText.width
                visible: emptyRecentText.visible
                text: emptyRecentText.text
                color: emptyRecentText.color
                opacity: 0.72
                font.pixelSize: emptyRecentText.font.pixelSize
                font.family: emptyRecentText.font.family
                font.bold: true
                font.weight: Font.Black
                horizontalAlignment: Text.AlignHCenter
                x: emptyRecentText.x + 0.45
            }
        }
    }
}
