import QtQuick
import QtQuick.Layouts

GlassPanel {
    id: root
    property var modelData: []

    width: contentRow.implicitWidth + 32
    height: 86
    cornerRadius: 28
    fillColor: "#F5F7FA12"
    strokeColor: "#FFFFFF24"

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
