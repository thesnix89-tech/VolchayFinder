import QtQuick
import QtQuick.Controls

// macOS-style overlay scrollbar: thin capsule thumb, subtle track, fades in on use.
ScrollBar {
    id: control

    readonly property bool verticalBar: control.orientation === Qt.Vertical
    readonly property bool engaged: control.active || control.hovered || control.pressed

    implicitWidth: verticalBar ? 8 : parent ? parent.height : 8
    implicitHeight: verticalBar ? (parent ? parent.height : 8) : 8
    padding: 2
    policy: ScrollBar.AsNeeded
    interactive: true

    opacity: engaged ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    contentItem: Rectangle {
        implicitWidth: verticalBar ? 6 : control.availableWidth
        implicitHeight: verticalBar ? control.availableHeight : 6
        radius: verticalBar ? width / 2 : height / 2
        color: control.pressed ? "#7B7B7B"
               : (control.hovered ? "#AEAEB2" : "#C7C7CC")

        Behavior on color {
            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    background: Rectangle {
        implicitWidth: verticalBar ? 8 : control.width
        implicitHeight: verticalBar ? control.height : 8
        radius: verticalBar ? width / 2 : height / 2
        color: "#0000000F"
        visible: control.engaged
        opacity: control.engaged ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
        }
    }
}
