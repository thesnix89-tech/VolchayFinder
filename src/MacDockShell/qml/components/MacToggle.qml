import QtQuick

Item {
    id: control

    property bool checked: false
    property bool enabled: true
    property bool darkTheme: false

    signal clicked()

    implicitWidth: 75
    implicitHeight: 30
    width: implicitWidth
    height: implicitHeight
    opacity: enabled ? 1.0 : 0.45

    readonly property color trackOff: darkTheme ? "#39393D" : "#E9E9EA"
    readonly property real stroke: 1.5
    readonly property real pad: 3
    readonly property real innerHeight: height - stroke * 2
    readonly property real knobHeight: innerHeight - pad * 2
    readonly property real knobWidth: (width - stroke * 2 - pad * 2) / 2
    readonly property real knobRadius: knobHeight / 2
    readonly property real knobY: stroke + pad
    readonly property real knobOffX: stroke + pad
    readonly property real knobOnX: width - stroke - pad - knobWidth

    Rectangle {
        id: outline
        anchors.fill: parent
        radius: height / 2
        color: "transparent"
        border.width: control.stroke
        border.color: control.checked
            ? "#003E8F"
            : (control.darkTheme ? "#5A5A5E" : "#AEAEB2")

        Behavior on border.color {
            ColorAnimation { duration: 220; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: fill
        anchors.fill: parent
        anchors.margins: control.stroke
        radius: control.innerHeight / 2
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
