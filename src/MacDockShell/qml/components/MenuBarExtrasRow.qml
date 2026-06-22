pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

RowLayout {
    id: root
    spacing: 4

    property bool darkTheme: false
    property int pendingMirrorIndex: -1
    property int mirroredSessionId: -1
    property var mirroredItems: []
    property int mirroredAnchorX: 0
    property int mirroredAnchorY: 0
    property var menuLevels: []
    readonly property int menuRowWidth: 236
    readonly property int menuLevelGap: 4
    readonly property color iconColor: darkTheme ? "#F2F2F7" : "#1D1D1F"
    readonly property color hoverFill: darkTheme ? "#5A5A5E" : "#DADADC"
    readonly property color pressedFill: darkTheme ? "#6E6E72" : "#C8C8CC"
    readonly property color activeFill: darkTheme ? "#636366" : "#D1D1D6"
    readonly property color menuBg: darkTheme ? "#2E2E30" : "#F5F5F7"
    readonly property color menuBorder: darkTheme ? "#4A4A4D" : "#C2C2C2"
    readonly property color menuText: darkTheme ? "#F2F2F7" : "#1D1D1F"
    readonly property color menuDisabledText: darkTheme ? "#77777C" : "#9A9A9E"
    readonly property color menuSeparator: darkTheme ? "#505054" : "#D6D6D6"

    visible: taskbarController.showMenuBarExtras && trayIconModel.enabled

    function closeMirroredMenu() {
        mirroredMenuPopup.close()
        pendingMirrorIndex = -1
        menuLevels = []
    }

    function menuRowHeight(item) {
        return item && item.separator ? 9 : 27
    }

    function menuPanelHeight(items) {
        var h = 0
        for (var i = 0; items && i < items.length; ++i)
            h += menuRowHeight(items[i])
        return h
    }

    function maxMenuPanelHeight() {
        var h = 1
        for (var i = 0; i < menuLevels.length; ++i)
            h = Math.max(h, menuPanelHeight(menuLevels[i].items))
        return h
    }

    function showSubmenu(level, item, rowY) {
        if (!item || !item.children || item.children.length === 0) {
            menuLevels = menuLevels.slice(0, level + 1)
            return
        }

        var parentLevel = menuLevels[level]
        var levels = menuLevels.slice(0, level + 1)
        levels.push({
            items: item.children,
            pathPrefix: parentLevel.pathPrefix.concat([item.index]),
            y: Math.max(0, rowY - 6)
        })
        menuLevels = levels
    }

    function closeSubmenusAfter(level) {
        if (menuLevels.length > level + 1)
            menuLevels = menuLevels.slice(0, level + 1)
    }

    function openMirroredMenu(sessionId, items, anchorX, anchorY) {
        mirroredSessionId = sessionId
        mirroredItems = items
        mirroredAnchorX = anchorX
        mirroredAnchorY = anchorY
        menuLevels = [{ items: items, pathPrefix: [], y: 0 }]
        var local = root.mapFromGlobal(anchorX, anchorY)
        mirroredMenuPopup.x = Math.round(local.x - mirroredMenuPopup.implicitWidth / 2)
        mirroredMenuPopup.y = Math.round(local.y + 8)
        mirroredMenuPopup.open()
    }

    Connections {
        target: trayIconModel
        function onMirroredMenuReady(sessionId, items, anchorX, anchorY) {
            root.openMirroredMenu(sessionId, items, anchorX, anchorY)
        }
        function onMirroredMenuFailed(index) {
            if (root.pendingMirrorIndex === index) {
                root.pendingMirrorIndex = -1
                trayIconModel.showMenu(index)
            }
        }
    }

    Popup {
        id: mirroredMenuPopup
        popupType: Popup.Window
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 6
        implicitWidth: Math.max(1, root.menuLevels.length) * root.menuRowWidth
                       + Math.max(0, root.menuLevels.length - 1) * root.menuLevelGap
                       + leftPadding + rightPadding
        implicitHeight: root.maxMenuPanelHeight() + topPadding + bottomPadding

        onClosed: {
            root.pendingMirrorIndex = -1
            root.menuLevels = []
        }

        background: Item {}

        contentItem: Item {
            implicitWidth: mirroredMenuPopup.implicitWidth
                           - mirroredMenuPopup.leftPadding
                           - mirroredMenuPopup.rightPadding
            implicitHeight: mirroredMenuPopup.implicitHeight
                            - mirroredMenuPopup.topPadding
                            - mirroredMenuPopup.bottomPadding

            Repeater {
                model: root.menuLevels

                delegate: Rectangle {
                    id: levelPanel
                    required property var modelData
                    required property int index

                    x: index * (root.menuRowWidth + root.menuLevelGap)
                    y: modelData.y || 0
                    width: root.menuRowWidth
                    height: root.menuPanelHeight(modelData.items) + 12
                    radius: 8
                    color: root.menuBg
                    border.width: 1
                    border.color: root.menuBorder

                    Column {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 0

                        Repeater {
                            model: levelPanel.modelData.items

                            delegate: Item {
                                id: rowRoot
                                required property var modelData
                                required property int index
                                readonly property bool separator: !!modelData.separator
                                readonly property bool itemEnabled: modelData.enabled !== false
                                readonly property bool hasChildren: modelData.children && modelData.children.length > 0
                                readonly property bool submenu: !!modelData.hasSubmenu
                                readonly property var itemPath: levelPanel.modelData.pathPrefix.concat([modelData.index])

                                width: root.menuRowWidth - 12
                                height: root.menuRowHeight(modelData)

                                Rectangle {
                                    visible: rowRoot.separator
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    height: 1
                                    color: root.menuSeparator
                                }

                                Rectangle {
                                    visible: !rowRoot.separator
                                    anchors.fill: parent
                                    anchors.leftMargin: 2
                                    anchors.rightMargin: 2
                                    radius: 5
                                    color: menuMouse.containsMouse && rowRoot.itemEnabled ? "#3478F6" : "transparent"
                                }

                                Text {
                                    visible: !rowRoot.separator && !!rowRoot.modelData.checked
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 9
                                    text: "\u2713"
                                    font.pixelSize: 12
                                    color: menuMouse.containsMouse && rowRoot.itemEnabled ? "white" : root.menuText
                                }

                                Text {
                                    visible: !rowRoot.separator
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.right: chevron.left
                                    anchors.leftMargin: 26
                                    anchors.rightMargin: 8
                                    text: rowRoot.modelData.text || ""
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                    font.pixelSize: 13
                                    color: !rowRoot.itemEnabled
                                           ? root.menuDisabledText
                                           : (menuMouse.containsMouse ? "white" : root.menuText)
                                }

                                Text {
                                    id: chevron
                                    visible: !rowRoot.separator && rowRoot.submenu
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.right: parent.right
                                    anchors.rightMargin: 12
                                    text: "\u203A"
                                    font.pixelSize: 16
                                    color: !rowRoot.itemEnabled
                                           ? root.menuDisabledText
                                           : (menuMouse.containsMouse ? "white" : root.menuText)
                                }

                                MouseArea {
                                    id: menuMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    enabled: !rowRoot.separator && rowRoot.itemEnabled
                                    cursorShape: Qt.PointingHandCursor
                                    onEntered: {
                                        if (rowRoot.hasChildren)
                                            root.showSubmenu(levelPanel.index, rowRoot.modelData, rowRoot.y + levelPanel.y)
                                        else
                                            root.closeSubmenusAfter(levelPanel.index)
                                    }
                                    onClicked: {
                                        if (rowRoot.submenu && rowRoot.hasChildren) {
                                            root.showSubmenu(levelPanel.index, rowRoot.modelData, rowRoot.y + levelPanel.y)
                                            return
                                        }
                                        trayIconModel.invokeMirroredMenuItem(root.mirroredSessionId, rowRoot.itemPath)
                                        root.closeMirroredMenu()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

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
            active: root.pendingMirrorIndex === index
            onClicked: trayIconModel.activate(index)
            onRightClicked: {
                var globalPoint = mapToGlobal(width / 2, height)
                root.pendingMirrorIndex = index
                trayIconModel.requestMirroredMenu(index, Math.round(globalPoint.x), Math.round(globalPoint.y))
            }
        }
    }
}
