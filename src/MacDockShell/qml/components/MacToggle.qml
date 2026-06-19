import QtQuick
import QtQuick.Effects

Item {
    id: control

    property bool checked: false
    property bool enabled: true
    property bool darkTheme: false

    signal clicked()

    implicitWidth: 51
    implicitHeight: 31
    width: 51
    height: 31
    opacity: enabled ? 1.0 : 0.45

    readonly property color trackOn: "#007AFF"
    readonly property color trackOff: darkTheme ? "#39393D" : "#E9E9EA"
    readonly property real knobSize: 27
    readonly property real knobInset: 2
    readonly property real knobOffX: knobInset
    readonly property real knobOnX: width - knobSize - knobInset

    Rectangle {
        id: track
        anchors.fill: parent
        radius: height / 2
        color: control.checked ? control.trackOn : control.trackOff

        Behavior on color {
            ColorAnimation { duration: 220; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: knob
        width: control.knobSize
        height: control.knobSize
        radius: width / 2
        x: control.checked ? control.knobOnX : control.knobOffX
        y: control.knobInset
        color: "#FFFFFF"
        z: 1

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#26000000"
            shadowBlur: 0.35
            shadowVerticalOffset: 1
            shadowHorizontalOffset: 0
            autoPaddingEnabled: false
        }

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
