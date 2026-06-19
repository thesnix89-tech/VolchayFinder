import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root

    property string currentMode: "auto"
    property color labelColor: "#1D1D1F"

    signal modeSelected(string mode)

    readonly property int tileWidth: 126
    readonly property int tileFrameHeight: 86
    readonly property int tileLabelHeight: 18
    readonly property int tileSpacing: 8
    readonly property int tileColumnHeight: tileFrameHeight + tileSpacing + tileLabelHeight

    spacing: 20

    implicitHeight: tileColumnHeight
    implicitWidth: modes.length * tileWidth + (modes.length - 1) * spacing
    height: implicitHeight

    readonly property var modes: [
        { mode: "auto", label: qsTr("Auto"), source: "qrc:/src/MacDockShell/icons/appearance_auto.png" },
        { mode: "light", label: qsTr("Light"), source: "qrc:/src/MacDockShell/icons/appearance_light.png" },
        { mode: "dark", label: qsTr("Dark"), source: "qrc:/src/MacDockShell/icons/appearance_dark.png" }
    ]

    Repeater {
        model: root.modes

        delegate: Item {
            Layout.preferredWidth: root.tileWidth
            Layout.preferredHeight: root.tileColumnHeight

            readonly property bool selected: root.currentMode === modelData.mode

            ColumnLayout {
                anchors.fill: parent
                spacing: root.tileSpacing

                Item {
                    Layout.preferredWidth: root.tileWidth
                    Layout.preferredHeight: root.tileFrameHeight

                    Image {
                        anchors.centerIn: parent
                        width: 120
                        height: 80
                        source: modelData.source
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        antialiasing: true
                    }

                    Rectangle {
                        anchors.fill: parent
                        radius: 13
                        color: "transparent"
                        border.width: selected ? 3 : 0
                        border.color: "#007AFF"
                        z: 2
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: modelData.label
                    color: root.labelColor
                    font.pixelSize: 12
                    font.weight: selected ? Font.DemiBold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            MouseArea {
                anchors.fill: parent
                z: 3
                cursorShape: Qt.PointingHandCursor
                preventStealing: true
                propagateComposedEvents: false
                onClicked: root.modeSelected(modelData.mode)
            }
        }
    }
}
