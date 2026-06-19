import QtQuick

Item {
    id: control

    property bool checked: false
    property bool enabled: true
    property bool darkTheme: false

    signal clicked()

    implicitWidth: 53
    implicitHeight: 23
    width: implicitWidth
    height: implicitHeight
    opacity: enabled ? 1.0 : 0.45

    readonly property color trackOff: darkTheme ? "#39393D" : "#E9E9EA"
    readonly property real padX: 2
    readonly property real knobWidth: 33
    readonly property real knobHeight: 21
    readonly property real padY: (height - knobHeight) / 2
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

    Rectangle {
        id: knob
        width: control.knobWidth
        height: control.knobHeight
        radius: control.knobRadius
        x: control.checked ? control.knobOnX : control.knobOffX
        y: control.knobY
        color: "#FFFFFF"
        z: 1

        Behavior on x {
            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
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
