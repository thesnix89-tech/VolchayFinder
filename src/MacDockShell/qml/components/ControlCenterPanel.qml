import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    popupType: Popup.Window
    modal: false
    focus: true
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
    padding: 12
    width: 328
    implicitHeight: contentColumn.implicitHeight + 24

    property bool darkTheme: false

    readonly property color panelBg: darkTheme ? "#2E2E30F0" : "#ECECEEF2"
    readonly property color tileBg: darkTheme ? "#FFFFFF1A" : "#FFFFFFCC"
    readonly property color tileBorder: darkTheme ? "#FFFFFF22" : "#FFFFFFE6"
    readonly property color labelColor: darkTheme ? "#F2F2F7" : "#1D1D1F"
    readonly property color sublabelColor: darkTheme ? "#AEAEB2" : "#6E6E73"
    readonly property color sliderTrack: darkTheme ? "#FFFFFF28" : "#00000018"
    readonly property color sliderFill: darkTheme ? "#FFFFFF" : "#FFFFFF"

    background: Rectangle {
        radius: 14
        color: root.panelBg
        border.width: 1
        border.color: darkTheme ? "#48484A" : "#D1D1D6"
    }

    contentItem: ColumnLayout {
        id: contentColumn
        spacing: 10
        width: parent.width

        // ── Row 1: connectivity + focus + mirror ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            // Wi-Fi / Bluetooth / AirDrop
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 132
                radius: 12
                color: root.tileBg
                border.width: 1
                border.color: root.tileBorder

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 0

                    Repeater {
                        model: [
                            { icon: "wifi", title: "Wi-Fi", subtitle: "Выкл." },
                            { icon: "bt", title: "Bluetooth", subtitle: "Выкл." },
                            { icon: "airdrop", title: "AirDrop", subtitle: "Получение: Выкл." }
                        ]
                        delegate: Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38

                            RowLayout {
                                anchors.fill: parent
                                spacing: 10

                                Rectangle {
                                    width: 28
                                    height: 28
                                    radius: 14
                                    color: root.darkTheme ? "#FFFFFF20" : "#00000010"
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.icon === "wifi" ? "󰖩" : (modelData.icon === "bt" ? "󰂯" : "󰀒")
                                        font.pixelSize: 14
                                        color: root.labelColor
                                        visible: false
                                    }
                                    Canvas {
                                        anchors.centerIn: parent
                                        width: 16
                                        height: 16
                                        onPaint: {
                                            var ctx = getContext("2d")
                                            ctx.reset()
                                            ctx.strokeStyle = root.labelColor
                                            ctx.fillStyle = root.labelColor
                                            ctx.lineWidth = 1.1
                                            if (modelData.icon === "wifi") {
                                                ctx.lineCap = "round"
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
                                            } else if (modelData.icon === "bt") {
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
                                            } else {
                                                ctx.beginPath()
                                                ctx.arc(8, 6, 3, Math.PI, 0)
                                                ctx.lineTo(11, 13)
                                                ctx.lineTo(5, 13)
                                                ctx.closePath()
                                                ctx.stroke()
                                            }
                                        }
                                        Component.onCompleted: requestPaint()
                                        Connections {
                                            target: root
                                            function onDarkThemeChanged() { parent.requestPaint() }
                                        }
                                    }
                                }

                                ColumnLayout {
                                    spacing: 0
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
                                width: parent.width
                                height: 1
                                color: root.darkTheme ? "#FFFFFF14" : "#0000000E"
                                visible: index < 2
                                anchors.bottom: parent.bottom
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                spacing: 10
                Layout.preferredWidth: 96

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 61
                    radius: 12
                    color: root.tileBg
                    border.width: 1
                    border.color: root.tileBorder

                    Column {
                        anchors.centerIn: parent
                        spacing: 6
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "☾"
                            font.pixelSize: 18
                            color: root.labelColor
                        }
                        Text {
                            text: "Фокусирование"
                            color: root.labelColor
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            width: 88
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 61
                    radius: 12
                    color: root.tileBg
                    border.width: 1
                    border.color: root.tileBorder

                    Column {
                        anchors.centerIn: parent
                        spacing: 6
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
                            text: "Повтор экрана"
                            color: root.labelColor
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            width: 88
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }

        // ── Display brightness ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            radius: 12
            color: root.tileBg
            border.width: 1
            border.color: root.tileBorder

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Text {
                    text: "☀"
                    font.pixelSize: 14
                    color: root.labelColor
                }

                Slider {
                    id: brightnessSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: 72
                    property bool _pressed: pressed

                    background: Item {
                        x: brightnessSlider.leftPadding
                        y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - 3
                        width: brightnessSlider.availableWidth
                        height: 6
                        Rectangle {
                            anchors.fill: parent
                            radius: 3
                            color: root.sliderTrack
                        }
                        Rectangle {
                            width: brightnessSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 3
                            color: root.sliderFill
                            opacity: root.darkTheme ? 0.9 : 1
                        }
                    }
                    handle: Rectangle {
                        x: brightnessSlider.leftPadding + brightnessSlider.visualPosition
                                * (brightnessSlider.availableWidth - width)
                        y: brightnessSlider.topPadding + brightnessSlider.availableHeight / 2 - height / 2
                        width: 18
                        height: 18
                        radius: 9
                        color: "#FFFFFF"
                        border.width: 1
                        border.color: "#00000018"
                    }
                }
            }

            Text {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 10
                text: "Дисплей"
                color: root.sublabelColor
                font.pixelSize: 10
                visible: false
            }
        }

        // ── Sound volume ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            radius: 12
            color: root.tileBg
            border.width: 1
            border.color: root.tileBorder

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                Text {
                    text: "🎧"
                    font.pixelSize: 13
                }

                Slider {
                    id: volumeSlider
                    Layout.fillWidth: true
                    from: 0
                    to: 100
                    value: 45

                    background: Item {
                        x: volumeSlider.leftPadding
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - 3
                        width: volumeSlider.availableWidth
                        height: 6
                        Rectangle {
                            anchors.fill: parent
                            radius: 3
                            color: root.sliderTrack
                        }
                        Rectangle {
                            width: volumeSlider.visualPosition * parent.width
                            height: parent.height
                            radius: 3
                            color: root.sliderFill
                            opacity: root.darkTheme ? 0.9 : 1
                        }
                    }
                    handle: Rectangle {
                        x: volumeSlider.leftPadding + volumeSlider.visualPosition
                                * (volumeSlider.availableWidth - width)
                        y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                        width: 18
                        height: 18
                        radius: 9
                        color: "#FFFFFF"
                        border.width: 1
                        border.color: "#00000018"
                    }
                }

                Rectangle {
                    width: 28
                    height: 28
                    radius: 14
                    color: root.darkTheme ? "#FFFFFF20" : "#0000000C"
                    Text {
                        anchors.centerIn: parent
                        text: "▷"
                        font.pixelSize: 12
                        color: root.labelColor
                    }
                }
            }
        }

        // ── Now Playing ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            radius: 12
            color: root.tileBg
            border.width: 1
            border.color: root.tileBorder

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
                    spacing: 14
                    Text {
                        text: "⏵"
                        font.pixelSize: 16
                        color: root.labelColor
                    }
                    Text {
                        text: "⏭"
                        font.pixelSize: 14
                        color: root.labelColor
                    }
                }
            }
        }
    }
}
