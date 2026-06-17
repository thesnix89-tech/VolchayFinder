import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    radius: 18
    color: "#F5F7FA10"
    border.width: 1
    border.color: "#FFFFFF22"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 18

        RowLayout {
            spacing: 12

            Text {
                text: ""
                color: "#F5F7FA"
                font.pixelSize: 16
                font.weight: Font.Medium
            }

            Repeater {
                model: ["Finder", "File", "Edit", "View", "Go", "Window", "Help"]
                delegate: Text {
                    text: modelData
                    color: "#E9EDF5"
                    font.pixelSize: 12
                    opacity: 0.92
                }
            }
        }

        Item { Layout.fillWidth: true }

        Text {
            text: "Workspace"
            color: "#B7BDC9"
            font.pixelSize: 12
        }

        StatusArea { }
    }
}
