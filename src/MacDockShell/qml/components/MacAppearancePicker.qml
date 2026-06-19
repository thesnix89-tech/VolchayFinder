import QtQuick
import QtQuick.Layouts

Row {
    id: root

    property string currentMode: "auto"
    property color labelColor: "#1D1D1F"

    signal modeSelected(string mode)

    spacing: 20

    readonly property var modes: [
        { mode: "auto", label: qsTr("Auto"), source: "qrc:/src/MacDockShell/icons/appearance_auto.png" },
        { mode: "light", label: qsTr("Light"), source: "qrc:/src/MacDockShell/icons/appearance_light.png" },
        { mode: "dark", label: qsTr("Dark"), source: "qrc:/src/MacDockShell/icons/appearance_dark.png" }
    ]

    Repeater {
        model: root.modes

        delegate: Column {
            spacing: 8

            readonly property bool selected: root.currentMode === modelData.mode

            Rectangle {
                id: frame
                width: 126
                height: 86
                radius: 13
                color: "transparent"
                border.width: 3
                border.color: selected ? "#007AFF" : "transparent"

                Rectangle {
                    anchors.centerIn: parent
                    width: 120
                    height: 80
                    radius: 10
                    color: "#000000"
                    clip: true

                    Image {
                        anchors.fill: parent
                        source: modelData.source
                        fillMode: Image.PreserveAspectCrop
                        smooth: true
                        antialiasing: true
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.modeSelected(modelData.mode)
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: modelData.label
                color: root.labelColor
                font.pixelSize: 12
                font.weight: selected ? Font.DemiBold : Font.Normal
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
