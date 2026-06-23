pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Popup {
    id: root
    popupType: Popup.Window
    modal: false
    focus: true
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
    padding: 0
    width: Math.max(360, Math.min(640, Math.max(540, Math.round(hostWidth * 0.32)), hostWidth - 48))
    implicitHeight: panel.implicitHeight

    property bool darkTheme: false
    property var hostWindow: null

    readonly property int hostWidth: hostWindow ? hostWindow.width : Screen.width
    readonly property int hostHeight: hostWindow ? hostWindow.height : Screen.height

    readonly property color panelColor: darkTheme ? "#2C2C2EE8" : "#EEF0F8E8"
    readonly property color panelBorder: darkTheme ? "#FFFFFF1F" : "#FFFFFFB8"
    readonly property color textColor: darkTheme ? "#F2F2F7" : "#1D1D1F"
    readonly property color subtextColor: darkTheme ? "#AEAEB2" : "#6E6E73"
    readonly property color highlightColor: darkTheme ? "#FFFFFF1C" : "#4F7DFF24"
    readonly property color fieldColor: darkTheme ? "#FFFFFF14" : "#FFFFFF98"
    readonly property color iconCircleColor: darkTheme ? "#FFFFFF18" : "#0000000B"

    function openCentered(window) {
        root.hostWindow = window || null
        root.x = Math.max(12, Math.round((root.hostWidth - root.width) / 2))
        root.y = Math.max(46, Math.round(root.hostHeight * 0.30))
        searchField.text = ""
        spotlightModel.setQuery("")
        root.open()
        Qt.callLater(function() {
            searchField.forceActiveFocus()
        })
    }

    function toggleCentered(window) {
        if (root.opened) {
            root.close()
        } else {
            root.openCentered(window)
        }
    }

    function activateCurrent() {
        if (resultsList.count <= 0)
            return
        spotlightModel.activate(Math.max(0, resultsList.currentIndex))
        root.close()
    }

    onClosed: {
        searchField.text = ""
        spotlightModel.setQuery("")
    }

    background: Item {
        implicitWidth: root.width
        implicitHeight: root.implicitHeight

        Rectangle {
            anchors.fill: parent
            radius: 19
            color: root.panelColor
            border.width: 1
            border.color: root.panelBorder

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: "#52000000"
                shadowBlur: 1.0
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 12
                autoPaddingEnabled: true
            }
        }
    }

    contentItem: ColumnLayout {
        id: panel
        spacing: 0
        width: root.width

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            radius: 19
            color: "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 10

                Canvas {
                    Layout.preferredWidth: 18
                    Layout.preferredHeight: 18
                    property color glyph: root.subtextColor
                    onGlyphChanged: requestPaint()
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = glyph
                        ctx.lineWidth = 1.45
                        ctx.lineCap = "round"
                        ctx.beginPath()
                        ctx.arc(7.6, 7.6, 5.2, 0, Math.PI * 2)
                        ctx.stroke()
                        ctx.beginPath()
                        ctx.moveTo(11.3, 11.3)
                        ctx.lineTo(15.1, 15.1)
                        ctx.stroke()
                    }
                    Component.onCompleted: requestPaint()
                }

                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Spotlight Search")
                    color: root.textColor
                    placeholderTextColor: root.subtextColor
                    selectionColor: root.darkTheme ? "#0A84FF" : "#007AFF"
                    selectedTextColor: "#FFFFFF"
                    font.pixelSize: 16
                    padding: 0
                    leftPadding: 0
                    rightPadding: 0
                    background: Item {}

                    onTextChanged: queryDebounce.restart()

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Escape) {
                            root.close()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            root.activateCurrent()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Down) {
                            if (resultsList.count > 0)
                                resultsList.currentIndex = Math.min(resultsList.count - 1, resultsList.currentIndex + 1)
                            event.accepted = true
                        } else if (event.key === Qt.Key_Up) {
                            if (resultsList.count > 0)
                                resultsList.currentIndex = Math.max(0, resultsList.currentIndex - 1)
                            event.accepted = true
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: resultsList.count > 0 ? Math.min(350, Math.max(52, resultsList.count * 52) + 12) : 0
            visible: height > 0
            clip: true

            ListView {
                id: resultsList
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                anchors.bottomMargin: 8
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                currentIndex: count > 0 ? 0 : -1
                model: spotlightModel

                onCountChanged: {
                    if (count <= 0)
                        currentIndex = -1
                    else if (currentIndex < 0 || currentIndex >= count)
                        currentIndex = 0
                }

                delegate: Item {
                    id: resultRow
                    required property int index
                    required property string title
                    required property string subtitle
                    required property string iconUrl
                    required property string kind

                    width: ListView.view.width
                    height: 52

                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: 0
                        anchors.rightMargin: 0
                        radius: 10
                        color: resultRow.ListView.isCurrentItem ? root.highlightColor : "transparent"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 12

                        Rectangle {
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            radius: 9
                            color: root.iconCircleColor

                            Image {
                                anchors.centerIn: parent
                                width: 23
                                height: 23
                                visible: resultRow.iconUrl !== ""
                                source: resultRow.iconUrl
                                sourceSize: Qt.size(40, 40)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: resultRow.iconUrl === ""
                                text: resultRow.kind === "windows-search" ? "S" : resultRow.title.charAt(0).toUpperCase()
                                color: root.textColor
                                font.pixelSize: resultRow.kind === "windows-search" ? 16 : 13
                                font.weight: Font.DemiBold
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Text {
                                Layout.fillWidth: true
                                text: resultRow.title
                                color: root.textColor
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }

                            Text {
                                Layout.fillWidth: true
                                text: resultRow.subtitle
                                color: root.subtextColor
                                font.pixelSize: 12
                                elide: Text.ElideMiddle
                                visible: text.length > 0
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: resultsList.currentIndex = resultRow.index
                        onClicked: {
                            resultsList.currentIndex = resultRow.index
                            root.activateCurrent()
                        }
                    }
                }
            }
        }
    }

    Timer {
        id: queryDebounce
        interval: 90
        repeat: false
        onTriggered: spotlightModel.setQuery(searchField.text)
    }
}
