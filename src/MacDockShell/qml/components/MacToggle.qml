import QtQuick

// Pill toggle like the reference: off = outline + dark knob left, on = filled + light knob right.
Item {
    id: control

    property bool checked: false
    property bool enabled: true

    signal clicked()

    implicitWidth: 51
    implicitHeight: 31
    width: 51
    height: 31
    opacity: enabled ? 1.0 : 0.45

    readonly property color trackOn: "#1D1D1F"
    readonly property color trackOff: "#FFFFFF"
    readonly property real knobSize: 25
    readonly property real knobInset: 3
    readonly property real knobOffX: knobInset
    readonly property real knobOnX: width - knobSize - knobInset

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: control.checked ? control.trackOn : control.trackOff
        border.width: control.checked ? 0 : 1.5
        border.color: control.trackOn

        Behavior on color {
            ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
        Behavior on border.width {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: knob
        width: control.knobSize
        height: control.knobSize
        radius: width / 2
        x: control.checked ? control.knobOnX : control.knobOffX
        y: control.knobInset
        color: control.checked ? "#FFFFFF" : control.trackOn
        z: 1

        Behavior on x {
            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
        }
        Behavior on color {
            ColorAnimation { duration: 200; easing.type: Easing.OutCubic }
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
