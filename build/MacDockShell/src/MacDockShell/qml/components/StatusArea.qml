import QtQuick
import QtQuick.Layouts

RowLayout {
    spacing: 10

    Repeater {
        model: ["⌃", "◌", "◎", "23:41"]
        delegate: Text {
            text: modelData
            color: "#E9EDF5"
            font.pixelSize: 12
            opacity: 0.92
        }
    }
}
