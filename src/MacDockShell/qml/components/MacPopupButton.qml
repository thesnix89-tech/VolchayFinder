import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

// macOS-style popup button: menu opens downward from the label with crossfade.
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
    readonly property color panelBg: darkTheme ? "#3A3A3C" : "#F5F5F7"
    readonly property color panelBorder: darkTheme ? "#FFFFFF1A" : "#0000001A"
    readonly property color chevronBtnBg: darkTheme ? "#3A3A3C" : "#E8E8ED"
    readonly property color chevronBtnBorder: darkTheme ? "#5A5A5E" : "#C8C8CC"
    readonly property color chevronGlyph: darkTheme ? "#E5E5EA" : "#636366"
    readonly property color hoverFill: "#3478F6"
    readonly property color hoverText: "#FFFFFF"

    readonly property Item popupOverlay: {
        var item = control
        while (item.parent)
            item = item.parent
        return item
    }

    readonly property int panelWidth: {
        var maxLabel = 0
        for (var i = 0; i < model.length; i++)
            maxLabel = Math.max(maxLabel, rowFontMetrics.advanceWidth(textForIndex(i)))
        return Math.max(132, Math.ceil(maxLabel) + 52)
    }

    FontMetrics {
        id: rowFontMetrics
        font.pixelSize: 13
    }

    implicitWidth: row.implicitWidth
    implicitHeight: 28

    function syncIndexFromModel() {
        // no-op hook for parents that need to refresh bindings
    }

    function repositionPopup() {
        const anchor = control.mapToItem(control.popupOverlay, 0, 0)
        popup.x = anchor.x + control.width - popup.width
        popup.y = anchor.y - popup.padding
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
            id: valueLabel
            Layout.alignment: Qt.AlignVCenter
            text: control.textForIndex(control.currentIndex)
            color: control.itemText
            font.pixelSize: 12
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            maximumLineCount: 1
            opacity: popup.opened ? 0 : 1

            Behavior on opacity {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
        }

        Item {
            id: chevronBtn
            Layout.preferredWidth: 22
            Layout.preferredHeight: 22
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: rowMouse.pressed ? Qt.darker(control.chevronBtnBg, 1.08)
                      : (rowMouse.containsMouse ? Qt.lighter(control.chevronBtnBg, 1.04) : control.chevronBtnBg)
                border.width: 1
                border.color: control.chevronBtnBorder
            }

            Canvas {
                anchors.centerIn: parent
                width: 10
                height: 10
                property color glyph: control.chevronGlyph
                property bool menuOpen: popup.opened
                onGlyphChanged: requestPaint()
                onMenuOpenChanged: requestPaint()
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

                    if (menuOpen) {
                        chevron(5, 3.5, false)
                        chevron(5, 6.5, true)
                    } else {
                        chevron(5, 3.5, true)
                        chevron(5, 6.5, false)
                    }
                }
            }
        }
    }

    MouseArea {
        id: rowMouse
        anchors.fill: row
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: control.openPopup()
    }

    Popup {
        id: popup
        parent: control.popupOverlay
        popupType: Popup.Item
        modal: false
        padding: 4
        closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
        z: 1000

        readonly property int rowHeight: 26
        width: control.panelWidth
        implicitWidth: control.panelWidth
        implicitHeight: padding * 2 + control.model.length * rowHeight

        onAboutToShow: control.repositionPopup()

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 180
                easing.type: Easing.OutCubic
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 150
                easing.type: Easing.OutCubic
            }
        }

        background: Item {
            implicitWidth: control.panelWidth
            implicitHeight: popup.implicitHeight

            Rectangle {
                anchors.fill: parent
                radius: 10
                color: control.panelBg
                border.width: 1
                border.color: control.panelBorder

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: "#40000000"
                    shadowBlur: 0.85
                    shadowVerticalOffset: 4
                    shadowHorizontalOffset: 0
                    autoPaddingEnabled: true
                }
            }
        }

        contentItem: Column {
            width: control.panelWidth - popup.padding * 2
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
                        color: itemMouse.containsMouse ? control.hoverFill : "transparent"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        Text {
                            Layout.preferredWidth: 14
                            text: rowItem.selected ? "\u2713" : ""
                            color: itemMouse.containsMouse ? control.hoverText : control.itemText
                            font.pixelSize: 12
                            font.weight: Font.Bold
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: control.textForIndex(rowItem.index)
                            color: itemMouse.containsMouse ? control.hoverText : control.itemText
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }
                    }

                    MouseArea {
                        id: itemMouse
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
