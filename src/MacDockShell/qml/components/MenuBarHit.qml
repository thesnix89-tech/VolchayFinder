import QtQuick

Item {
    id: hit

    signal clicked()

    property bool darkTheme: false
    property bool active: false
    property bool interactive: true
    property int hitW: 0
    property int hitH: 20
    property int hPad: 8

    readonly property color hoverFill: darkTheme ? "#5A5A5E" : "#DADADC"
    readonly property color pressedFill: darkTheme ? "#6E6E72" : "#C8C8CC"
    readonly property color activeFill: darkTheme ? "#636366" : "#D1D1D6"

    default property alias content: contentSlot.data

    implicitWidth: hitW > 0 ? hitW : contentSlot.childrenRect.width + hPad * 2
    implicitHeight: hitH

    readonly property bool pointerOver: {
        var gp = hoverTracker.globalCursor
        var local = hit.mapFromGlobal(gp.x, gp.y)
        return local.x >= 0 && local.x <= hit.width
                && local.y >= 0 && local.y <= hit.height
    }

    Rectangle {
        id: highlight
        z: 0
        anchors.centerIn: parent
        width: parent.width
        height: hit.hitH
        radius: 5
        color: hit.active ? hit.activeFill
              : (mouse.pressed ? hit.pressedFill : hit.hoverFill)
        opacity: (hit.active || mouse.pressed || hit.pointerOver) ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 80 } }
    }

    Item {
        id: contentSlot
        z: 1
        anchors.centerIn: parent
        width: childrenRect.width
        height: childrenRect.height
    }

    MouseArea {
        id: mouse
        z: 2
        anchors.fill: parent
        enabled: hit.interactive
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: hit.interactive ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: hit.clicked()
    }
}
