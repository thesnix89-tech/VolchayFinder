import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root
    spacing: 4

    signal spotlightRequested()
    signal notificationCenterRequested()

    property bool darkTheme: false
    property bool spotlightOpen: false
    property bool notificationCenterOpen: false
    readonly property color iconColor: darkTheme ? "#F2F2F7" : "#1D1D1F"
    // Neutral gray pills — macOS menu bar extras, not warm/yellow white overlays.
    readonly property color hoverFill: darkTheme ? "#5A5A5E" : "#DADADC"
    readonly property color pressedFill: darkTheme ? "#6E6E72" : "#C8C8CC"
    readonly property color activeFill: darkTheme ? "#636366" : "#D1D1D6"
    property string clockText: ""

    readonly property var _weekdays: []
    readonly property var _months: []

    function refreshClock() {
        var d = new Date()
        var locale = Qt.locale()
        var weekday = locale.toString(d, "ddd")
        var day = d.getDate()
        var month = locale.toString(d, "MMMM")
        var time = locale.toString(d, "hh:mm")
        clockText = weekday + ", " + day + " " + month + " " + time
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
        signal entered()
        signal exited()

        property bool active: false
        property int hitW: 28
        property int hitH: 22
        property bool interactive: true
        property bool drawHighlight: true

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
            opacity: hit.drawHighlight && (hit.active || mouse.pressed || hit.pointerOver) ? 1 : 0
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
            onEntered: hit.entered()
            onExited: hit.exited()
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
                arc(2.4, Math.PI * 1.18, Math.PI * 1.82)
                arc(4.8, Math.PI * 1.2, Math.PI * 1.8)
                arc(7.2, Math.PI * 1.25, Math.PI * 1.75)
                ctx.beginPath()
                ctx.arc(8, 13, 1.15, 0, Math.PI * 2)
                ctx.fill()
            }
        }
    }

    // Spotlight
    StatusHit {
        id: spotlightHit
        hitW: 22
        hitH: 20
        active: root.spotlightOpen

        onClicked: root.spotlightRequested()

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

    // Control Center — two stacked mini toggles (off top, on bottom).
    StatusHit {
        id: controlCenterHit
        hitW: 22
        hitH: 20
        active: controlCenterPanel.visible

        Image {
            anchors.centerIn: parent
            width: 15
            height: 18
            source: root.darkTheme
                ? "qrc:/src/MacDockShell/icons/control_center_toggles_dark.png"
                : "qrc:/src/MacDockShell/icons/control_center_toggles_light.png"
            sourceSize: Qt.size(
                Math.max(1, Math.round(width * Screen.devicePixelRatio)),
                Math.max(1, Math.round(height * Screen.devicePixelRatio)))
            fillMode: Image.PreserveAspectFit
            smooth: true
            antialiasing: true
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
        active: root.notificationCenterOpen
        hitW: clockTextItem.implicitWidth + 12
        hitH: 22
        onClicked: root.notificationCenterRequested()

        Text {
            id: clockTextItem
            text: root.clockText
            color: root.iconColor
            font.pixelSize: 12
            font.weight: Font.Medium
            opacity: 0.96
        }
    }

    StatusHit {
        id: desktopStripHit
        hitW: 9
        hitH: 22
        drawHighlight: false

        onClicked: taskbarController.toggleShowDesktop()

        Rectangle {
            anchors.centerIn: parent
            width: 2
            height: desktopStripHit.pointerOver ? 17 : 14
            radius: width / 2
            color: root.darkTheme ? "#F2F2F7" : "#1D1D1F"
            opacity: desktopStripHit.pointerOver ? 0.58 : 0

            Behavior on height { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
        }
    }
}
