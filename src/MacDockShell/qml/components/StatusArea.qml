import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

RowLayout {
    id: root
    spacing: 4

    property bool darkTheme: false
    readonly property color iconColor: darkTheme ? "#F2F2F7" : "#1D1D1F"
    // Neutral gray pills — macOS menu bar extras, not warm/yellow white overlays.
    readonly property color hoverFill: darkTheme ? "#5A5A5E" : "#DADADC"
    readonly property color pressedFill: darkTheme ? "#6E6E72" : "#C8C8CC"
    readonly property color activeFill: darkTheme ? "#636366" : "#D1D1D6"
    property string clockText: ""

    readonly property var _weekdays: ["Вс", "Пн", "Вт", "Ср", "Чт", "Пт", "Сб"]
    readonly property var _months: [
        "января", "февраля", "марта", "апреля", "мая", "июня",
        "июля", "августа", "сентября", "октября", "ноября", "декабря"
    ]

    function refreshClock() {
        var d = new Date()
        var hh = d.getHours()
        var mm = d.getMinutes()
        var time = (hh < 10 ? "0" : "") + hh + ":" + (mm < 10 ? "0" : "") + mm
        clockText = _weekdays[d.getDay()] + ", " + d.getDate() + " "
                + _months[d.getMonth()] + " " + time
    }

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: root.refreshClock()
    }

    Component.onCompleted: refreshClock()

    component StatusHit : Item {
        id: hit
        signal clicked()

        property bool active: false
        property int hitW: 28
        property int hitH: 22
        property bool interactive: true

        readonly property bool pointerOver: {
            var gp = hoverTracker.globalCursor
            var local = hit.mapFromGlobal(gp.x, gp.y)
            return local.x >= 0 && local.x <= hit.width
                    && local.y >= 0 && local.y <= hit.height
        }

        implicitWidth: hitW
        implicitHeight: hitH
        Layout.alignment: Qt.AlignVCenter

        default property alias content: contentSlot.data

        Rectangle {
            id: highlight
            z: 0
            anchors.centerIn: parent
            width: hit.hitW
            height: hit.hitH
            radius: 5
            color: hit.active ? root.activeFill
                  : (mouse.pressed ? root.pressedFill : root.hoverFill)
            opacity: (hit.active || mouse.pressed || hit.pointerOver) ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 80 } }
        }

        Item {
            id: contentSlot
            z: 1
            anchors.centerIn: parent
            width: childrenRect.width
            height: childrenRect.height
        }

        MouseArea {
            id: mouse
            z: 2
            anchors.fill: parent
            enabled: hit.interactive
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton
            cursorShape: hit.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: hit.clicked()
        }
    }

    // Keyboard / input source (A)
    StatusHit {
        hitW: 24
        hitH: 20
        Rectangle {
            width: 17
            height: 15
            radius: 4
            color: root.darkTheme ? "#FFFFFF14" : "#0000000A"
            border.width: 1
            border.color: root.darkTheme ? "#FFFFFF22" : "#00000018"
            Text {
                anchors.centerIn: parent
                text: "A"
                color: root.iconColor
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }
        }
    }

    // Screen mirroring
    StatusHit {
        Canvas {
            width: 16
            height: 16
            property color glyph: root.iconColor
            onGlyphChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = glyph
                ctx.lineWidth = 1.2
                ctx.strokeRect(1.5, 3, 9, 7)
                ctx.strokeRect(5.5, 6, 9, 7)
            }
        }
    }

    // Wi-Fi
    StatusHit {
        Canvas {
            width: 16
            height: 16
            property color glyph: root.iconColor
            onGlyphChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = glyph
                ctx.fillStyle = glyph
                ctx.lineWidth = 1.15
                ctx.lineCap = "round"
                function arc(r, start, end) {
                    ctx.beginPath()
                    ctx.arc(8, 13, r, start, end)
                    ctx.stroke()
                }
                arc(2.2, Math.PI * 1.15, Math.PI * 1.85)
                arc(4.6, Math.PI * 1.2, Math.PI * 1.8)
                arc(7.0, Math.PI * 1.28, Math.PI * 1.72)
                ctx.beginPath()
                ctx.arc(8, 13, 1.1, 0, Math.PI * 2)
                ctx.fill()
                ctx.font = "bold 8px sans-serif"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                ctx.fillText("!", 8, 7.5)
            }
        }
    }

    // Spotlight
    StatusHit {
        Canvas {
            width: 16
            height: 16
            property color glyph: root.iconColor
            onGlyphChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = glyph
                ctx.lineWidth = 1.2
                ctx.lineCap = "round"
                ctx.beginPath()
                ctx.arc(7, 7, 4.5, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(10.2, 10.2)
                ctx.lineTo(13.5, 13.5)
                ctx.stroke()
            }
        }
    }

    // Control Center
    StatusHit {
        id: controlCenterHit
        hitW: 30
        hitH: 22
        active: controlCenterPanel.visible

        Row {
            spacing: 3
            Repeater {
                model: 2
                Item {
                    width: 5
                    height: 14
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 3
                        height: 10
                        radius: 1.5
                        color: root.iconColor
                    }
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: index === 0 ? 1 : 5
                        width: 5
                        height: 5
                        radius: 2.5
                        color: root.iconColor
                    }
                }
            }
        }

        onClicked: {
            controlCenterPanel.darkTheme = root.darkTheme
            if (controlCenterPanel.visible) {
                controlCenterPanel.close()
            } else {
                controlCenterPanel.open(controlCenterHit,
                    -controlCenterPanel.width + controlCenterHit.width,
                    controlCenterHit.height + 6)
            }
        }
    }

    ControlCenterPanel {
        id: controlCenterPanel
        darkTheme: root.darkTheme
    }

    // Date & time
    StatusHit {
        interactive: false
        hitW: clockTextItem.implicitWidth + 12
        hitH: 22
        Text {
            id: clockTextItem
            text: root.clockText
            color: root.iconColor
            font.pixelSize: 12
            font.weight: Font.Medium
            opacity: 0.96
        }
    }
}
