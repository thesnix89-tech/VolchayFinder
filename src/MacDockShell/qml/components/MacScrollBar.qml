import QtQuick
import QtQuick.Controls

// macOS-style overlay scrollbar: thin capsule thumb, subtle track, fades in on use.
ScrollBar {
    id: control

    property bool darkTheme: false
    property color trackColor: "#0000000F"
    property bool alwaysVisible: false

    readonly property bool verticalBar: control.orientation === Qt.Vertical
    readonly property bool engaged: control.active || control.hovered || control.pressed
    readonly property bool shown: alwaysVisible || engaged
    readonly property int trackWidth: alwaysVisible ? 15 : 8
    readonly property int thumbWidth: alwaysVisible ? 11 : 6
    readonly property int barPadding: 2
    readonly property color thumbColor: {
        if (alwaysVisible)
            return darkTheme ? "#AEAEB2" : "#757575"
        return darkTheme
            ? (pressed ? "#636366" : (hovered ? "#8E8E93" : "#636366"))
            : (pressed ? "#7B7B7B" : (hovered ? "#AEAEB2" : "#C7C7CC"))
    }

    implicitWidth: verticalBar ? trackWidth : parent ? parent.height : trackWidth
    implicitHeight: verticalBar ? (parent ? parent.height : trackWidth) : trackWidth
    padding: barPadding
    policy: ScrollBar.AsNeeded
    interactive: true

    opacity: shown ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    contentItem: Rectangle {
        implicitWidth: verticalBar ? control.thumbWidth : control.availableWidth
        implicitHeight: verticalBar ? control.availableHeight : control.thumbWidth
        radius: verticalBar ? width / 2 : height / 2
        color: control.thumbColor

        Behavior on color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    background: Rectangle {
        implicitWidth: verticalBar ? control.trackWidth : control.width
        implicitHeight: verticalBar ? control.height : control.trackWidth
        radius: verticalBar ? width / 2 : height / 2
        color: control.shown ? control.trackColor : "transparent"
        visible: control.shown
        opacity: control.shown ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }
        Behavior on color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }
}
