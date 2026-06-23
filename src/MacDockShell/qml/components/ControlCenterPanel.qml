pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQml.Models

Popup {
    id: root
    popupType: Popup.Window
    modal: false
    focus: true
    clip: true
    closePolicy: Popup.CloseOnPressOutside | Popup.CloseOnEscape
    padding: 10
    width: 320
    implicitHeight: contentColumn.implicitHeight + 20

    property bool darkTheme: false
    property bool wifiOn: false
    property bool bluetoothOn: false
    property bool airDropOn: false
    property bool focusOn: false
    property bool editing: false
    property bool layoutLoaded: false
    property bool dragLayoutDirty: false
    property string draggingControlId: ""
    property string dropTargetControlId: ""
    property real dragGhostX: 0
    property real dragGhostY: 0
    property real dragGhostWidth: 0
    property real dragGhostHeight: 0
    property real dragGhostOffsetX: 0
    property real dragGhostOffsetY: 0
    property var controlShells: []
    property real displayValue: 0.82
    property real soundValue: 0.62
    property var controlLayout: []

    readonly property var defaultControlIds: [
        "wifi", "media", "bluetooth", "airdrop", "keyboard",
        "mirror", "camera", "appearance", "focus", "display", "sound"
    ]
    readonly property var visibleControls: controlLayout.filter(function(control) { return control.visible })
    readonly property var hiddenControls: controlLayout.filter(function(control) { return !control.visible })
    readonly property var editSlots: buildEditSlots()
    readonly property color panelBg: darkTheme ? "#3A3A3C" : "#E2E2E4"
    readonly property color tileBg: darkTheme ? "#6D6D70" : "#777779"
    readonly property color tileBgHover: darkTheme ? "#7A7A7E" : "#838385"
    readonly property color tileBgActive: darkTheme ? "#5F5F63" : "#6F6F71"
    readonly property color labelColor: "#FFFFFF"
    readonly property color mutedColor: "#ECECF0"
    readonly property color darkText: darkTheme ? "#F5F5F7" : "#1D1D1F"
    readonly property color subtleText: darkTheme ? "#D1D1D6" : "#6E6E73"
    readonly property color bubbleOff: darkTheme ? "#FFFFFF2A" : "#F2F2F4"
    readonly property color bubbleOn: "#0A84FF"
    readonly property color sliderTrack: darkTheme ? "#FFFFFF38" : "#A6A6AA"
    readonly property color sliderFill: "#F2F2F4"
    readonly property bool controlDragActive: draggingControlId.length > 0

    onEditingChanged: {
        if (!editing)
            endControlDrag()
    }

    onClosed: endControlDrag()

    function cloneControl(control) {
        return { "id": control.id, "visible": control.visible }
    }

    function defaultLayout() {
        var controls = []
        for (var i = 0; i < root.defaultControlIds.length; ++i)
            controls.push({ "id": root.defaultControlIds[i], "visible": true })
        return controls
    }

    function buildEditSlots() {
        var slots = []
        for (var i = 0; i < root.hiddenControls.length; ++i)
            slots.push({ "id": root.hiddenControls[i].id, "empty": false })
        var minimumSlots = 8
        var remainder = slots.length % 4
        var target = Math.max(minimumSlots, slots.length + (remainder === 0 ? 0 : 4 - remainder))
        while (slots.length < target)
            slots.push({ "id": "", "empty": true })
        return slots
    }

    function normalizeLayout(rawControls) {
        var controls = []
        var seen = {}
        if (rawControls && rawControls.length !== undefined) {
            for (var i = 0; i < rawControls.length; ++i) {
                var id = rawControls[i].id
                if (root.defaultControlIds.indexOf(id) === -1 || seen[id])
                    continue
                seen[id] = true
                controls.push({ "id": id, "visible": rawControls[i].visible !== false })
            }
        }
        for (var j = 0; j < root.defaultControlIds.length; ++j) {
            var defaultId = root.defaultControlIds[j]
            if (seen[defaultId])
                continue
            if (defaultId === "appearance") {
                var cameraIndex = indexOfControl(controls, "camera")
                if (cameraIndex >= 0) {
                    controls.splice(cameraIndex + 1, 0, { "id": defaultId, "visible": true })
                    continue
                }
                var focusIndex = indexOfControl(controls, "focus")
                if (focusIndex >= 0) {
                    controls.splice(focusIndex, 0, { "id": defaultId, "visible": true })
                    continue
                }
            }
            if (!seen[defaultId])
                controls.push({ "id": defaultId, "visible": true })
        }
        return controls
    }

    function loadControlLayout() {
        var parsed = null
        var savedLayout = taskbarController.controlCenterLayout()
        if (savedLayout.length > 0) {
            try {
                parsed = JSON.parse(savedLayout)
            } catch (error) {
                parsed = null
            }
        }
        root.controlLayout = normalizeLayout(parsed || defaultLayout())
        syncVisibleControlModel()
        if (visibleControlModel.count === 0) {
            root.controlLayout = normalizeLayout(defaultLayout())
            syncVisibleControlModel()
        }
        root.layoutLoaded = true
        saveControlLayout()
    }

    function saveControlLayout() {
        if (!root.layoutLoaded)
            return
        var packed = []
        for (var i = 0; i < root.controlLayout.length; ++i)
            packed.push(cloneControl(root.controlLayout[i]))
        taskbarController.setControlCenterLayout(JSON.stringify(packed))
    }

    function syncVisibleControlModel() {
        visibleControlModel.clear()
        for (var i = 0; i < root.controlLayout.length; ++i) {
            if (root.controlLayout[i].visible)
                visibleControlModel.append({ "controlId": root.controlLayout[i].id })
        }
    }

    function visibleModelIds() {
        var ids = []
        for (var i = 0; i < visibleControlModel.count; ++i)
            ids.push(visibleControlModel.get(i).controlId)
        return ids
    }

    function applyVisibleOrder(visibleIds) {
        var byId = {}
        var hidden = []
        for (var i = 0; i < root.controlLayout.length; ++i) {
            var control = cloneControl(root.controlLayout[i])
            if (control.visible)
                byId[control.id] = control
            else
                hidden.push(control)
        }

        var next = []
        for (var j = 0; j < visibleIds.length; ++j) {
            var id = visibleIds[j]
            if (byId[id])
                next.push(byId[id])
        }
        for (var k = 0; k < hidden.length; ++k)
            next.push(hidden[k])

        root.controlLayout = next
        root.dragLayoutDirty = true
    }

    function registerControlShell(shell) {
        var shells = root.controlShells.slice()
        if (shells.indexOf(shell) === -1) {
            shells.push(shell)
            root.controlShells = shells
        }
    }

    function unregisterControlShell(shell) {
        var shells = root.controlShells.slice()
        var index = shells.indexOf(shell)
        if (index >= 0) {
            shells.splice(index, 1)
            root.controlShells = shells
        }
    }

    function shellForControl(controlId) {
        for (var i = 0; i < root.controlShells.length; ++i) {
            var shell = root.controlShells[i]
            if (shell && shell.controlId === controlId)
                return shell
        }
        return null
    }

    function shellAtPoint(contentX, contentY, exceptControlId) {
        for (var i = root.controlShells.length - 1; i >= 0; --i) {
            var shell = root.controlShells[i]
            if (!shell || !shell.visible || shell.controlId === exceptControlId)
                continue
            var position = shell.mapToItem(root.contentItem, 0, 0)
            if (contentX >= position.x && contentX <= position.x + shell.width
                    && contentY >= position.y && contentY <= position.y + shell.height) {
                return shell
            }
        }
        return null
    }

    function pointHitsRemoveButton(shell, contentX, contentY) {
        if (!shell)
            return false
        var position = shell.mapToItem(root.contentItem, 0, 0)
        return contentX >= position.x - 9 && contentX <= position.x + 21
                && contentY >= position.y - 9 && contentY <= position.y + 21
    }

    function beginControlDrag(controlId, pressContentX, pressContentY) {
        var shell = shellForControl(controlId)
        if (!shell)
            return
        var position = shell.mapToItem(root.contentItem, 0, 0)
        root.draggingControlId = controlId
        root.dragGhostWidth = shell.width
        root.dragGhostHeight = shell.height
        root.dragGhostOffsetX = pressContentX - position.x
        root.dragGhostOffsetY = pressContentY - position.y
        root.dragGhostX = position.x
        root.dragGhostY = position.y
        root.dragLayoutDirty = false
    }

    function updateControlDrag(contentX, contentY) {
        if (!root.controlDragActive)
            return
        root.dragGhostX = contentX - root.dragGhostOffsetX
        root.dragGhostY = contentY - root.dragGhostOffsetY
        reorderControlAt(root.dragGhostX + root.dragGhostWidth / 2,
                         root.dragGhostY + root.dragGhostHeight / 2)
    }

    function endControlDrag() {
        if (!root.controlDragActive)
            return
        root.draggingControlId = ""
        root.dropTargetControlId = ""
        root.dragGhostWidth = 0
        root.dragGhostHeight = 0
        if (root.dragLayoutDirty) {
            root.dragLayoutDirty = false
            saveControlLayout()
        }
    }

    function reorderControlAt(contentX, contentY) {
        var targetShell = shellAtPoint(contentX, contentY, root.draggingControlId)
        if (!targetShell) {
            root.dropTargetControlId = ""
            return
        }
        root.dropTargetControlId = targetShell.controlId
        var targetCenter = targetShell.mapToItem(root.contentItem,
                                                targetShell.width / 2,
                                                targetShell.height / 2)
        var beforeTarget = contentY < targetCenter.y - 8
                || (Math.abs(contentY - targetCenter.y) <= 8 && contentX < targetCenter.x)
        moveControlRelative(root.draggingControlId, targetShell.controlId, beforeTarget)
    }

    function moveControlRelative(controlId, targetId, beforeTarget) {
        if (controlId.length === 0 || targetId.length === 0 || controlId === targetId)
            return
        var ids = visibleModelIds()
        var from = ids.indexOf(controlId)
        var target = ids.indexOf(targetId)
        if (from < 0 || target < 0)
            return
        var moving = ids.splice(from, 1)[0]
        target = ids.indexOf(targetId)
        var insertAt = beforeTarget ? target : target + 1
        ids.splice(insertAt, 0, moving)
        var newIndex = ids.indexOf(controlId)
        if (newIndex < 0 || newIndex === from)
            return

        visibleControlModel.move(from, newIndex, 1)
        applyVisibleOrder(ids)
    }

    function indexOfControl(controls, controlId) {
        for (var i = 0; i < controls.length; ++i) {
            if (controls[i].id === controlId)
                return i
        }
        return -1
    }

    function hideControl(controlId) {
        var next = root.controlLayout.map(function(control) { return cloneControl(control) })
        var index = indexOfControl(next, controlId)
        if (index < 0)
            return
        next[index].visible = false
        root.controlLayout = next
        syncVisibleControlModel()
        saveControlLayout()
    }

    function showControl(controlId) {
        var next = root.controlLayout.map(function(control) { return cloneControl(control) })
        var index = indexOfControl(next, controlId)
        if (index < 0)
            return
        var control = cloneControl(next[index])
        control.visible = true
        next.splice(index, 1)
        next.push(control)
        root.controlLayout = next
        syncVisibleControlModel()
        saveControlLayout()
    }

    function controlTitle(controlId) {
        switch (controlId) {
        case "wifi": return qsTr("Wi-Fi")
        case "bluetooth": return qsTr("Bluetooth")
        case "airdrop": return qsTr("AirDrop")
        case "media": return qsTr("Now Playing")
        case "keyboard": return qsTr("Keyboard")
        case "mirror": return qsTr("Screen Mirroring")
        case "camera": return qsTr("Camera")
        case "appearance": return qsTr("Appearance")
        case "focus": return qsTr("Focus")
        case "display": return qsTr("Display")
        case "sound": return qsTr("Sound")
        default: return controlId
        }
    }

    function controlGlyph(controlId) {
        switch (controlId) {
        case "bluetooth": return "bluetooth"
        case "airdrop": return "airdrop"
        case "media": return "play"
        case "keyboard": return "keyboard"
        case "mirror": return "mirror"
        case "camera": return "camera"
        case "appearance": return "appearance"
        case "focus": return "focus"
        case "display": return "sun"
        case "sound": return "speaker"
        default: return "wifi"
        }
    }

    function controlColumnSpan(controlId) {
        if (controlId === "display" || controlId === "sound")
            return 4
        if (controlId === "focus")
            return 3
        if (controlId === "keyboard" || controlId === "mirror" || controlId === "camera" || controlId === "appearance")
            return 1
        return 2
    }

    function controlRowSpan(controlId) {
        return controlId === "media" ? 2 : 1
    }

    function controlHeight(controlId) {
        if (controlId === "media")
            return 144
        if (controlId === "display" || controlId === "sound")
            return 76
        if (controlId === "focus" || controlId === "keyboard" || controlId === "mirror" || controlId === "camera" || controlId === "appearance")
            return 62
        return 68
    }

    function componentForControl(controlId) {
        switch (controlId) {
        case "wifi": return wifiControlComponent
        case "bluetooth": return bluetoothControlComponent
        case "airdrop": return airDropControlComponent
        case "media": return mediaControlComponent
        case "keyboard": return keyboardControlComponent
        case "mirror": return mirrorControlComponent
        case "camera": return cameraControlComponent
        case "appearance": return appearanceControlComponent
        case "focus": return focusControlComponent
        case "display": return displayControlComponent
        case "sound": return soundControlComponent
        default: return null
        }
    }

    Component.onCompleted: loadControlLayout()

    ListModel {
        id: visibleControlModel
    }

    background: Item {
        implicitWidth: root.width
        implicitHeight: root.implicitHeight

        Rectangle {
            anchors.fill: parent
            radius: 30
            color: root.panelBg
            border.width: root.darkTheme ? 1 : 0
            border.color: root.darkTheme ? "#FFFFFF24" : "transparent"

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: "#4A000000"
                shadowBlur: 1.0
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 12
                autoPaddingEnabled: true
            }
        }
    }

    component CcGlyph : Canvas {
        id: glyph
        property string kind: "wifi"
        property color glyphColor: "#FFFFFF"

        width: 20
        height: 20
        onKindChanged: requestPaint()
        onGlyphColorChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = glyphColor
            ctx.fillStyle = glyphColor
            ctx.lineWidth = 1.45
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            if (kind === "wifi") {
                function arc(r, a, b) {
                    ctx.beginPath()
                    ctx.arc(10, 15, r, a, b)
                    ctx.stroke()
                }
                arc(3, Math.PI * 1.18, Math.PI * 1.82)
                arc(6, Math.PI * 1.22, Math.PI * 1.78)
                arc(9, Math.PI * 1.27, Math.PI * 1.73)
                ctx.beginPath()
                ctx.arc(10, 15, 1.35, 0, Math.PI * 2)
                ctx.fill()
            } else if (kind === "bluetooth") {
                ctx.beginPath()
                ctx.moveTo(10, 2)
                ctx.lineTo(10, 18)
                ctx.lineTo(15, 13)
                ctx.lineTo(6, 7)
                ctx.lineTo(10, 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(6, 13)
                ctx.lineTo(15, 7)
                ctx.stroke()
            } else if (kind === "airdrop") {
                ctx.beginPath()
                ctx.arc(10, 10, 7, Math.PI * 1.1, Math.PI * 1.9)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(10, 10, 4.3, Math.PI * 1.1, Math.PI * 1.9)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(10, 13, 1.5, 0, Math.PI * 2)
                ctx.fill()
            } else if (kind === "focus") {
                ctx.beginPath()
                ctx.arc(10, 10, 7, Math.PI * 0.18, Math.PI * 1.82)
                ctx.arc(13, 8, 7, Math.PI * 1.75, Math.PI * 0.25, true)
                ctx.closePath()
                ctx.fill()
            } else if (kind === "mirror") {
                ctx.strokeRect(2.5, 5, 11, 8)
                ctx.strokeRect(7, 8, 11, 8)
            } else if (kind === "keyboard") {
                ctx.strokeRect(2.5, 5, 15, 10)
                for (var k = 0; k < 4; ++k) {
                    ctx.beginPath()
                    ctx.moveTo(5 + k * 3, 8)
                    ctx.lineTo(6 + k * 3, 8)
                    ctx.stroke()
                }
                ctx.beginPath()
                ctx.moveTo(6, 12)
                ctx.lineTo(14, 12)
                ctx.stroke()
            } else if (kind === "camera") {
                ctx.strokeRect(4, 7, 12, 8)
                ctx.beginPath()
                ctx.arc(10, 11, 2.4, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(7, 7)
                ctx.lineTo(8, 5)
                ctx.lineTo(12, 5)
                ctx.lineTo(13, 7)
                ctx.stroke()
            } else if (kind === "appearance") {
                ctx.beginPath()
                ctx.arc(10, 10, 7, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(10, 10, 4.4, Math.PI * 0.5, Math.PI * 1.5)
                ctx.arc(10, 10, 4.4, Math.PI * 1.5, Math.PI * 0.5, true)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.arc(10, 10, 7, Math.PI * 0.5, Math.PI * 1.5)
                ctx.stroke()
            } else if (kind === "sun") {
                ctx.beginPath()
                ctx.arc(10, 10, 3.3, 0, Math.PI * 2)
                ctx.stroke()
                for (var i = 0; i < 8; ++i) {
                    var a = i * Math.PI / 4
                    ctx.beginPath()
                    ctx.moveTo(10 + Math.cos(a) * 5.5, 10 + Math.sin(a) * 5.5)
                    ctx.lineTo(10 + Math.cos(a) * 8.0, 10 + Math.sin(a) * 8.0)
                    ctx.stroke()
                }
            } else if (kind === "speaker") {
                ctx.beginPath()
                ctx.moveTo(3, 12)
                ctx.lineTo(6, 12)
                ctx.lineTo(10, 16)
                ctx.lineTo(10, 4)
                ctx.lineTo(6, 8)
                ctx.lineTo(3, 8)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.arc(11, 10, 4, Math.PI * 1.75, Math.PI * 0.25)
                ctx.stroke()
            } else if (kind === "airplay") {
                ctx.strokeRect(3, 4, 14, 10)
                ctx.beginPath()
                ctx.moveTo(10, 11)
                ctx.lineTo(6, 17)
                ctx.lineTo(14, 17)
                ctx.closePath()
                ctx.fill()
            } else if (kind === "play") {
                ctx.beginPath()
                ctx.moveTo(7, 4)
                ctx.lineTo(16, 10)
                ctx.lineTo(7, 16)
                ctx.closePath()
                ctx.fill()
            } else if (kind === "prev") {
                ctx.beginPath()
                ctx.moveTo(5, 10)
                ctx.lineTo(12, 5)
                ctx.lineTo(12, 15)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(12, 10)
                ctx.lineTo(18, 5)
                ctx.lineTo(18, 15)
                ctx.closePath()
                ctx.fill()
            } else if (kind === "next") {
                ctx.beginPath()
                ctx.moveTo(15, 10)
                ctx.lineTo(8, 5)
                ctx.lineTo(8, 15)
                ctx.closePath()
                ctx.fill()
                ctx.beginPath()
                ctx.moveTo(8, 10)
                ctx.lineTo(2, 5)
                ctx.lineTo(2, 15)
                ctx.closePath()
                ctx.fill()
            }
        }
        Component.onCompleted: requestPaint()
    }

    component IconBubble : Rectangle {
        id: bubble
        property string kind: "wifi"
        property bool checked: false
        property int bubbleSize: 36

        width: bubbleSize
        height: bubbleSize
        radius: bubbleSize / 2
        color: checked ? root.bubbleOn : root.bubbleOff

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        CcGlyph {
            anchors.centerIn: parent
            kind: bubble.kind
            glyphColor: bubble.checked ? "#FFFFFF" : (root.darkTheme ? "#F5F5F7" : "#7A7A7E")
        }
    }

    component EditDotButton : Rectangle {
        id: editButton
        signal clicked()

        property string label: "-"
        property bool enabledButton: true
        property color buttonColor: "#FFFFFF"
        property color labelColor: "#1D1D1F"

        width: 24
        height: 24
        radius: 12
        color: enabledButton ? buttonColor : "#9A9A9E"
        opacity: enabledButton ? 1 : 0.45
        border.width: 1
        border.color: "#00000020"

        Behavior on color {
            ColorAnimation { duration: 140; easing.type: Easing.OutCubic }
        }

        Behavior on opacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        Text {
            anchors.centerIn: parent
            text: editButton.label
            color: editButton.labelColor
            font.pixelSize: 14
            font.weight: Font.Bold
        }

        MouseArea {
            anchors.fill: parent
            enabled: editButton.enabledButton
            cursorShape: editButton.enabledButton ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: editButton.clicked()
        }
    }

    component EditOverlay : Item {
        id: overlay
        property string controlId: ""

        anchors.fill: parent
        visible: root.editing

        EditDotButton {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: -7
            z: 20
            label: "-"
            buttonColor: "#FF453A"
            labelColor: "#FFFFFF"
            onClicked: root.hideControl(overlay.controlId)
        }
    }

    component ControlPill : Rectangle {
        id: pill
        signal toggled()

        property string title: ""
        property string subtitle: ""
        property string kind: "wifi"
        property bool checked: false

        radius: height / 2
        color: mouse.containsMouse && !root.editing ? root.tileBgHover : (checked ? root.tileBgActive : root.tileBg)

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 13
            anchors.rightMargin: 13
            spacing: 10

            IconBubble {
                kind: pill.kind
                checked: pill.checked
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                    Layout.fillWidth: true
                    text: pill.title
                    color: root.labelColor
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: pill.subtitle
                    color: root.mutedColor
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: root.editing ? Qt.NoButton : Qt.LeftButton
            cursorShape: root.editing ? Qt.ArrowCursor : Qt.PointingHandCursor
            onClicked: {
                if (!root.editing)
                    pill.toggled()
            }
        }
    }

    component RoundAction : Rectangle {
        id: action
        signal toggled()

        property string kind: "keyboard"
        property bool checked: false

        radius: Math.min(width, height) / 2
        color: mouse.containsMouse && !root.editing ? root.tileBgHover : (checked ? root.tileBgActive : root.tileBg)

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        IconBubble {
            anchors.centerIn: parent
            kind: action.kind
            checked: action.checked
            bubbleSize: 34
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: root.editing ? Qt.NoButton : Qt.LeftButton
            cursorShape: root.editing ? Qt.ArrowCursor : Qt.PointingHandCursor
            onClicked: {
                if (!root.editing)
                    action.toggled()
            }
        }
    }

    component FocusPill : Rectangle {
        id: focus
        signal toggled()

        radius: height / 2
        color: mouse.containsMouse && !root.editing ? root.tileBgHover : (root.focusOn ? root.tileBgActive : root.tileBg)

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 13
            anchors.rightMargin: 14
            spacing: 11

            IconBubble {
                kind: "focus"
                checked: root.focusOn
                bubbleSize: 34
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Focus")
                color: root.labelColor
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: root.editing ? Qt.NoButton : Qt.LeftButton
            cursorShape: root.editing ? Qt.ArrowCursor : Qt.PointingHandCursor
            onClicked: {
                if (!root.editing)
                    focus.toggled()
            }
        }
    }

    component MediaTile : Rectangle {
        id: media
        radius: 32
        color: mouse.containsMouse && !root.editing ? root.tileBgHover : root.tileBg

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                    radius: 14
                    color: root.darkTheme ? "#FFFFFF22" : "#8F8F92"
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Not Playing")
                    color: root.labelColor
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Item { Layout.fillWidth: true }

                CcGlyph {
                    kind: "prev"
                    glyphColor: root.mutedColor
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                }
                CcGlyph {
                    kind: "play"
                    glyphColor: root.labelColor
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                }
                CcGlyph {
                    kind: "next"
                    glyphColor: root.mutedColor
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 22
                }
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }
    }

    component SliderBar : Item {
        id: slider
        signal moved(real value)

        property real value: 0.5
        property string kind: "sun"

        implicitHeight: 22

        Rectangle {
            id: track
            anchors.fill: parent
            radius: height / 2
            color: root.sliderTrack

            Rectangle {
                height: parent.height
                width: Math.max(track.height, slider.value * track.width)
                radius: height / 2
                color: root.sliderFill

                Behavior on width {
                    NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
                }
            }

            CcGlyph {
                anchors.left: parent.left
                anchors.leftMargin: 7
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                kind: slider.kind
                glyphColor: root.darkText
            }
        }

        MouseArea {
            anchors.fill: parent
            enabled: !root.editing
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onPressed: function(mouse) { setFromX(mouse.x) }
            onPositionChanged: function(mouse) {
                if (pressed)
                    setFromX(mouse.x)
            }

            function setFromX(mouseX) {
                slider.value = Math.max(0, Math.min(1, mouseX / Math.max(1, track.width)))
                slider.moved(slider.value)
            }
        }
    }

    component SliderTile : Rectangle {
        id: tile
        property string title: ""
        property string kind: "sun"
        property real value: 0.5
        signal moved(real value)

        radius: 24
        color: root.darkTheme ? "#FFFFFF1C" : root.tileBg

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 15
            anchors.rightMargin: 15
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            spacing: 9

            Text {
                Layout.fillWidth: true
                text: tile.title
                color: root.labelColor
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            SliderBar {
                Layout.fillWidth: true
                kind: tile.kind
                value: tile.value
                onMoved: function(newValue) { tile.moved(newValue) }
            }
        }
    }

    component ControlShell : Item {
        id: shell
        required property string controlId

        Layout.columnSpan: root.controlColumnSpan(controlId)
        Layout.rowSpan: root.controlRowSpan(controlId)
        Layout.fillWidth: true
        Layout.preferredHeight: root.controlHeight(controlId)
        opacity: root.draggingControlId === controlId ? 0.32 : 1
        scale: dragMouse.pressed && !root.controlDragActive ? 0.992 : 1

        Loader {
            anchors.fill: parent
            sourceComponent: root.componentForControl(shell.controlId)
        }

        Rectangle {
            anchors.fill: parent
            radius: Math.min(width, height) / 2
            color: root.darkTheme ? "#FFFFFF18" : "#FFFFFF42"
            border.width: 1
            border.color: root.darkTheme ? "#FFFFFF40" : "#FFFFFFAA"
            opacity: root.editing && root.dropTargetControlId === shell.controlId ? 1 : 0
            z: 8

            Behavior on opacity {
                NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
            }
        }

        MouseArea {
            id: dragMouse
            anchors.fill: parent
            z: 12
            enabled: root.editing
            acceptedButtons: Qt.LeftButton
            hoverEnabled: true
            cursorShape: pressed || root.draggingControlId === shell.controlId ? Qt.ClosedHandCursor : Qt.OpenHandCursor

            property bool dragStarted: false
            property real pressContentX: 0
            property real pressContentY: 0

            onPressed: function(mouse) {
                var point = shell.mapToItem(root.contentItem, mouse.x, mouse.y)
                pressContentX = point.x
                pressContentY = point.y
                dragStarted = false
                mouse.accepted = true
            }

            onPositionChanged: function(mouse) {
                if (!pressed)
                    return
                var point = shell.mapToItem(root.contentItem, mouse.x, mouse.y)
                var dx = point.x - pressContentX
                var dy = point.y - pressContentY
                if (!dragStarted && Math.sqrt(dx * dx + dy * dy) >= 6) {
                    dragStarted = true
                    root.beginControlDrag(shell.controlId, pressContentX, pressContentY)
                }
                if (dragStarted && root.draggingControlId === shell.controlId)
                    root.updateControlDrag(point.x, point.y)
            }

            onReleased: function(mouse) {
                if (dragStarted && root.draggingControlId === shell.controlId)
                    root.endControlDrag()
                dragStarted = false
            }

            onCanceled: {
                if (dragStarted && root.draggingControlId === shell.controlId)
                    root.endControlDrag()
                dragStarted = false
            }
        }

        EditOverlay {
            controlId: shell.controlId
            z: 30
        }

        Behavior on x {
            enabled: root.editing && root.draggingControlId !== shell.controlId
            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
        }

        Behavior on y {
            enabled: root.editing && root.draggingControlId !== shell.controlId
            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
        }

        Behavior on opacity {
            NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
        }

        Behavior on scale {
            NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
        }

        Component.onCompleted: root.registerControlShell(shell)
        Component.onDestruction: root.unregisterControlShell(shell)
    }

    component EditSlot : Rectangle {
        id: slot
        required property string controlId
        required property bool empty

        Layout.preferredWidth: 64
        Layout.preferredHeight: 64
        radius: 32
        color: root.darkTheme ? "#FFFFFF20" : "#B8B8BA"
        opacity: empty ? 0.82 : 1

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        Behavior on opacity {
            NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
        }

        CcGlyph {
            anchors.centerIn: parent
            width: 22
            height: 22
            visible: !slot.empty
            kind: root.controlGlyph(slot.controlId)
            glyphColor: root.darkTheme ? "#F5F5F7" : "#6E6E73"
        }

        MouseArea {
            anchors.fill: parent
            enabled: !slot.empty
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: root.showControl(slot.controlId)
        }

        EditDotButton {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: -4
            z: 2
            visible: !slot.empty
            label: "+"
            buttonColor: "#30D158"
            labelColor: "#FFFFFF"
            onClicked: root.showControl(slot.controlId)
        }
    }

    component EditButton : Rectangle {
        id: edit
        signal clicked()

        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: root.editing ? 72 : 96
        Layout.preferredHeight: 30
        radius: 15
        color: mouse.containsMouse ? (root.darkTheme ? "#FFFFFF30" : "#00000024")
                                  : (root.darkTheme ? "#FFFFFF22" : "#00000018")

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        Text {
            anchors.centerIn: parent
            text: root.editing ? qsTr("Done") : qsTr("Edit Controls")
            color: root.darkText
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: edit.clicked()
        }
    }

    Component {
        id: wifiControlComponent
        ControlPill {
            title: qsTr("Wi-Fi")
            subtitle: root.wifiOn ? qsTr("Connected") : qsTr("Not Connected")
            kind: "wifi"
            checked: root.wifiOn
            onToggled: root.wifiOn = !root.wifiOn
        }
    }

    Component {
        id: bluetoothControlComponent
        ControlPill {
            title: qsTr("Bluetooth")
            subtitle: root.bluetoothOn ? qsTr("On") : qsTr("Off")
            kind: "bluetooth"
            checked: root.bluetoothOn
            onToggled: root.bluetoothOn = !root.bluetoothOn
        }
    }

    Component {
        id: airDropControlComponent
        ControlPill {
            title: qsTr("AirDrop")
            subtitle: root.airDropOn ? qsTr("Everyone") : qsTr("Off")
            kind: "airdrop"
            checked: root.airDropOn
            onToggled: root.airDropOn = !root.airDropOn
        }
    }

    Component {
        id: mediaControlComponent
        MediaTile {}
    }

    Component {
        id: keyboardControlComponent
        RoundAction {
            kind: "keyboard"
        }
    }

    Component {
        id: mirrorControlComponent
        RoundAction {
            kind: "mirror"
        }
    }

    Component {
        id: cameraControlComponent
        RoundAction {
            kind: "camera"
        }
    }

    Component {
        id: appearanceControlComponent
        RoundAction {
            kind: "appearance"
            checked: taskbarController.darkTheme
            onToggled: taskbarController.setAppearanceMode(taskbarController.darkTheme ? "light" : "dark")
        }
    }

    Component {
        id: focusControlComponent
        FocusPill {
            onToggled: root.focusOn = !root.focusOn
        }
    }

    Component {
        id: displayControlComponent
        SliderTile {
            title: qsTr("Display")
            kind: "sun"
            value: root.displayValue
            onMoved: function(newValue) { root.displayValue = newValue }
        }
    }

    Component {
        id: soundControlComponent
        SliderTile {
            title: qsTr("Sound")
            kind: "speaker"
            value: root.soundValue
            onMoved: function(newValue) { root.soundValue = newValue }
        }
    }

    contentItem: Item {
        id: contentRoot
        implicitWidth: root.width - root.padding * 2
        implicitHeight: contentColumn.implicitHeight

        ColumnLayout {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            spacing: 10

            GridLayout {
                id: controlsGrid
                Layout.fillWidth: true
                columns: 4
                columnSpacing: 10
                rowSpacing: 10

                Repeater {
                    model: visibleControlModel
                    delegate: ControlShell {
                        required property int index
                        controlId: visibleControlModel.get(index).controlId
                    }
                }
            }

            GridLayout {
                Layout.alignment: Qt.AlignHCenter
                columns: 4
                columnSpacing: 12
                rowSpacing: 12
                visible: root.editing

                Repeater {
                    model: root.editSlots
                    delegate: EditSlot {
                        required property var modelData
                        controlId: modelData.id
                        empty: modelData.empty
                    }
                }
            }

            EditButton {
                onClicked: root.editing = !root.editing
            }
        }

        Item {
            id: dragGhost
            x: root.dragGhostX
            y: root.dragGhostY
            width: root.dragGhostWidth
            height: root.dragGhostHeight
            z: 800
            visible: root.controlDragActive
            opacity: visible ? 0.96 : 0
            scale: visible ? 1.035 : 1

            Behavior on opacity {
                NumberAnimation { duration: 110; easing.type: Easing.OutCubic }
            }

            Behavior on scale {
                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
            }

            Loader {
                anchors.fill: parent
                sourceComponent: root.componentForControl(root.draggingControlId)
            }

            layer.enabled: visible
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: "#44000000"
                shadowBlur: 0.9
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 10
                autoPaddingEnabled: true
            }
        }
    }
}
