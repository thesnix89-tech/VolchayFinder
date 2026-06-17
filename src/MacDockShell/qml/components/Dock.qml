import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property var modelData: []

    width: contentRow.implicitWidth + 32
    height: 86
    radius: 28
    color: "#F5F7FA12"
    border.width: 1
    border.color: "#FFFFFF24"

    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: 14

        Repeater {
            model: root.modelData
            delegate: DockItem {
                symbol: modelData.symbol
                label: modelData.label
            }
        }
    }
}
