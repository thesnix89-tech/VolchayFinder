import QtQuick
import QtQuick.Effects

Item {
    id: control

    property bool checked: false
    property bool enabled: true
    property bool darkTheme: false

    signal clicked()

    implicitWidth: 52
    implicitHeight: 22
    width: implicitWidth
    height: implicitHeight
    opacity: enabled ? 1.0 : 0.45

    readonly property color trackOff: darkTheme ? "#39393D" : "#E9E9EA"
    readonly property real padX: 2
    readonly property real knobWidth: 29
    readonly property real knobHeight: 18
    readonly property real padY: 2
    readonly property real knobRadius: knobHeight / 2
    readonly property real knobY: padY
    readonly property real knobOffX: padX
    readonly property real knobOnX: width - padX - knobWidth

    Rectangle {
        id: fill
        anchors.fill: parent
        radius: height / 2
        color: control.checked ? "#007AFF" : control.trackOff

        Behavior on color {
            ColorAnimation { duration: 220; easing.type: Easing.OutCubic }
        }
    }

    Item {
        id: knobHost
        width: control.knobWidth
        height: control.knobHeight
        x: Math.round(control.checked ? control.knobOnX : control.knobOffX)
        y: Math.round(control.knobY)
        z: 1

        Behavior on x {
            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
        }

        Rectangle {
            id: knobShape
            anchors.fill: parent
            radius: control.knobRadius
            color: "#FFFFFF"
            visible: false
        }

        Item {
            id: knobShadow
            anchors.fill: parent
            z: 0

            Repeater {
                model: [
                    { blur: 0.22, opacity: 0.10, offsetY: 3 },
                    { blur: 0.82, opacity: 0.15, offsetY: 3 }
                ]
                delegate: MultiEffect {
                    anchors.fill: parent
                    source: knobShape
                    autoPaddingEnabled: true
                    shadowEnabled: true
                    shadowColor: "#000000"
                    shadowOpacity: modelData.opacity
                    shadowBlur: modelData.blur
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: modelData.offsetY
                }
            }
        }

        Rectangle {
            id: knob
            anchors.fill: parent
            z: 1
            radius: control.knobRadius
            color: "#FFFFFF"
            layer.enabled: true
            layer.smooth: true
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: control.enabled
        hoverEnabled: true
        cursorShape: control.enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        onClicked: control.clicked()
    }
}
