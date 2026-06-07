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
    // macOS layout: a centered pill for icons plus transparent side wings so edge
    // tooltips and hover magnify stay centered on the icon instead of being clamped.
    readonly property int dockEndCap: 18
    readonly property int hoverBleed: Math.max(8, Math.ceil(taskbarController.dockIconSize * 0.12))
    readonly property int tooltipWing: 128
    readonly property int dockPillWidth: dockRow.implicitWidth + 2 * dockEndCap
    width: Math.min(dockPillWidth + 2 * tooltipWing, Screen.width - 40)
    height: taskbarController.dockIconSize + 96 + dockBottomGap
    x: Math.round((Screen.width - width) / 2)
    // Resting position; slides fully below the screen when a fullscreen app is active.
    readonly property int restY: Screen.height - height - 18 + dockBottomGap
    readonly property real iconDevicePixelRatio: screen ? screen.devicePixelRatio : 1.0
    readonly property bool darkTheme: taskbarController.darkTheme
    readonly property int themeAnimMs: 320
    y: (taskbarController.shellActive && taskbarController.dockAutoHidden) ? (Screen.height + 4) : restY
    visible: taskbarController.shellActive
    color: "transparent"
    title: "MacDockShellDock"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    Component.onCompleted: windowEffects.applyDockGlass(dockWindow)

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

    // Soft shadow under the dock pill.
    Rectangle {
        id: dockShadow
        anchors.horizontalCenter: dockBg.horizontalCenter
        anchors.verticalCenter: dockBg.verticalCenter
        anchors.verticalCenterOffset: 4
        width: dockBg.width
        height: dockBg.height
        radius: dockBg.radius
        color: "#00000000"
        z: -1
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: "#3A000000"
            shadowBlur: 0.65
            shadowVerticalOffset: 5
            autoPaddingEnabled: true
        }
    }

    Rectangle {
        id: dockBg
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: dockWindow.dockBottomGap
        width: dockWindow.dockPillWidth
        height: taskbarController.dockIconSize + 38
        radius: 22
        // macOS Monterey 12 dock — opaque tints (no see-through).
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: dockWindow.darkTheme ? "#323234" : "#D6DAE2"
                Behavior on color {
                    ColorAnimation { duration: dockWindow.themeAnimMs; easing.type: Easing.InOutCubic }
                }
            }
            GradientStop {
                position: 0.48
                color: dockWindow.darkTheme ? "#2C2C2E" : "#CCD1D9"
                Behavior on color {
                    ColorAnimation { duration: dockWindow.themeAnimMs; easing.type: Easing.InOutCubic }
                }
            }
            GradientStop {
                position: 1.0
                color: dockWindow.darkTheme ? "#252527" : "#C4CAD3"
                Behavior on color {
                    ColorAnimation { duration: dockWindow.themeAnimMs; easing.type: Easing.InOutCubic }
                }
            }
        }
        border.width: 1
        border.color: dockWindow.darkTheme ? "#48484A" : "#E8ECF2"
        Behavior on border.color {
            ColorAnimation { duration: dockWindow.themeAnimMs; easing.type: Easing.InOutCubic }
        }

        // Subtle top sheen — Monterey rim.
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 1
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            height: 1
            radius: 1
            color: dockWindow.darkTheme ? "#5A5A5E" : "#F4F6F9"
            Behavior on color {
                ColorAnimation { duration: dockWindow.themeAnimMs; easing.type: Easing.InOutCubic }
            }
        }

        // Thin inner hairline for crisp definition.
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.width: 1
            border.color: dockWindow.darkTheme ? "#1E1E20" : "#B8BEC8"
            Behavior on border.color {
                ColorAnimation { duration: dockWindow.themeAnimMs; easing.type: Easing.InOutCubic }
            }
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
        contentWidth: Math.max(width, dockRow.implicitWidth)
        contentHeight: height
        clip: false
        interactive: dockRow.implicitWidth > width

        Row {
            id: dockRow
            x: dockRow.implicitWidth < parent.width ? Math.round((parent.width - dockRow.implicitWidth) / 2) : 0
            y: parent.height - (taskbarController.dockIconSize + 38) - dockWindow.dockBottomGap
            height: taskbarController.dockIconSize + 38
            spacing: 14
            leftPadding: dockWindow.hoverBleed
            rightPadding: dockWindow.hoverBleed

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
                    // Fixed slot width: magnify via transform only so Row layout does not shift
                    // under the cursor (which caused hover enter/leave jitter).
                    property bool magnifyHover: mouseArea.containsMouse
                            && !taskbarController.dockStaticIcons
                            && !dockWindow.reordering
                            && !launchBounceAnim.running
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

                    width: taskbarController.dockIconSize
                    height: taskbarController.dockIconSize + 38

                    opacity: dockItemRoot.running || dockItemRoot.pinned ? 1.0 : 0.55
                    z: dockItemRoot.dragging ? 100 : (dockItemRoot.magnifyHover ? 10 : 0)

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
                        anchors.verticalCenterOffset: (dockItemRoot.magnifyHover && taskbarController.dockHoverBounce) ? -8 : 0
                        color: "transparent"
                        border.width: 0
                        border.color: "transparent"

                        // Drag-follow translate, launch bounce, and hover magnify (visual only).
                        transform: [
                            Translate { x: dockItemRoot.slideX },
                            Translate { id: launchTranslate },
                            Scale {
                                id: hoverScale
                                origin.x: iconBubble.width / 2
                                origin.y: iconBubble.height / 2
                                xScale: dockItemRoot.magnifyHover ? 1.18 : 1.0
                                yScale: dockItemRoot.magnifyHover ? 1.18 : 1.0
                                Behavior on xScale {
                                    NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
                                }
                                Behavior on yScale {
                                    NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
                                }
                            }
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
                            width: Math.round(taskbarController.dockIconSize * 0.96)
                            height: width

                            Image {
                                id: dockIconImage
                                anchors.centerIn: parent
                                width: parent.width
                                height: parent.height
                                source: dockItemRoot.iconUrl
                                // Decode at display DPR so Qt downscales from cache, never upscales.
                                sourceSize: Qt.size(
                                    Math.ceil(width * dockWindow.iconDevicePixelRatio * 1.25),
                                    Math.ceil(height * dockWindow.iconDevicePixelRatio * 1.25))
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: false
                                visible: status === Image.Ready
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
                                font.pixelSize: dockItemRoot.magnifyHover ? 24 : 20
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
                        color: dockWindow.darkTheme ? "#FFFFFF" : "#000000"
                        opacity: dockWindow.darkTheme ? 0.42 : 0.35
                        Behavior on color {
                            ColorAnimation { duration: dockWindow.themeAnimMs; easing.type: Easing.InOutCubic }
                        }
                        Behavior on opacity {
                            NumberAnimation { duration: dockWindow.themeAnimMs; easing.type: Easing.InOutCubic }
                        }
                    }

                    // macOS-style label: compact bubble centered above the icon (app name only).
                    Item {
                        id: tooltipHost
                        anchors.horizontalCenter: iconBubble.horizontalCenter
                        anchors.bottom: iconBubble.top
                        anchors.bottomMargin: 8
                        width: tooltipBg.width
                        height: tooltipBg.height + tooltipTail.height
                        opacity: (mouseArea.containsMouse && !dockWindow.reordering) ? 1 : 0
                        visible: opacity > 0
                        z: 20

                        Behavior on opacity {
                            NumberAnimation { duration: 110 }
                        }

                        Rectangle {
                            id: tooltipBg
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            width: Math.min(200, Math.max(52, tooltipText.implicitWidth + 18))
                            height: tooltipText.implicitHeight + 10
                            radius: 6
                            color: dockWindow.darkTheme ? "#2A2A2CF0" : "#F7F7F7F2"
                            border.width: 1
                            border.color: dockWindow.darkTheme ? "#4A4A4E" : "#C8C8C8"

                            Text {
                                id: tooltipText
                                anchors.centerIn: parent
                                text: dockItemRoot.label
                                color: dockWindow.darkTheme ? "#F2F2F7" : "#1C222B"
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                        }

                        Canvas {
                            id: tooltipTail
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: tooltipBg.bottom
                            anchors.topMargin: -1
                            width: 14
                            height: 7
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                ctx.beginPath()
                                ctx.moveTo(width / 2, height)
                                ctx.lineTo(0, 0)
                                ctx.lineTo(width, 0)
                                ctx.closePath()
                                ctx.fillStyle = dockWindow.darkTheme ? "#2A2A2C" : "#F7F7F7"
                                ctx.fill()
                                ctx.strokeStyle = dockWindow.darkTheme ? "#4A4A4E" : "#C8C8C8"
                                ctx.lineWidth = 1
                                ctx.stroke()
                            }
                        }
                    }

                    // macOS-style context menu for a running dock app
                    Menu {
                        id: appContextMenu
                        popupType: Popup.Window
                        // Suspend the periodic model refresh while open, otherwise the model
                        // reset recreates this delegate and the menu closes on its own.
                        onOpened: dockModel.setReorderActive(true)
                        onClosed: dockModel.setReorderActive(false)
                        // Extra bottom padding leaves room for the downward arrow tail.
                        topPadding: 6
                        leftPadding: 6
                        rightPadding: 6
                        bottomPadding: 16
                        implicitWidth: 200
                        // Centered horizontally over the icon and floating just above it (macOS-style).
                        x: Math.round(dockItemRoot.width / 2 - width / 2)
                        y: -height - 4

                        // Rounded body plus a downward arrow drawn as one path (no seam).
                        background: Canvas {
                            implicitWidth: 200
                            property real arrowH: 10
                            property real arrowW: 20
                            property real bodyRadius: 9
                            onWidthChanged: requestPaint()
                            onHeightChanged: requestPaint()
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.reset()
                                var w = width
                                var h = height
                                var bodyH = h - arrowH
                                var r = bodyRadius
                                var cx = w / 2
                                ctx.beginPath()
                                ctx.moveTo(r, 0)
                                ctx.lineTo(w - r, 0)
                                ctx.arcTo(w, 0, w, r, r)
                                ctx.lineTo(w, bodyH - r)
                                ctx.arcTo(w, bodyH, w - r, bodyH, r)
                                ctx.lineTo(cx + arrowW / 2, bodyH)
                                ctx.lineTo(cx, bodyH + arrowH)
                                ctx.lineTo(cx - arrowW / 2, bodyH)
                                ctx.lineTo(r, bodyH)
                                ctx.arcTo(0, bodyH, 0, bodyH - r, r)
                                ctx.lineTo(0, r)
                                ctx.arcTo(0, 0, r, 0, r)
                                ctx.closePath()
                                ctx.fillStyle = "#F5F5F5"
                                ctx.fill()
                                ctx.strokeStyle = "#C2C2C2"
                                ctx.lineWidth = 1
                                ctx.stroke()
                            }
                        }

                        delegate: MenuItem {
                            id: menuEntry
                            implicitWidth: 188
                            implicitHeight: 26
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
                            implicitWidth: 190

                            background: Rectangle {
                                implicitWidth: 190
                                color: "#F5F5F5"
                                radius: 9
                                border.width: 1
                                border.color: "#C2C2C2"
                            }

                            delegate: MenuItem {
                                id: subEntry
                                implicitWidth: 178
                                implicitHeight: 26
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
