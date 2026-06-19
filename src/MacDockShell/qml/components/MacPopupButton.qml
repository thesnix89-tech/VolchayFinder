import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// macOS-style popup button: current value label + circular chevron trigger,
// floating menu with checkmark on the selected row.
Item {
    id: control

    property var model: []
    property int currentIndex: 0
    property var textForIndex: function(index) {
        return index >= 0 && index < model.length ? String(model[index]) : ""
    }
    property bool darkTheme: false

    signal activated(int index)
    signal valueActivated(var value)

    readonly property color itemText: darkTheme ? "#F2F2F7" : "#1D1D1F"
    readonly property color panelBg: darkTheme ? "#323234" : "#F5F5F7"
    readonly property color panelBorder: darkTheme ? "#48484A" : "#C2C2C2"
    readonly property color chevronBtnBg: darkTheme ? "#3A3A3C" : "#E8E8ED"
    readonly property color chevronBtnBorder: darkTheme ? "#5A5A5E" : "#C8C8CC"
    readonly property color chevronGlyph: darkTheme ? "#E5E5EA" : "#636366"
    readonly property color hoverFill: "#3478F6"
    readonly property color hoverText: "#FFFFFF"

    implicitWidth: row.implicitWidth
    implicitHeight: 28

    function syncIndexFromModel() {
        // no-op hook for parents that need to refresh bindings
    }

    function openPopup() {
        if (popup.opened)
            popup.close()
        else
            popup.open()
    }

    RowLayout {
        id: row
        anchors.right: parent.right
        spacing: 8

        Text {
            Layout.alignment: Qt.AlignVCenter
            text: control.textForIndex(control.currentIndex)
            color: control.itemText
            font.pixelSize: 12
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        Item {
            id: chevronBtn
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: chevronMouse.pressed ? Qt.darker(control.chevronBtnBg, 1.08)
                      : (chevronMouse.containsMouse ? Qt.lighter(control.chevronBtnBg, 1.04) : control.chevronBtnBg)
                border.width: 1
                border.color: control.chevronBtnBorder
            }

            Canvas {
                anchors.centerIn: parent
                width: 10
                height: 10
                property color glyph: control.chevronGlyph
                onGlyphChanged: requestPaint()
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = glyph
                    ctx.lineWidth = 1.2
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"

                    function chevron(cx, cy, up) {
                        ctx.beginPath()
                        if (up) {
                            ctx.moveTo(cx - 3, cy + 1)
                            ctx.lineTo(cx, cy - 2)
                            ctx.lineTo(cx + 3, cy + 1)
                        } else {
                            ctx.moveTo(cx - 3, cy - 1)
                            ctx.lineTo(cx, cy + 2)
                            ctx.lineTo(cx + 3, cy - 1)
                        }
                        ctx.stroke()
                    }

                    chevron(5, 3.5, true)
                    chevron(5, 6.5, false)
                }
            }

            MouseArea {
                id: chevronMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: control.openPopup()
            }
        }
    }

    Popup {
        id: popup
        popupType: Popup.Item
        modal: false
        padding: 6
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
        z: 1000

        readonly property int rowHeight: 28
        readonly property int panelWidth: 220
        implicitWidth: panelWidth
        implicitHeight: padding * 2 + control.model.length * rowHeight

        x: -width + control.width
        y: control.height + 4

        background: Rectangle {
            implicitWidth: popup.panelWidth
            implicitHeight: popup.implicitHeight
            color: control.panelBg
            radius: 9
            border.width: 1
            border.color: control.panelBorder
        }

        contentItem: Column {
            width: popup.panelWidth - popup.padding * 2
            spacing: 0

            Repeater {
                model: control.model

                delegate: Item {
                    id: rowItem
                    required property var modelData
                    required property int index

                    width: parent.width
                    height: popup.rowHeight

                    readonly property bool selected: index === control.currentIndex

                    Rectangle {
                        anchors.fill: parent
                        anchors.leftMargin: 4
                        anchors.rightMargin: 4
                        radius: 5
                        color: rowMouse.containsMouse ? control.hoverFill : "transparent"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Text {
                            Layout.preferredWidth: 14
                            text: rowItem.selected ? "\u2713" : ""
                            color: rowMouse.containsMouse ? control.hoverText : control.itemText
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: control.textForIndex(rowItem.index)
                            color: rowMouse.containsMouse ? control.hoverText : control.itemText
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        preventStealing: true
                        onClicked: {
                            control.currentIndex = rowItem.index
                            control.activated(rowItem.index)
                            control.valueActivated(rowItem.modelData)
                            popup.close()
                        }
                    }
                }
            }
        }
    }
}
