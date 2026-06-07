import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

Window {
    id: dockWindow
    // Extra room below the dock so its soft drop shadow is not clipped by the window edge.
    property int dockBottomGap: 16
    // Drag-to-reorder state (shared so neighbors can open a gap for the dragged icon).
    property int dragFrom: -1
    property int dragTo: -1
    readonly property bool reordering: dragFrom >= 0
    width: Math.min(dockRow.implicitWidth + 32, Screen.width - 40)
    height: taskbarController.dockIconSize + 86 + dockBottomGap
    x: Math.round((Screen.width - width) / 2)
    // Resting position; slides fully below the screen when a fullscreen app is active.
    readonly property int restY: Screen.height - height - 18 + dockBottomGap
    y: (taskbarController.shellActive && taskbarController.fullscreenAppActive) ? (Screen.height + 4) : restY
    visible: taskbarController.shellActive
    color: "transparent"
    title: "MacDockShellDock"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    // macOS-style: glide the dock down/up when toggling fullscreen.
    Behavior on y {
        NumberAnimation {
            duration: 280
            easing.type: Easing.InOutCubic
        }
    }

    Timer {
        interval: 1500
        repeat: true
        running: false
        onTriggered: dockModel.refresh()
    }

    // Context Menu for Dock Background
    Menu {
        id: dockContextMenu
        
        MenuItem {
            text: "Системные настройки..."
            onTriggered: taskbarController.settingsVisible = true
        }
        
        MenuSeparator {}
        
        MenuItem {
            text: "Выход из оболочки"
            onTriggered: taskbarController.quitApplication()
        }
    }

    Rectangle {
        id: dockBg
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: dockWindow.dockBottomGap
        height: taskbarController.dockIconSize + 38
        radius: 28
        // Monterey-style frosted panel: solid (opaque) light gradient, no see-through.
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#FFF6F7F9" }
            GradientStop { position: 1.0; color: "#FFE9EBEF" }
        }
        border.width: 1
        border.color: "#FFFFFFFF"

        // Soft drop shadow to lift the dock off the wallpaper like macOS.
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#55000000"
            shadowBlur: 0.7
            shadowVerticalOffset: 6
            autoPaddingEnabled: true
        }

        // Bright rim highlight along the top edge (glass sheen).
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 1
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            height: 1
            radius: 1
            color: "#AAFFFFFF"
        }

        // Thin inner hairline for crisp definition.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: "#14000000"
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton
            onClicked: function(mouse) {
                dockContextMenu.popup()
            }
        }
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        contentWidth: Math.max(width, dockRow.implicitWidth)
        contentHeight: height
        clip: true
        interactive: dockRow.implicitWidth > width

        Row {
            id: dockRow
            x: dockRow.implicitWidth < parent.width ? Math.round((parent.width - dockRow.implicitWidth) / 2) : 0
            y: parent.height - (taskbarController.dockIconSize + 38) - dockWindow.dockBottomGap
            height: taskbarController.dockIconSize + 38
            spacing: 14

            Repeater {
                id: dockRepeater
                model: dockModel

                delegate: Item {
                    id: dockItemRoot
                    required property int index
                    required property string label
                    required property string windowTitle
                    required property string iconHint
                    required property string iconUrl
                    required property bool running
                    required property bool active
                    required property bool pinned
                    required property bool minimized
                    required property bool clickable

                    // Drag-to-reorder bookkeeping
                    property bool dragging: false
                    property real grabDX: 0
                    property real pointerInRow: 0
                    property bool suppressClick: false
                    property real dragOffset: dragging ? (pointerInRow - grabDX - x) : 0
                    // Horizontal shift applied to the icon: the dragged icon follows the
                    // pointer; the others slide aside to open a gap at the drop target.
                    property real slideX: {
                        if (dockItemRoot.dragging)
                            return dockItemRoot.dragOffset
                        var from = dockWindow.dragFrom
                        if (from < 0)
                            return 0
                        var to = dockWindow.dragTo
                        var stride = taskbarController.dockIconSize + dockRow.spacing
                        var i = dockItemRoot.index
                        if (to > from && i > from && i <= to)
                            return -stride
                        if (to < from && i >= to && i < from)
                            return stride
                        return 0
                    }

                    width: (mouseArea.containsMouse && !taskbarController.dockStaticIcons && !dockWindow.reordering) ? Math.round(taskbarController.dockIconSize * 1.333) : taskbarController.dockIconSize
                    height: taskbarController.dockIconSize + 38

                    opacity: dockItemRoot.running || dockItemRoot.pinned ? 1.0 : 0.55
                    z: dockItemRoot.dragging ? 100 : 0

                    Behavior on width {
                        NumberAnimation {
                            duration: 140
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on slideX {
                        enabled: !dockItemRoot.dragging
                        NumberAnimation {
                            duration: 130
                            easing.type: Easing.OutCubic
                        }
                    }

                    Rectangle {
                        id: iconBubble
                        width: parent.width
                        height: parent.width
                        radius: 18
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: (mouseArea.containsMouse && taskbarController.dockHoverBounce && !taskbarController.dockStaticIcons && !dockWindow.reordering) ? -8 : 0
                        color: "transparent"
                        border.width: 0
                        border.color: "transparent"

                        // Drag-follow translate plus the macOS-style launch bounce (kept separate
                        // so neither fights the hover binding).
                        transform: [
                            Translate { x: dockItemRoot.slideX },
                            Translate { id: launchTranslate }
                        ]

                        SequentialAnimation {
                            id: launchBounceAnim
                            running: false
                            // Single launch hop in every mode.
                            loops: 1
                            NumberAnimation { target: launchTranslate; property: "y"; from: 0; to: -22; duration: 220; easing.type: Easing.OutQuad }
                            NumberAnimation { target: launchTranslate; property: "y"; to: 0; duration: 280; easing.type: Easing.OutBounce }
                        }

                        Behavior on anchors.verticalCenterOffset {
                            NumberAnimation {
                                duration: 140
                                easing.type: Easing.OutCubic
                            }
                        }

                        Item {
                            id: iconContainer
                            anchors.centerIn: parent
                            width: (mouseArea.containsMouse && !taskbarController.dockStaticIcons && !dockWindow.reordering) ? Math.round(taskbarController.dockIconSize * 1.05) : Math.round(taskbarController.dockIconSize * 0.82)
                            height: (mouseArea.containsMouse && !taskbarController.dockStaticIcons && !dockWindow.reordering) ? Math.round(taskbarController.dockIconSize * 1.05) : Math.round(taskbarController.dockIconSize * 0.82)

                            Behavior on width {
                                NumberAnimation {
                                    duration: 140
                                    easing.type: Easing.OutCubic
                                }
                            }
                            Behavior on height {
                                NumberAnimation {
                                    duration: 140
                                    easing.type: Easing.OutCubic
                                }
                            }

                            Image {
                                id: dockIconImage
                                anchors.centerIn: parent
                                width: parent.width
                                height: parent.height
                                source: dockItemRoot.iconUrl
                                // Decode at full icon-cache resolution for crisp scaling.
                                sourceSize: Qt.size(256, 256)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                                antialiasing: true
                                visible: status === Image.Ready

                                // Subtle shadow gives each icon depth on the glass, like macOS.
                                layer.enabled: true
                                layer.effect: MultiEffect {
                                    shadowEnabled: true
                                    shadowColor: "#3C000000"
                                    shadowBlur: 0.45
                                    shadowVerticalOffset: 3
                                    autoPaddingEnabled: true
                                }
                            }

                            Rectangle {
                                anchors.centerIn: parent
                                width: parent.width
                                height: parent.height
                                radius: 8
                                color: "#D7DDE780"
                                visible: dockIconImage.status !== Image.Ready
                            }

                            Text {
                                anchors.centerIn: parent
                                text: dockItemRoot.iconHint.length > 0 ? dockItemRoot.iconHint : dockItemRoot.label.slice(0, 1)
                                color: "#1C222B"
                                font.pixelSize: (mouseArea.containsMouse && !taskbarController.dockStaticIcons && !dockWindow.reordering) ? 24 : 20
                                font.weight: Font.DemiBold
                                visible: dockIconImage.status !== Image.Ready
                            }
                        }
                    }

                    Rectangle {
                        width: 4
                        height: 4
                        radius: 2
                        visible: dockItemRoot.running
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 6
                        color: "#000000"
                        opacity: 0.35
                    }

                    Rectangle {
                        id: tooltipBg
                        anchors.bottom: iconBubble.top
                        anchors.bottomMargin: 10
                        width: Math.max(tooltipColumn.implicitWidth + 18, 72)
                        height: tooltipColumn.implicitHeight + 12
                        radius: 14
                        color: "#FFFFFFF7"
                        border.width: 1
                        border.color: "#DDE3EC"
                        opacity: (mouseArea.containsMouse && !dockWindow.reordering) ? 1 : 0
                        visible: opacity > 0
                        z: 20

                        x: {
                            var preferredX = (dockItemRoot.width - width) / 2;
                            var delegateAbsX = 16 + dockRow.x - flickable.contentX + dockItemRoot.x;
                            var clampedAbsX = Math.max(8, Math.min(dockWindow.width - width - 8, delegateAbsX + preferredX));
                            return clampedAbsX - delegateAbsX;
                        }

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 110
                            }
                        }

                        Column {
                            id: tooltipColumn
                            anchors.centerIn: parent
                            spacing: 1

                            Text {
                                id: tooltipText
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: dockItemRoot.label
                                color: "#1C222B"
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.NoWrap
                                maximumLineCount: 1
                                elide: Text.ElideRight
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: Math.min(220, implicitWidth)
                                text: dockItemRoot.windowTitle
                                color: "#6B7380"
                                font.pixelSize: 9
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.NoWrap
                                maximumLineCount: 1
                                elide: Text.ElideRight
                                visible: dockItemRoot.windowTitle.length > 0
                            }
                        }
                    }

                    // macOS-style context menu for a running dock app
                    Menu {
                        id: appContextMenu
                        popupType: Popup.Window
                        padding: 6
                        implicitWidth: 240
                        // Centered horizontally over the icon and floating just above it (macOS-style).
                        x: Math.round(dockItemRoot.width / 2 - width / 2)
                        y: -height - 12

                        background: Rectangle {
                            implicitWidth: 240
                            color: "#F5F5F5"
                            radius: 9
                            border.width: 1
                            border.color: "#C2C2C2"
                        }

                        delegate: MenuItem {
                            id: menuEntry
                            implicitWidth: 228
                            implicitHeight: 30
                            padding: 0
                            arrow: Item {}
                            indicator: Item {}

                            contentItem: Item {
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.leftMargin: 16
                                    text: menuEntry.text
                                    font.pixelSize: 13
                                    color: menuEntry.highlighted ? "white" : "#1D1D1F"
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.right: parent.right
                                    anchors.rightMargin: 12
                                    text: "\u203A"
                                    font.pixelSize: 16
                                    color: menuEntry.highlighted ? "white" : "#1D1D1F"
                                    visible: menuEntry.subMenu !== null
                                }
                            }

                            background: Rectangle {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                radius: 5
                                color: menuEntry.highlighted ? "#3478F6" : "transparent"
                            }
                        }

                        Menu {
                            title: "Параметры"
                            popupType: Popup.Window
                            padding: 6
                            implicitWidth: 220

                            background: Rectangle {
                                implicitWidth: 220
                                color: "#F5F5F5"
                                radius: 9
                                border.width: 1
                                border.color: "#C2C2C2"
                            }

                            delegate: MenuItem {
                                id: subEntry
                                implicitWidth: 208
                                implicitHeight: 30
                                padding: 0
                                arrow: Item {}
                                indicator: Item {}
                                contentItem: Text {
                                    verticalAlignment: Text.AlignVCenter
                                    leftPadding: 16
                                    text: subEntry.text
                                    font.pixelSize: 13
                                    color: subEntry.highlighted ? "white" : "#1D1D1F"
                                }
                                background: Rectangle {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    radius: 5
                                    color: subEntry.highlighted ? "#3478F6" : "transparent"
                                }
                            }

                            Action {
                                text: "Показать в Проводнике"
                                onTriggered: {
                                    var i = dockItemRoot.index
                                    Qt.callLater(function() { dockModel.revealIndex(i) })
                                }
                            }
                        }

                        MenuSeparator {
                            topPadding: 4
                            bottomPadding: 4
                            contentItem: Rectangle {
                                implicitHeight: 1
                                color: "#D6D6D6"
                            }
                        }

                        Action {
                            text: "Показать все окна"
                            onTriggered: {
                                var i = dockItemRoot.index
                                Qt.callLater(function() { dockModel.activateIndex(i) })
                            }
                        }
                        Action {
                            text: "Скрыть"
                            onTriggered: {
                                var i = dockItemRoot.index
                                Qt.callLater(function() { dockModel.minimizeIndex(i) })
                            }
                        }
                        Action {
                            text: "Завершить"
                            onTriggered: {
                                var i = dockItemRoot.index
                                Qt.callLater(function() { dockModel.closeIndex(i) })
                            }
                        }
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        // Keep the press so a horizontal drag is not stolen by the Flickable.
                        preventStealing: true
                        cursorShape: dockItemRoot.dragging ? Qt.ClosedHandCursor : Qt.PointingHandCursor

                        onPressed: function(mouse) {
                            if (mouse.button !== Qt.LeftButton)
                                return
                            dockItemRoot.suppressClick = false
                            dockItemRoot.dragging = false
                            var p = mapToItem(dockRow, mouse.x, 0)
                            dockItemRoot.grabDX = p.x - dockItemRoot.x
                            dockItemRoot.pointerInRow = p.x
                        }

                        onPositionChanged: function(mouse) {
                            if (!(mouse.buttons & Qt.LeftButton))
                                return
                            var p = mapToItem(dockRow, mouse.x, 0)
                            dockItemRoot.pointerInRow = p.x
                            if (!dockItemRoot.dragging) {
                                // Start a drag only after a small threshold so taps still click.
                                if (Math.abs((p.x - dockItemRoot.grabDX) - dockItemRoot.x) <= 6)
                                    return
                                dockItemRoot.dragging = true
                                dockWindow.dragFrom = dockItemRoot.index
                                dockModel.setReorderActive(true)
                            }
                            var stride = taskbarController.dockIconSize + dockRow.spacing
                            var desired = Math.round((p.x - dockItemRoot.grabDX) / stride)
                            if (desired < 0)
                                desired = 0
                            if (desired > dockRepeater.count - 1)
                                desired = dockRepeater.count - 1
                            dockWindow.dragTo = desired
                        }

                        onReleased: function(mouse) {
                            if (!dockItemRoot.dragging)
                                return
                            var from = dockWindow.dragFrom
                            var to = dockWindow.dragTo
                            dockItemRoot.dragging = false
                            dockItemRoot.suppressClick = true
                            dockWindow.dragFrom = -1
                            dockWindow.dragTo = -1
                            dockModel.setReorderActive(false)
                            if (from >= 0 && to >= 0 && from !== to)
                                dockModel.moveItem(from, to)
                        }

                        onCanceled: {
                            if (!dockItemRoot.dragging)
                                return
                            dockItemRoot.dragging = false
                            dockWindow.dragFrom = -1
                            dockWindow.dragTo = -1
                            dockModel.setReorderActive(false)
                        }

                        onClicked: function(mouse) {
                            if (dockItemRoot.suppressClick) {
                                dockItemRoot.suppressClick = false
                                return
                            }
                            if (!dockItemRoot.clickable)
                                return
                            var idx = dockItemRoot.index
                            if (mouse.button === Qt.RightButton) {
                                // Running apps get the macOS-style menu; pinned-only icons do nothing.
                                if (dockItemRoot.running)
                                    appContextMenu.open()
                            } else {
                                // Defer model actions out of this signal handler: launching/activating
                                // pumps a nested message loop, and the refresh timer would otherwise
                                // reset the model and destroy this delegate mid-handler (FATAL crash).
                                if (!dockItemRoot.running)
                                    launchBounceAnim.restart()
                                Qt.callLater(function() { dockModel.toggleIndex(idx) })
                            }
                        }
                    }
                }
            }
        }
    }
}
