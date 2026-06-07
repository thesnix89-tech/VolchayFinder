import QtQuick

Rectangle {
    id: root
    property color fillColor: "#F5F7FA10"
    property color strokeColor: "#FFFFFF22"
    property real cornerRadius: 22

    radius: cornerRadius
    color: fillColor
    border.width: 1
    border.color: strokeColor

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: Math.max(0, root.radius - 1)
        color: "transparent"
        border.width: 1
        border.color: "#FFFFFF08"
    }
}
