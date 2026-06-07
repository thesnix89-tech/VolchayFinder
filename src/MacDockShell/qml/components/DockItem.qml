import QtQuick

Item {
    id: root
    property string symbol: "A"
    property string label: "App"

    width: hovered ? 72 : 52
    height: 72

    property bool hovered: mouseArea.containsMouse

    Behavior on width {
        NumberAnimation {
            duration: 140
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        id: iconBubble
        width: parent.width
        height: parent.width
        radius: 18
        anchors.bottom: parent.bottom
        color: hovered ? "#FFFFFF22" : "#FFFFFF14"
        border.width: 1
        border.color: hovered ? "#BFD3FF66" : "#FFFFFF18"
        y: hovered ? -8 : 0

        Behavior on y {
            NumberAnimation {
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        Text {
            anchors.centerIn: parent
            text: root.symbol
            color: "#F5F7FA"
            font.pixelSize: hovered ? 24 : 20
            font.weight: Font.DemiBold

            Behavior on font.pixelSize {
                NumberAnimation {
                    duration: 120
                }
            }
        }
    }

    Rectangle {
        width: 5
        height: 5
        radius: 3
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        color: hovered ? "#8BE9FD" : "#7AA2FF88"
        opacity: 0.95
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: iconBubble.top
        anchors.bottomMargin: 10
        width: tooltipText.implicitWidth + 18
        height: 30
        radius: 14
        color: "#171A20"
        border.width: 1
        border.color: "#FFFFFF16"
        opacity: hovered ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation {
                duration: 110
            }
        }

        Text {
            id: tooltipText
            anchors.centerIn: parent
            text: root.label
            color: "#F5F7FA"
            font.pixelSize: 11
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
    }
}
