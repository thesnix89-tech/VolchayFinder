import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

RowLayout {
    id: root
    spacing: 4

    property bool darkTheme: false
    readonly property color iconColor: darkTheme ? "#F2F2F7" : "#1D1D1F"
    readonly property color hoverFill: darkTheme ? "#5A5A5E" : "#DADADC"
    readonly property color pressedFill: darkTheme ? "#6E6E72" : "#C8C8CC"
    readonly property color activeFill: darkTheme ? "#636366" : "#D1D1D6"

    visible: taskbarController.showMenuBarExtras && trayIconModel.enabled

    component ExtraHit : Item {
        id: hit
        signal clicked()
        signal rightClicked()

        property bool active: false
        property int hitW: 22
        property int hitH: 20
        property string tooltipText: ""
        property string iconSource: ""
        property string iconHint: "?"

        readonly property bool pointerOver: {
            var gp = hoverTracker.globalCursor
            var local = hit.mapFromGlobal(gp.x, gp.y)
            return local.x >= 0 && local.x <= hit.width
                    && local.y >= 0 && local.y <= hit.height
        }

        implicitWidth: hitW
        implicitHeight: hitH
        Layout.alignment: Qt.AlignVCenter

        Rectangle {
            id: highlight
            z: 0
            anchors.centerIn: parent
            width: hit.hitW
            height: hit.hitH
            radius: 5
            color: hit.active ? root.activeFill
                  : (mouse.pressed ? root.pressedFill : root.hoverFill)
            opacity: (hit.active || mouse.pressed || hit.pointerOver) ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 80 } }
        }

        Item {
            z: 1
            anchors.centerIn: parent
            width: 16
            height: 16

            Image {
                anchors.fill: parent
                visible: hit.iconSource !== ""
                source: hit.iconSource
                sourceSize: Qt.size(
                    Math.max(1, Math.round(width * Screen.devicePixelRatio)),
                    Math.max(1, Math.round(height * Screen.devicePixelRatio)))
                fillMode: Image.PreserveAspectFit
                smooth: true
                antialiasing: true
            }

            Text {
                anchors.centerIn: parent
                visible: hit.iconSource === ""
                text: hit.iconHint
                color: root.iconColor
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }
        }

        MouseArea {
            id: mouse
            z: 2
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            cursorShape: Qt.PointingHandCursor
            onClicked: function(mouse) {
                if (mouse.button === Qt.RightButton)
                    hit.rightClicked()
                else
                    hit.clicked()
            }
        }
    }

    Repeater {
        model: trayIconModel
        delegate: ExtraHit {
            required property int index
            required property string tooltip
            required property string iconUrl
            required property string iconHint

            tooltipText: tooltip
            iconSource: iconUrl
            iconHint: iconHint
            onClicked: trayIconModel.activate(index)
            onRightClicked: trayIconModel.showMenu(index)
        }
    }
}
