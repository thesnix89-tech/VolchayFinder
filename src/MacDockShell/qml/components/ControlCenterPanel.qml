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
    padding: 10
    width: 322
    implicitHeight: contentColumn.implicitHeight + 20

    property bool darkTheme: false

    readonly property color panelBg: darkTheme ? "#2C2C2EE6" : "#E9E9EBD9"
    readonly property color tileBg: darkTheme ? "#FFFFFF1A" : "#FFFFFFA6"
    readonly property color labelColor: darkTheme ? "#F2F2F7" : "#1D1D1F"
    readonly property color sublabelColor: darkTheme ? "#AEAEB2" : "#6E6E73"
    readonly property color iconCircle: darkTheme ? "#FFFFFF22" : "#00000010"
    readonly property color dividerColor: darkTheme ? "#FFFFFF14" : "#0000000E"

    background: Item {
        implicitWidth: root.width
        implicitHeight: root.implicitHeight

        Rectangle {
            id: panelBgRect
            anchors.fill: parent
            radius: 18
            color: root.panelBg
            border.width: 1
            border.color: root.darkTheme ? "#FFFFFF18" : "#FFFFFF80"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: "#40000000"
                shadowBlur: 1.0
                shadowVerticalOffset: 10
                shadowHorizontalOffset: 0
                autoPaddingEnabled: true
            }
        }
    }

    component CcTile : Rectangle {
        id: tile
        default property alias content: contentSlot.data
        radius: 14
        color: root.tileBg
        border.width: 1
        border.color: root.darkTheme ? "#FFFFFF14" : "#FFFFFFCC"
        clip: true

        Item {
            id: contentSlot
            anchors.fill: parent
        }
    }

    component CcSlider : Item {
        id: slider
        property real value: 0.5
        property string icon: "sun"
        property bool darkTheme: root.darkTheme

        implicitHeight: 28

        Rectangle {
            id: track
            anchors.fill: parent
            radius: height / 2
            color: slider.darkTheme ? "#FFFFFF18" : "#00000010"

            Rectangle {
                id: fill
                height: parent.height
                width: Math.max(slider.value * track.width, height)
                radius: height / 2
                color: "#FFFFFF"

                Canvas {
                    id: sliderIcon
                    anchors.left: parent.left
                    anchors.leftMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14
                    height: 14
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = "#1D1D1F"
                        ctx.fillStyle = "#1D1D1F"
                        ctx.lineWidth = 1.1
                        ctx.lineCap = "round"
                        if (slider.icon === "sun") {
                            ctx.beginPath()
                            ctx.arc(7, 7, 3, 0, Math.PI * 2)
                            ctx.stroke()
                            for (var i = 0; i < 8; i++) {
                                var a = i * Math.PI / 4
                                ctx.beginPath()
                                ctx.moveTo(7 + Math.cos(a) * 4.5, 7 + Math.sin(a) * 4.5)
                                ctx.lineTo(7 + Math.cos(a) * 6, 7 + Math.sin(a) * 6)
                                ctx.stroke()
                            }
                        } else {
                            ctx.beginPath()
                            ctx.arc(4, 9, 3.5, Math.PI * 1.1, Math.PI * 1.65)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(2, 9)
                            ctx.lineTo(12, 9)
                            ctx.stroke()
                            ctx.beginPath()
                            ctx.moveTo(10, 6)
                            ctx.lineTo(13, 9)
                            ctx.lineTo(10, 12)
                            ctx.stroke()
                        }
                    }
                    Component.onCompleted: requestPaint()
                }

                Connections {
                    target: slider
                    function onIconChanged() { sliderIcon.requestPaint() }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            onPressed: function(mouse) { setValue(mouse.x) }
            onPositionChanged: function(mouse) { if (pressed) setValue(mouse.x) }
            function setValue(x) {
                slider.value = Math.max(0, Math.min(1, x / track.width))
            }
        }
    }

    component CcIconButton : Rectangle {
        id: iconBtn
        property string kind: "wifi"
        property bool off: true
        width: 30
        height: 30
        radius: 15
        color: root.iconCircle

        Canvas {
            anchors.centerIn: parent
            width: 16
            height: 16
            property color glyph: root.labelColor
            onGlyphChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.strokeStyle = glyph
                ctx.fillStyle = glyph
                ctx.lineWidth = 1.1
                ctx.lineCap = "round"
                if (iconBtn.kind === "wifi") {
                    function arc(r, a, b) {
                        ctx.beginPath()
                        ctx.arc(8, 12, r, a, b)
                        ctx.stroke()
                    }
                    arc(2, Math.PI * 1.15, Math.PI * 1.85)
                    arc(4, Math.PI * 1.2, Math.PI * 1.8)
                    arc(6, Math.PI * 1.28, Math.PI * 1.72)
                    ctx.beginPath()
                    ctx.arc(8, 12, 1, 0, Math.PI * 2)
                    ctx.fill()
                    if (iconBtn.off) {
                        ctx.beginPath()
                        ctx.moveTo(3, 5)
                        ctx.lineTo(13, 14)
                        ctx.stroke()
                    }
                } else if (iconBtn.kind === "bt") {
                    ctx.beginPath()
                    ctx.moveTo(9, 3)
                    ctx.lineTo(5, 7)
                    ctx.lineTo(9, 11)
                    ctx.lineTo(5, 15)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.moveTo(11, 5)
                    ctx.quadraticCurveTo(13, 8, 11, 11)
                    ctx.stroke()
                    if (iconBtn.off) {
                        ctx.beginPath()
                        ctx.moveTo(3, 4)
                        ctx.lineTo(13, 14)
                        ctx.stroke()
                    }
                } else {
                    ctx.beginPath()
                    ctx.arc(8, 6, 3, Math.PI, 0)
                    ctx.lineTo(11, 13)
                    ctx.lineTo(5, 13)
                    ctx.closePath()
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.arc(8, 8, 1.2, 0, Math.PI * 2)
                    ctx.stroke()
                }
            }
            Component.onCompleted: requestPaint()
            Connections {
                target: root
                function onDarkThemeChanged() { parent.requestPaint() }
            }
            Connections {
                target: iconBtn
                function onOffChanged() { parent.requestPaint() }
            }
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: 8
        width: parent.width

        // ── Top row: connectivity | focus + mirror ──
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 130

            Row {
                anchors.fill: parent
                spacing: 8

                CcTile {
                    width: parent.width * 0.615
                    height: parent.height

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 0

                        Repeater {
                            model: [
                                { kind: "wifi", title: "Wi-Fi", subtitle: "Выкл." },
                                { kind: "bt", title: "Bluetooth", subtitle: "Выкл." },
                                { kind: "airdrop", title: "AirDrop", subtitle: "Получение: Выкл." }
                            ]
                            delegate: Item {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 38

                                RowLayout {
                                    anchors.fill: parent
                                    spacing: 10

                                    CcIconButton {
                                        kind: modelData.kind
                                        off: true
                                    }

                                    ColumnLayout {
                                        spacing: 1
                                        Layout.fillWidth: true
                                        Text {
                                            text: modelData.title
                                            color: root.labelColor
                                            font.pixelSize: 13
                                            font.weight: Font.Medium
                                        }
                                        Text {
                                            text: modelData.subtitle
                                            color: root.sublabelColor
                                            font.pixelSize: 11
                                        }
                                    }
                                }

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: 1
                                    color: root.dividerColor
                                    visible: index < 2
                                }
                            }
                        }
                    }
                }

                Column {
                    width: parent.width * 0.385 - 8
                    height: parent.height
                    spacing: 8

                    CcTile {
                        width: parent.width
                        height: (parent.height - parent.spacing) / 2

                        Column {
                            anchors.centerIn: parent
                            spacing: 5
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "☾"
                                font.pixelSize: 17
                                color: root.labelColor
                            }
                            Text {
                                width: parent.width
                                text: "Фокусирование"
                                color: root.labelColor
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    CcTile {
                        width: parent.width
                        height: (parent.height - parent.spacing) / 2

                        Column {
                            anchors.centerIn: parent
                            spacing: 5
                            Canvas {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 18
                                height: 14
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.reset()
                                    ctx.strokeStyle = root.labelColor
                                    ctx.lineWidth = 1.2
                                    ctx.strokeRect(1, 2, 10, 8)
                                    ctx.strokeRect(6, 5, 10, 8)
                                }
                                Component.onCompleted: requestPaint()
                            }
                            Text {
                                width: parent.width
                                text: "Повтор экрана"
                                color: root.labelColor
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }

        // ── Display ──
        CcTile {
            Layout.fillWidth: true
            Layout.preferredHeight: 68

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text {
                    text: "Дисплей"
                    color: root.labelColor
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                CcSlider {
                    Layout.fillWidth: true
                    icon: "sun"
                    value: 0.72
                }
            }
        }

        // ── Sound ──
        CcTile {
            Layout.fillWidth: true
            Layout.preferredHeight: 68

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                Text {
                    text: "Звук"
                    color: root.labelColor
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    CcSlider {
                        Layout.fillWidth: true
                        icon: "sound"
                        value: 0.42
                    }

                    Rectangle {
                        width: 30
                        height: 30
                        radius: 15
                        color: root.iconCircle
                        Canvas {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                ctx.strokeStyle = root.labelColor
                                ctx.fillStyle = root.labelColor
                                ctx.lineWidth = 1
                                ctx.beginPath()
                                ctx.moveTo(2, 10)
                                ctx.lineTo(5, 7)
                                ctx.lineTo(8, 9)
                                ctx.lineTo(12, 4)
                                ctx.stroke()
                                ctx.beginPath()
                                ctx.moveTo(9, 4)
                                ctx.lineTo(12, 4)
                                ctx.lineTo(12, 7)
                                ctx.stroke()
                            }
                            Component.onCompleted: requestPaint()
                        }
                    }
                }
            }
        }

        // ── Music ──
        CcTile {
            Layout.fillWidth: true
            Layout.preferredHeight: 56

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Rectangle {
                    width: 36
                    height: 36
                    radius: 8
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#FC3C44" }
                        GradientStop { position: 1; color: "#FA2D55" }
                    }
                    Text {
                        anchors.centerIn: parent
                        text: "♫"
                        color: "white"
                        font.pixelSize: 16
                    }
                }

                Text {
                    text: "Музыка"
                    color: root.labelColor
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }

                Row {
                    spacing: 16
                    Canvas {
                        width: 12
                        height: 14
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            ctx.fillStyle = root.labelColor
                            ctx.beginPath()
                            ctx.moveTo(1, 1)
                            ctx.lineTo(11, 7)
                            ctx.lineTo(1, 13)
                            ctx.closePath()
                            ctx.fill()
                        }
                        Component.onCompleted: requestPaint()
                    }
                    Canvas {
                        width: 14
                        height: 12
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            ctx.fillStyle = root.labelColor
                            ctx.beginPath()
                            ctx.moveTo(0, 1)
                            ctx.lineTo(5, 1)
                            ctx.lineTo(5, 11)
                            ctx.lineTo(0, 11)
                            ctx.closePath()
                            ctx.fill()
                            ctx.beginPath()
                            ctx.moveTo(6, 1)
                            ctx.lineTo(11, 1)
                            ctx.lineTo(14, 6)
                            ctx.lineTo(11, 11)
                            ctx.lineTo(6, 11)
                            ctx.closePath()
                            ctx.fill()
                        }
                        Component.onCompleted: requestPaint()
                    }
                }
            }
        }
    }
}
