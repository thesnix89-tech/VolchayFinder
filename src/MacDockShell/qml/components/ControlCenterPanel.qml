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
    clip: true
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
    padding: 10
    width: 320
    implicitHeight: contentColumn.implicitHeight + 20

    property bool darkTheme: false
    property bool wifiOn: false
    property bool bluetoothOn: false
    property bool airDropOn: false
    property bool focusOn: false
    property real displayValue: 0.82
    property real soundValue: 0.62

    readonly property color panelBg: darkTheme ? "#3A3A3CF2" : "#E5E5E7F2"
    readonly property color tileBg: darkTheme ? "#6D6D70CC" : "#777779CC"
    readonly property color tileBgHover: darkTheme ? "#7A7A7ECC" : "#838385CC"
    readonly property color tileBgActive: darkTheme ? "#5F5F63D9" : "#6F6F71D9"
    readonly property color labelColor: "#FFFFFF"
    readonly property color mutedColor: "#ECECF0"
    readonly property color darkText: darkTheme ? "#F5F5F7" : "#1D1D1F"
    readonly property color subtleText: darkTheme ? "#D1D1D6" : "#6E6E73"
    readonly property color bubbleOff: darkTheme ? "#FFFFFF2A" : "#FFFFFFD9"
    readonly property color bubbleOn: "#0A84FF"
    readonly property color sliderTrack: darkTheme ? "#FFFFFF2A" : "#00000026"
    readonly property color sliderFill: "#F5F5F7"

    background: Item {
        implicitWidth: root.width
        implicitHeight: root.implicitHeight

        Rectangle {
            anchors.fill: parent
            radius: 30
            color: root.panelBg
            border.width: 1
            border.color: root.darkTheme ? "#FFFFFF24" : "#FFFFFFD8"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: "#4A000000"
                shadowBlur: 1.0
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 12
                autoPaddingEnabled: true
            }
        }
    }

    component CcGlyph : Canvas {
        id: glyph
        property string kind: "wifi"
        property color glyphColor: "#FFFFFF"

        width: 20
        height: 20
        onKindChanged: requestPaint()
        onGlyphColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = glyphColor
            ctx.fillStyle = glyphColor
            ctx.lineWidth = 1.45
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (kind === "wifi") {
                function arc(r, a, b) {
                    ctx.beginPath()
                    ctx.arc(10, 15, r, a, b)
                    ctx.stroke()
                }
                arc(3, Math.PI * 1.18, Math.PI * 1.82)
                arc(6, Math.PI * 1.22, Math.PI * 1.78)
                arc(9, Math.PI * 1.27, Math.PI * 1.73)
                ctx.beginPath()
                ctx.arc(10, 15, 1.35, 0, Math.PI * 2)
                ctx.fill()
            } else if (kind === "bluetooth") {
                ctx.beginPath()
                ctx.moveTo(10, 2)
                ctx.lineTo(10, 18)
                ctx.lineTo(15, 13)
                ctx.lineTo(6, 7)
                ctx.lineTo(10, 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(6, 13)
                ctx.lineTo(15, 7)
                ctx.stroke()
            } else if (kind === "airdrop") {
                ctx.beginPath()
                ctx.arc(10, 10, 7, Math.PI * 1.1, Math.PI * 1.9)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(10, 10, 4.3, Math.PI * 1.1, Math.PI * 1.9)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(10, 13, 1.5, 0, Math.PI * 2)
                ctx.fill()
            } else if (kind === "focus") {
                ctx.beginPath()
                ctx.arc(10, 10, 7, Math.PI * 0.18, Math.PI * 1.82)
                ctx.arc(13, 8, 7, Math.PI * 1.75, Math.PI * 0.25, true)
                ctx.closePath()
                ctx.fill()
            } else if (kind === "mirror") {
                ctx.strokeRect(2.5, 5, 11, 8)
                ctx.strokeRect(7, 8, 11, 8)
            } else if (kind === "keyboard") {
                ctx.strokeRect(2.5, 5, 15, 10)
                for (var k = 0; k < 4; ++k) {
                    ctx.beginPath()
                    ctx.moveTo(5 + k * 3, 8)
                    ctx.lineTo(6 + k * 3, 8)
                    ctx.stroke()
                }
                ctx.beginPath()
                ctx.moveTo(6, 12)
                ctx.lineTo(14, 12)
                ctx.stroke()
            } else if (kind === "camera") {
                ctx.strokeRect(4, 7, 12, 8)
                ctx.beginPath()
                ctx.arc(10, 11, 2.4, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(7, 7)
                ctx.lineTo(8, 5)
                ctx.lineTo(12, 5)
                ctx.lineTo(13, 7)
                ctx.stroke()
            } else if (kind === "sun") {
                ctx.beginPath()
                ctx.arc(10, 10, 3.3, 0, Math.PI * 2)
                ctx.stroke()
                for (var i = 0; i < 8; ++i) {
                    var a = i * Math.PI / 4
                    ctx.beginPath()
                    ctx.moveTo(10 + Math.cos(a) * 5.5, 10 + Math.sin(a) * 5.5)
                    ctx.lineTo(10 + Math.cos(a) * 8.0, 10 + Math.sin(a) * 8.0)
                    ctx.stroke()
                }
            } else if (kind === "speaker") {
                ctx.beginPath()
                ctx.moveTo(3, 12)
                ctx.lineTo(6, 12)
                ctx.lineTo(10, 16)
                ctx.lineTo(10, 4)
                ctx.lineTo(6, 8)
                ctx.lineTo(3, 8)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.arc(11, 10, 4, Math.PI * 1.75, Math.PI * 0.25)
                ctx.stroke()
            } else if (kind === "airplay") {
                ctx.strokeRect(3, 4, 14, 10)
                ctx.beginPath()
                ctx.moveTo(10, 11)
                ctx.lineTo(6, 17)
                ctx.lineTo(14, 17)
                ctx.closePath()
                ctx.fill()
            } else if (kind === "music") {
                ctx.beginPath()
                ctx.moveTo(12, 4)
                ctx.lineTo(12, 14)
                ctx.arc(8, 14, 3, 0, Math.PI * 2)
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(12, 4)
                ctx.lineTo(16, 5.5)
                ctx.lineTo(16, 8)
                ctx.lineTo(12, 6.5)
                ctx.closePath()
                ctx.fill()
            } else if (kind === "play") {
                ctx.beginPath()
                ctx.moveTo(7, 4)
                ctx.lineTo(16, 10)
                ctx.lineTo(7, 16)
                ctx.closePath()
                ctx.fill()
            } else if (kind === "prev") {
                ctx.beginPath()
                ctx.moveTo(5, 10)
                ctx.lineTo(12, 5)
                ctx.lineTo(12, 15)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(12, 10)
                ctx.lineTo(18, 5)
                ctx.lineTo(18, 15)
                ctx.closePath()
                ctx.fill()
            } else if (kind === "next") {
                ctx.beginPath()
                ctx.moveTo(15, 10)
                ctx.lineTo(8, 5)
                ctx.lineTo(8, 15)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(8, 10)
                ctx.lineTo(2, 5)
                ctx.lineTo(2, 15)
                ctx.closePath()
                ctx.fill()
            }
        }
        Component.onCompleted: requestPaint()
    }

    component IconBubble : Rectangle {
        id: bubble
        property string kind: "wifi"
        property bool checked: false
        property int bubbleSize: 36

        width: bubbleSize
        height: bubbleSize
        radius: bubbleSize / 2
        color: checked ? root.bubbleOn : root.bubbleOff

        CcGlyph {
            anchors.centerIn: parent
            kind: bubble.kind
            glyphColor: bubble.checked ? "#FFFFFF" : (root.darkTheme ? "#F5F5F7" : "#8A8A8E")
        }
    }

    component ControlPill : Rectangle {
        id: pill
        signal toggled()

        property string title: ""
        property string subtitle: ""
        property string kind: "wifi"
        property bool checked: false

        Layout.fillWidth: true
        Layout.preferredHeight: 68
        radius: 34
        color: mouse.containsMouse ? root.tileBgHover : (checked ? root.tileBgActive : root.tileBg)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 13
            anchors.rightMargin: 13
            spacing: 10

            IconBubble {
                kind: pill.kind
                checked: pill.checked
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                    Layout.fillWidth: true
                    text: pill.title
                    color: root.labelColor
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: pill.subtitle
                    color: root.mutedColor
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: pill.toggled()
        }
    }

    component RoundAction : Rectangle {
        id: action
        signal toggled()

        property string kind: "keyboard"
        property bool checked: false

        Layout.preferredWidth: 62
        Layout.preferredHeight: 62
        radius: 31
        color: mouse.containsMouse ? root.tileBgHover : (checked ? root.tileBgActive : root.tileBg)

        IconBubble {
            anchors.centerIn: parent
            kind: action.kind
            checked: action.checked
            bubbleSize: 34
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: action.toggled()
        }
    }

    component FocusPill : Rectangle {
        id: focus
        signal toggled()

        Layout.fillWidth: true
        Layout.preferredHeight: 62
        radius: 31
        color: mouse.containsMouse ? root.tileBgHover : (root.focusOn ? root.tileBgActive : root.tileBg)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 13
            anchors.rightMargin: 14
            spacing: 11

            IconBubble {
                kind: "focus"
                checked: root.focusOn
                bubbleSize: 34
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Focus")
                color: root.labelColor
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: focus.toggled()
        }
    }

    component MediaTile : Rectangle {
        id: media

        Layout.fillWidth: true
        Layout.preferredHeight: 144
        radius: 32
        color: mouse.containsMouse ? root.tileBgHover : root.tileBg

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                    radius: 14
                    color: root.darkTheme ? "#FFFFFF22" : "#FFFFFF6A"
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Not Playing")
                    color: root.labelColor
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item { Layout.fillWidth: true }

                CcGlyph {
                    kind: "prev"
                    glyphColor: root.mutedColor
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                }
                CcGlyph {
                    kind: "play"
                    glyphColor: root.labelColor
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                }
                CcGlyph {
                    kind: "next"
                    glyphColor: root.mutedColor
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                }
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }
    }

    component SliderBar : Item {
        id: slider
        signal moved(real value)

        property real value: 0.5
        property string kind: "sun"

        implicitHeight: 22

        Rectangle {
            id: track
            anchors.fill: parent
            radius: height / 2
            color: root.sliderTrack

            Rectangle {
                id: fill
                height: parent.height
                width: Math.max(track.height, slider.value * track.width)
                radius: height / 2
                color: root.sliderFill
            }

            CcGlyph {
                anchors.left: parent.left
                anchors.leftMargin: 7
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                kind: slider.kind
                glyphColor: root.darkText
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onPressed: function(mouse) { setFromX(mouse.x) }
            onPositionChanged: function(mouse) {
                if (pressed)
                    setFromX(mouse.x)
            }

            function setFromX(mouseX) {
                slider.value = Math.max(0, Math.min(1, mouseX / Math.max(1, track.width)))
                slider.moved(slider.value)
            }
        }
    }

    component SliderTile : Rectangle {
        id: tile
        property string title: ""
        property string kind: "sun"
        property real value: 0.5
        signal moved(real value)

        Layout.fillWidth: true
        Layout.preferredHeight: 76
        radius: 24
        color: root.darkTheme ? "#FFFFFF1C" : "#FFFFFFB8"

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 15
            anchors.rightMargin: 15
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            spacing: 9

            Text {
                Layout.fillWidth: true
                text: tile.title
                color: root.darkText
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            SliderBar {
                Layout.fillWidth: true
                kind: tile.kind
                value: tile.value
                onMoved: function(newValue) { tile.moved(newValue) }
            }
        }
    }

    component EditButton : Rectangle {
        id: edit
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: 96
        Layout.preferredHeight: 30
        radius: 15
        color: mouse.containsMouse ? (root.darkTheme ? "#FFFFFF30" : "#00000024")
                                  : (root.darkTheme ? "#FFFFFF22" : "#00000018")

        Text {
            anchors.centerIn: parent
            text: qsTr("Edit Controls")
            color: root.darkText
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
        }
    }

    contentItem: ColumnLayout {
        id: contentColumn
        width: root.width - root.padding * 2
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.preferredWidth: 142
                Layout.fillHeight: true
                spacing: 8

                ControlPill {
                    title: qsTr("Wi-Fi")
                    subtitle: root.wifiOn ? qsTr("Connected") : qsTr("Not Connected")
                    kind: "wifi"
                    checked: root.wifiOn
                    onToggled: root.wifiOn = !root.wifiOn
                }

                ControlPill {
                    title: qsTr("Bluetooth")
                    subtitle: root.bluetoothOn ? qsTr("On") : qsTr("Off")
                    kind: "bluetooth"
                    checked: root.bluetoothOn
                    onToggled: root.bluetoothOn = !root.bluetoothOn
                }

                ControlPill {
                    title: qsTr("AirDrop")
                    subtitle: root.airDropOn ? qsTr("Everyone") : qsTr("Off")
                    kind: "airdrop"
                    checked: root.airDropOn
                    onToggled: root.airDropOn = !root.airDropOn
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                MediaTile {}

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    RoundAction {
                        kind: "keyboard"
                    }

                    RoundAction {
                        kind: "mirror"
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            RoundAction {
                kind: "camera"
            }

            FocusPill {
                onToggled: root.focusOn = !root.focusOn
            }
        }

        SliderTile {
            title: qsTr("Display")
            kind: "sun"
            value: root.displayValue
            onMoved: function(newValue) { root.displayValue = newValue }
        }

        SliderTile {
            title: qsTr("Sound")
            kind: "speaker"
            value: root.soundValue
            onMoved: function(newValue) { root.soundValue = newValue }
        }

        EditButton {}
    }
}
