import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: controlWindow
    width: 140
    height: 38
    x: Screen.width - width - 20
    y: Screen.height - height - 128
    visible: true
    color: "transparent"
    title: "MacDockShellControls"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    Rectangle {
        anchors.fill: parent
        radius: 19
        color: closeArea.containsMouse ? "#B63A3ACC" : "#8A2E2ECC"
        border.width: 1
        border.color: "#FFFFFF22"

        Text {
            anchors.centerIn: parent
            text: "Exit shell"
            color: "#F5F7FA"
            font.pixelSize: 13
            font.weight: Font.Medium
        }

        MouseArea {
            id: closeArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: taskbarController.quitApplication()
        }
    }
}
