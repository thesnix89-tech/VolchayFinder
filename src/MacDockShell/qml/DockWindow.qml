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
    property real dragLeftX: 0
    property real settleDragY: 0
    property bool reorderSettling: false
    property bool neighborsPacked: false
    // Vertical offset of the dragged icon; gap preview only when near the dock row.
    property real dragGripY: 0
    property bool reorderDragOverlayActive: false
    property string reorderDragIconUrl: ""
    property string reorderDragIconHint: ""
    property string reorderDragLabel: ""
    property bool reorderDragLiftMagnified: false
    property bool reorderDragFaded: false
    property bool dragInDockZone: false
    property bool dockZoneEverEntered: false
    // 0 = full dock, 1 = compact (lifted icon slot removed) — single driver for pill width.
    property real dockPackT: 0
    property int prevLayoutDragTo: -1
    property int layoutMorphFrom: -1
    property real layoutMorph: 1
    readonly property bool reordering: dragFrom >= 0 || reorderSettling
    property bool externalDropActive: false
    property bool externalPinPreview: false
    property int externalPinTo: 0
    property bool externalPinDropping: false
    property bool externalPinSettling: false
    property bool externalPinRetracting: false
    property bool externalPinCommitting: false
    property bool unpinCommitting: false
    // Frozen expanded slot count/width during model handoff — prevents one-frame pill shrink.
    property int externalPinCommitLayoutCount: -1
    property real externalPinFrozenPillWidth: -1
    // Suppress post-commit x slide for the icon that just replaced the ghost.
    property int externalPinHandoffIndex: -1
    property bool externalPinGhostVisible: false
    property real externalPinGhostLiftY: 0
    property string externalPinPath: ""
    property string externalPinIconUrl: ""
    property int prevLayoutExternalPinTo: -1
    // Continuous insertion index — glides between slots without snapping layoutMorph to 0.
    property real externalPinMorphSlot: 0
    property bool externalPinSlotSnapped: false
    // Screen edges captured when external pin preview opens (compact dock, before expand).
    property real externalPinAnchorLeftX: -1
    property real externalPinAnchorRightX: -1
    // After edge pin: keep the same screen anchor so resting dock matches preview width/position.
    property int externalPinRestEdge: 0
    property real externalPinRestAnchorLeftX: -1
    property real externalPinRestAnchorRightX: -1
    // Only one dock icon label at a time — fast sweeps must not stack tooltips.
    property int activeTooltipIndex: -1
    property bool hasDropPointer: false
    property real lastDropGlobalX: 0
    property real lastDropGlobalY: 0
    property real lastDropIconCenterGlobalX: 0
    property real lastDropIconCenterGlobalY: 0
    readonly property int dockSpacing: 14
    readonly property int dockStride: taskbarController.dockIconSize + dockSpacing
    readonly property int dockShrinkAnimMs: 220
    readonly property int reorderAnimMs: 360
    readonly property int dockPackAnimMs: 220
    readonly property int externalPinMorphMs: 320
    readonly property int externalPinSettleMs: 260
    readonly property int externalPinRestY: 19
    readonly property int externalPinGhostLiftMs: 280
    // macOS: insertion gap opens only when the icon is lowered back into the dock row.
    readonly property int dockZoneEnterY: -12
    readonly property int dockZoneLeaveY: -28
    // Extra headroom so a dragged icon can float above the dock pill (macOS-style).
    readonly property int dockTopAirspace: 128
    // Drag upward past this gripY threshold to enter unpin-preview state.
    readonly property int unpinThreshold: -80

    function slotLeftForIndex(index) {
        return hoverBleed + index * dockStride
    }

    // Full-width preview: one empty slot at `to` while the dragged icon hovers.
    function previewSlotForIndex(i, from, to) {
        if (i === from)
            return -1
        var compact = i > from ? i - 1 : i
        if (compact >= to)
            return compact + 1
        return compact
    }

    // External pin: insert a new slot at `to` — icons at/after `to` shift right.
    function previewSlotForInsert(i, to) {
        if (i >= to)
            return i + 1
        return i
    }

    function candidateInsertSlotForCenter(center, itemCount) {
        return candidateSlotForCenter(center, itemCount + 1)
    }

    function packedSlotForIndex(i, from) {
        if (from < 0)
            return i
        if (i === from)
            return -1
        if (i > from)
            return i - 1
        return i
    }

    function gapCenterForSlot(slot) {
        return slotLeftForIndex(slot) + taskbarController.dockIconSize / 2
    }

    // Midpoint between two neighboring slots — cursor past it picks the right slot.
    function insertionBoundaryBefore(slot) {
        if (slot <= 0)
            return -Infinity
        return (gapCenterForSlot(slot - 1) + gapCenterForSlot(slot)) / 2
    }

    function candidateSlotForCenter(center, count) {
        var candidate = 0
        for (var t = 1; t < count; t++) {
            if (center >= insertionBoundaryBefore(t))
                candidate = t
        }
        return Math.min(count - 1, Math.max(0, candidate))
    }

    function gapLeftForDragTo(to) {
        return slotLeftForIndex(to)
    }

    function insertSlotXForItem(itemIndex, slotReal) {
        var slotFloor = Math.floor(slotReal)
        var slotCeil = Math.ceil(slotReal)
        if (slotFloor === slotCeil)
            return slotLeftForIndex(previewSlotForInsert(itemIndex, slotFloor))
        var xFloor = slotLeftForIndex(previewSlotForInsert(itemIndex, slotFloor))
        var xCeil = slotLeftForIndex(previewSlotForInsert(itemIndex, slotCeil))
        return xFloor + (slotReal - slotFloor) * (xCeil - xFloor)
    }

    function externalPinInsertSlotX(slotReal) {
        var slotFloor = Math.floor(slotReal)
        var slotCeil = Math.ceil(slotReal)
        if (slotFloor === slotCeil)
            return slotLeftForIndex(slotFloor)
        var xFloor = slotLeftForIndex(slotFloor)
        var xCeil = slotLeftForIndex(slotCeil)
        return xFloor + (slotReal - slotFloor) * (xCeil - xFloor)
    }

    // Preview icon sits at the insertion gap — not at the compact trailing edge.
    function externalPinGhostX() {
        return externalPinInsertSlotX(externalPinMorphSlot)
    }

    function externalPinGhostLiftTarget(centerGlobalX, centerGlobalY) {
        var rowLocal = dockRowHost.mapFromGlobal(centerGlobalX, centerGlobalY)
        var iconSize = taskbarController.dockIconSize
        var restTopY = externalPinRestY
        var liftedTopY = rowLocal.y - iconSize / 2
        var rowCenterY = restTopY + iconSize / 2
        // Track the dragged icon vertically; ease into the dock row as it lowers.
        var enterBand = 56
        var seatedDy = -4
        var dy = rowLocal.y - rowCenterY
        if (dy >= seatedDy)
            return 0
        var floatLift = liftedTopY - restTopY
        if (dy <= -enterBand)
            return floatLift
        var t = (dy + enterBand) / (enterBand - seatedDy)
        t = Math.max(0, Math.min(1, t))
        t = t * t * (3 - 2 * t)
        return floatLift * (1 - t)
    }

    function syncExternalPinGhostLift(centerGlobalX, centerGlobalY) {
        externalPinGhostLiftAnim.stop()
        externalPinGhostLiftY = externalPinGhostLiftTarget(centerGlobalX, centerGlobalY)
    }

    function showExternalPinGhostIfReady() {
        if (!externalPinPreview || !hasDropPointer || externalPinIconUrl.length === 0)
            return
        externalPinSlotAnim.stop()
        externalPinMorphSlot = externalPinTo
        syncExternalPinGhostLift(lastDropIconCenterGlobalX, lastDropIconCenterGlobalY)
        externalPinGhostVisible = true
    }

    function snapExternalPinMorphSlot() {
        externalPinSlotAnim.stop()
        externalPinMorphSlot = externalPinTo
        externalPinSlotSnapped = true
        showExternalPinGhostIfReady()
    }

    function animateExternalPinMorphSlot(targetSlot) {
        externalPinSlotAnim.stop()
        externalPinSlotAnim.from = externalPinMorphSlot
        externalPinSlotAnim.to = targetSlot
        externalPinSlotAnim.start()
    }

    // Lifted icon follows the pointer; neighbors glide via layoutMorph / dockPackT.
    function visualDragLeftX() {
        return dragLeftX
    }

    function syncDockPackState() {
        if (dragFrom < 0 || reorderSettling)
            return
        if (dragInDockZone)
            animateDockPack(0)
        else if (neighborsPacked)
            animateDockPack(1)
    }

    function syncDragZone(gripY) {
        dragGripY = gripY
        var wasInZone = dragInDockZone
        if (dragInDockZone) {
            if (gripY < dockZoneLeaveY)
                dragInDockZone = false
        } else if (gripY > dockZoneEnterY) {
            dragInDockZone = true
            dockZoneEverEntered = true
        }
        if (wasInZone !== dragInDockZone)
            syncDockPackState()
    }

    // Boundaries between slots (pointer, not magnetized x) — avoids left-slot bias.
    function updateDragTarget(leftX, gripY, count, fromIndex) {
        if (fromIndex < 0)
            return

        syncDragZone(gripY)
        if (!dragInDockZone)
            return

        var iconSize = taskbarController.dockIconSize
        var dragCenter = leftX + iconSize / 2
        var candidate = candidateSlotForCenter(dragCenter, count)

        if (dragTo < 0) {
            dragTo = fromIndex
            return
        }

        if (candidate === dragTo)
            return

        if (Math.abs(candidate - dragTo) === 1) {
            var boundary = insertionBoundaryBefore(Math.max(candidate, dragTo))
            if (candidate > dragTo && dragCenter < boundary)
                return
            if (candidate < dragTo && dragCenter >= boundary)
                return
        }

        dragTo = candidate
        syncDockPackState()
    }

    function animateDockPack(target) {
        dockPackAnim.stop()
        dockPackAnim.to = target
        dockPackAnim.start()
    }

    function beginReorderSettle(from, to, releaseDragY) {
        dragTo = to
        reorderSettling = true
        _settleDoneX = false
        _settleDoneY = false
        animateDockPack(0)
        settleAnim.from = visualDragLeftX()
        settleAnim.to = slotLeftForIndex(to)
        settleAnim.start()
        settleDragY = releaseDragY
        settleYAnim.from = releaseDragY
        settleYAnim.to = 0
        settleYAnim.start()
    }

    function clearReorderState() {
        if (reorderSettling) {
            settleAnim.stop()
            settleYAnim.stop()
        }
        dockPackAnim.stop()
        dragFrom = -1
        dragTo = -1
        dragLeftX = 0
        settleDragY = 0
        reorderSettling = false
        neighborsPacked = false
        dragGripY = 0
        dragInDockZone = false
        dockZoneEverEntered = false
        prevLayoutDragTo = -1
        layoutMorphFrom = -1
        layoutMorph = 1
        dockModel.setReorderActive(false)
    }

    function cancelReorder() {
        clearReorderState()
        animateDockPack(0)
    }

    // Unpin: skip pack-back animation — model removal shrinks the dock once.
    function finishUnpin(index) {
        clearReorderState()
        clearActiveTooltip()
        dockPackT = 0
        unpinCommitting = true
        dockModel.unpinIndex(index)
        unpinCommitting = false
        clearExternalPinRestAnchor()
        syncAnimatedPillWidth(true)
        publishDropGeometry()
        Qt.callLater(dockWindow.refreshClickHitRegions)
    }

    property bool _settleDoneX: true
    property bool _settleDoneY: true

    NumberAnimation {
        id: settleAnim
        target: dockWindow
        property: "dragLeftX"
        duration: reorderAnimMs
        easing.type: Easing.OutCubic
        onFinished: {
            dockWindow._settleDoneX = true
            dockWindow.tryFinishReorderSettle()
        }
    }

    NumberAnimation {
        id: settleYAnim
        target: dockWindow
        property: "settleDragY"
        duration: reorderAnimMs
        easing.type: Easing.OutCubic
        onFinished: {
            dockWindow._settleDoneY = true
            dockWindow.tryFinishReorderSettle()
        }
    }

    function tryFinishReorderSettle() {
        if (!reorderSettling || !_settleDoneX || !_settleDoneY)
            return

        var from = dragFrom
        var to = dragTo
        reorderSettling = false
        dragFrom = -1
        dragTo = -1
        dragLeftX = 0
        settleDragY = 0
        neighborsPacked = false
        dragGripY = 0
        dragInDockZone = false
        dockZoneEverEntered = false
        prevLayoutDragTo = -1
        layoutMorphFrom = -1
        layoutMorph = 1
        dockPackAnim.stop()
        dockPackT = 0
        _settleDoneX = true
        _settleDoneY = true
        dockModel.setReorderActive(false)
        if (from >= 0 && to >= 0 && from !== to)
            dockModel.moveItem(from, to)
    }
    // macOS layout: a centered pill for icons plus transparent side wings so edge
    // tooltips and hover magnify stay centered on the icon instead of being clamped.
    readonly property int dockEndCap: 18
    readonly property int hoverBleed: Math.max(8, Math.ceil(taskbarController.dockIconSize * 0.12))
    readonly property int tooltipWing: 128
    readonly property int layoutDockCount: {
        if (externalPinCommitting && externalPinCommitLayoutCount >= 0)
            return externalPinCommitLayoutCount
        if (!externalPinPreview)
            return dockRepeater.count
        return dockRepeater.count + 1
    }
    readonly property real dockRowFullWidth: layoutDockCount * dockStride + 2 * hoverBleed
    readonly property real dockRowCompactWidth: (externalPinPreview || externalPinCommitting)
            ? Math.max(0, layoutDockCount - 1) * dockStride + 2 * hoverBleed
            : (reordering ? Math.max(0, dockRepeater.count - 1) : dockRepeater.count) * dockStride + 2 * hoverBleed
    readonly property real dockRowWidth: dockRowCompactWidth
            + (1 - dockPackT) * (dockRowFullWidth - dockRowCompactWidth)
    readonly property int dockPillWidth: dockRowWidth + 2 * dockEndCap
    // Single animated driver — window and pill stay in sync (no wing/pill desync).
    property real animatedPillWidth: dockPillWidth
    readonly property int dockWindowWidth: Math.min(
                Math.round(animatedPillWidth) + 2 * tooltipWing, Screen.width - 40)
    // Middle slot: grow symmetrically. Left insert: grow left (right edge fixed). Right append: grow right.
    readonly property real dockWindowX: {
        if (externalPinPreview && externalPinAnchorLeftX >= 0) {
            if (externalPinTo <= 0)
                return Math.round(externalPinAnchorRightX - dockWindowWidth)
            if (externalPinTo >= dockRepeater.count)
                return Math.round(externalPinAnchorLeftX)
            return Math.round((Screen.width - dockWindowWidth) / 2)
        }
        if (externalPinRestEdge !== 0 && externalPinRestAnchorLeftX >= 0) {
            if (externalPinRestEdge < 0)
                return Math.round(externalPinRestAnchorRightX - dockWindowWidth)
            return Math.round(externalPinRestAnchorLeftX)
        }
        return Math.round((Screen.width - dockWindowWidth) / 2)
    }
    width: dockWindowWidth
    height: taskbarController.dockIconSize + dockTopAirspace + dockBottomGap
    x: dockWindowX

    function syncAnimatedPillWidth(immediate) {
        if (externalPinCommitting && externalPinFrozenPillWidth >= 0) {
            dockChromeWidthAnim.stop()
            animatedPillWidth = externalPinFrozenPillWidth
            return
        }
        if (immediate || externalPinPreview || externalPinCommitting || unpinCommitting
                || dockPackAnim.running) {
            dockChromeWidthAnim.stop()
            animatedPillWidth = dockPillWidth
            return
        }
        if (Math.abs(animatedPillWidth - dockPillWidth) < 0.5) {
            animatedPillWidth = dockPillWidth
            return
        }
        dockChromeWidthAnim.stop()
        dockChromeWidthAnim.from = animatedPillWidth
        dockChromeWidthAnim.to = dockPillWidth
        dockChromeWidthAnim.start()
    }

    onDockPillWidthChanged: syncAnimatedPillWidth(false)

    NumberAnimation {
        id: dockPackAnim
        target: dockWindow
        property: "dockPackT"
        duration: dockWindow.dockPackAnimMs
        easing.type: Easing.OutCubic
        onFinished: {
            if (dockWindow.externalPinRetracting)
                dockWindow.clearExternalPinPreview()
            dockWindow.syncAnimatedPillWidth(true)
            dockWindow.refreshClickHitRegions()
        }
    }

    NumberAnimation {
        id: dockChromeWidthAnim
        target: dockWindow
        property: "animatedPillWidth"
        duration: dockWindow.dockPackAnimMs
        easing.type: Easing.OutCubic
    }

    onDragToChanged: {
        if (dragFrom >= 0 && dragInDockZone && prevLayoutDragTo >= 0 && dragTo !== prevLayoutDragTo) {
            layoutMorphFrom = prevLayoutDragTo
            layoutMorph = 0
            layoutMorphAnim.start()
        }
        prevLayoutDragTo = dragTo
        syncDockPackState()
    }

    onExternalPinToChanged: {
        if (!externalPinPreview || externalPinDropping || externalPinSettling)
            return
        // While the ghost is still hidden, track the slot silently so the first
        // paint is already under the cursor (no slide from the trailing edge).
        if (!externalPinGhostVisible) {
            externalPinSlotAnim.stop()
            externalPinMorphSlot = externalPinTo
            prevLayoutExternalPinTo = externalPinTo
            showExternalPinGhostIfReady()
            publishDropGeometry()
            return
        }
        if (externalPinSlotSnapped
                && prevLayoutExternalPinTo >= 0
                && externalPinTo !== prevLayoutExternalPinTo)
            animateExternalPinMorphSlot(externalPinTo)
        prevLayoutExternalPinTo = externalPinTo
        publishDropGeometry()
    }

    function iconCenterGlobalFromDrag(drag, localX, localY) {
        var iconSize = taskbarController.dockIconSize
        var hotSpotX = drag.hotSpot !== undefined ? drag.hotSpot.x : iconSize / 2
        var hotSpotY = drag.hotSpot !== undefined ? drag.hotSpot.y : iconSize / 2
        return dockDropArea.mapToGlobal(localX - hotSpotX + iconSize / 2,
                                        localY - hotSpotY + iconSize / 2)
    }

    function updateExternalPinTargetFromIconCenter(centerGlobalX, centerGlobalY) {
        var rowLocal = dockRowHost.mapFromGlobal(centerGlobalX, centerGlobalY)
        var dragCenter = rowLocal.x
        var candidate = candidateInsertSlotForCenter(dragCenter, dockRepeater.count)

        if (externalPinSlotSnapped && prevLayoutExternalPinTo >= 0 && candidate !== externalPinTo) {
            if (Math.abs(candidate - externalPinTo) === 1) {
                var boundary = insertionBoundaryBefore(Math.max(candidate, externalPinTo))
                if (candidate > externalPinTo && dragCenter < boundary)
                    return
                if (candidate < externalPinTo && dragCenter >= boundary)
                    return
            }
        }

        if (externalPinTo !== candidate)
            externalPinTo = candidate

        if (!externalPinSlotSnapped)
            snapExternalPinMorphSlot()
        else if (externalPinGhostVisible)
            syncExternalPinGhostLift(centerGlobalX, centerGlobalY)
    }

    function setExternalPinPath(path) {
        if (externalPinPath === path)
            return
        externalPinPath = path
        externalPinIconUrl = path.length > 0 ? dockModel.externalIconUrlForPath(path) : ""
        if (externalPinSlotSnapped)
            showExternalPinGhostIfReady()
    }

    function clearExternalPinRestAnchor() {
        externalPinRestEdge = 0
        externalPinRestAnchorLeftX = -1
        externalPinRestAnchorRightX = -1
    }

    function beginExternalPinPreview() {
        if (externalPinPreview || reordering)
            return
        clearExternalPinRestAnchor()
        externalPinAnchorLeftX = dockWindow.x
        externalPinAnchorRightX = dockWindow.x + dockWindow.width
        windowEffects.setDockDropCapture(dockWindow, true)
        neighborsPacked = true
        dockModel.setReorderActive(true)
        layoutMorphFrom = -1
        layoutMorph = 1
        externalPinSlotSnapped = false
        externalPinGhostVisible = false
        externalPinMorphSlot = dockRepeater.count
        externalPinTo = dockRepeater.count
        prevLayoutExternalPinTo = externalPinTo
        externalPinPreview = true
        if (hasDropPointer)
            updateExternalPinTargetFromIconCenter(lastDropIconCenterGlobalX, lastDropIconCenterGlobalY)
        dockPackT = 1
        animatedPillWidth = dockPillWidth
        animateDockPack(0)
        publishDropGeometry()
        refreshClickHitRegions()
    }

    function beginExternalPinRetract() {
        if (!externalPinPreview || externalPinDropping || externalPinSettling)
            return
        externalPinRetracting = true
        externalPinGhostVisible = false
        externalPinGhostLiftAnim.stop()
        externalPinSlotAnim.stop()
        externalPinDropSettleAnim.stop()
        externalPinSettlePauseTimer.stop()
        dockPackAnim.stop()
        if (dockPackT >= 0.99) {
            clearExternalPinPreview()
            return
        }
        animateDockPack(1)
    }

    function cancelExternalPinPreview() {
        if (!externalPinPreview)
            return
        beginExternalPinRetract()
    }

    function resetExternalPinState() {
        externalPinRetracting = false
        externalPinAnchorLeftX = -1
        externalPinAnchorRightX = -1
        neighborsPacked = false
        layoutMorphFrom = -1
        layoutMorph = 1
        dockPackAnim.stop()
        dockChromeWidthAnim.stop()
        layoutMorphAnim.stop()
        externalPinSlotAnim.stop()
        dockPackT = 0
        externalDropActive = false
        hasDropPointer = false
        windowEffects.setDockDropCapture(dockWindow, false)
        dockModel.setReorderActive(false)
    }

    function endExternalPinPreview() {
        externalPinHandoffIndex = -1
        externalPinCommitLayoutCount = -1
        externalPinFrozenPillWidth = -1
        externalPinCommitting = false
        externalPinPreview = false
        externalPinDropping = false
        externalPinSettling = false
        externalPinGhostVisible = false
        externalPinGhostLiftY = 0
        externalPinGhostLiftAnim.stop()
        externalPinMorphSlot = 0
        externalPinSlotSnapped = false
        setExternalPinPath("")
        prevLayoutExternalPinTo = -1
        resetExternalPinState()
        publishDropGeometry()
        Qt.callLater(dockWindow.refreshClickHitRegions)
    }

    function resetDockChromeLayout() {
        if (externalPinPreview || externalPinDropping || externalPinSettling || unpinCommitting)
            return
        clearExternalPinRestAnchor()
        resetExternalPinState()
        syncAnimatedPillWidth(false)
        publishDropGeometry()
        Qt.callLater(dockWindow.refreshClickHitRegions)
    }

    function clearExternalPinPreview() {
        endExternalPinPreview()
    }

    function finishExternalPinDrop(path, index) {
        if (externalPinSettling)
            return
        externalPinDropping = true
        externalDropActive = false
        externalPinTo = index
        setExternalPinPath(path)
        layoutMorphAnim.stop()
        layoutMorph = 1
        layoutMorphFrom = -1
        prevLayoutExternalPinTo = index
        externalPinSlotAnim.stop()
        externalPinSettling = true
        externalPinGhostVisible = true
        externalPinGhostLiftAnim.stop()
        externalPinGhostLiftAnim.from = externalPinGhostLiftY
        externalPinGhostLiftAnim.to = 0
        externalPinGhostLiftAnim.start()
        if (Math.abs(externalPinMorphSlot - index) < 0.01) {
            externalPinMorphSlot = index
            externalPinSettlePauseTimer.restart()
        } else {
            externalPinDropSettleAnim.from = externalPinMorphSlot
            externalPinDropSettleAnim.to = index
            externalPinDropSettleAnim.restart()
        }
    }

    function commitExternalPinDrop() {
        var path = externalPinPath
        var index = externalPinTo
        var itemCount = dockRepeater.count
        var anchorLeft = externalPinAnchorLeftX
        var anchorRight = externalPinAnchorRightX
        // Keep preview width (N+1 slots) while the model catches up — no shrink/grow flicker.
        externalPinHandoffIndex = index
        externalPinCommitLayoutCount = itemCount + 1
        externalPinFrozenPillWidth = animatedPillWidth
        externalPinCommitting = true
        externalPinGhostVisible = false
        dockModel.pinPathAt(path, index)
        if (index <= 0 && anchorLeft >= 0) {
            externalPinRestEdge = -1
            externalPinRestAnchorLeftX = anchorLeft
            externalPinRestAnchorRightX = anchorRight
        } else if (index >= itemCount && anchorLeft >= 0) {
            externalPinRestEdge = 1
            externalPinRestAnchorLeftX = anchorLeft
            externalPinRestAnchorRightX = anchorRight
        } else {
            clearExternalPinRestAnchor()
        }
        externalPinPreview = false
        externalPinDropping = false
        externalPinSettling = false
        externalPinMorphSlot = 0
        externalPinSlotSnapped = false
        setExternalPinPath("")
        prevLayoutExternalPinTo = -1
        resetExternalPinState()
        syncAnimatedPillWidth(true)
        publishDropGeometry()
        Qt.callLater(function() {
            // Pin the new delegate at the ghost slot before re-enabling x Behavior.
            var item = dockRepeater.itemAt(index)
            if (item)
                item.x = dockWindow.slotLeftForIndex(index)
            externalPinCommitting = false
            externalPinCommitLayoutCount = -1
            externalPinFrozenPillWidth = -1
            externalPinHandoffIndex = -1
            dockWindow.syncAnimatedPillWidth(true)
            dockWindow.refreshClickHitRegions()
            if (item && item.playLaunchBounce)
                item.playLaunchBounce()
        })
    }

    Timer {
        id: externalDragLeaveTimer
        interval: 180
        repeat: false
        onTriggered: {
            if (!dockWindow.externalDropActive && !dockWindow.externalPinDropping
                    && !dockWindow.externalPinSettling)
                dockWindow.beginExternalPinRetract()
        }
    }

    Timer {
        id: reorderDragFadeTimer
        interval: 1000
        repeat: false
        onTriggered: dockWindow.reorderDragFaded = true
    }

    onReorderDragOverlayActiveChanged: {
        if (reorderDragOverlayActive) {
            reorderDragFaded = false
            if (taskbarController.dockDragFadeEnabled)
                reorderDragFadeTimer.restart()
            else
                reorderDragFadeTimer.stop()
        } else {
            reorderDragFadeTimer.stop()
            reorderDragFaded = false
        }
    }

    NumberAnimation {
        id: layoutMorphAnim
        target: dockWindow
        property: "layoutMorph"
        from: 0
        to: 1
        duration: dockWindow.reorderAnimMs
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: externalPinSlotAnim
        target: dockWindow
        property: "externalPinMorphSlot"
        duration: dockWindow.externalPinMorphMs
        easing.type: Easing.InOutCubic
    }

    NumberAnimation {
        id: externalPinGhostLiftAnim
        target: dockWindow
        property: "externalPinGhostLiftY"
        duration: dockWindow.externalPinGhostLiftMs
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: externalPinDropSettleAnim
        target: dockWindow
        property: "externalPinMorphSlot"
        duration: dockWindow.externalPinSettleMs
        easing.type: Easing.OutCubic
        onFinished: dockWindow.commitExternalPinDrop()
    }

    Timer {
        id: externalPinSettlePauseTimer
        interval: dockWindow.externalPinSettleMs
        repeat: false
        onTriggered: dockWindow.commitExternalPinDrop()
    }

    // Window x/width track dockPillWidth instantly — animated Behaviors desynced the pill from wings.
    // Resting position; slides fully below the screen when a fullscreen app is active.
    readonly property int dockRestY: Screen.height - height - 18 + dockBottomGap
    readonly property int dockHiddenY: Screen.height + 4
    property real dockSlideY: dockRestY
    readonly property real iconDevicePixelRatio: screen ? screen.devicePixelRatio : 1.0
    readonly property bool darkTheme: taskbarController.darkTheme
    readonly property int themeAnimMs: 320
    y: dockSlideY

    function dockSlideTargetY() {
        return (taskbarController.shellActive && taskbarController.dockAutoHidden)
                ? dockHiddenY : dockRestY
    }

    function animateDockSlide() {
        var target = dockSlideTargetY()
        if (Math.abs(dockSlideY - target) < 0.5) {
            dockSlideY = target
            refreshClickHitRegions()
            return
        }
        dockSlideAnim.to = target
        hitRegionRefreshDefer.stop()
        refreshClickHitRegions()
        dockSlideAnim.start()
    }
    visible: taskbarController.shellActive
    color: "transparent"
    title: "MacDockShellDock"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool

    // Window-local coords — C++ maps to native screen pixels via QWindow::mapToGlobal.
    function dockWindowLocalRect() {
        return Qt.rect(0, 0, dockWindow.width, dockWindow.height)
    }

    function pillLocalRect() {
        var pillPt = dockBg.mapToItem(dockWindow.contentItem, 0, 0)
        return Qt.rect(pillPt.x, pillPt.y, dockBg.width, dockBg.height)
    }

    // Clip rect slightly wider than the pill so animated growth does not square off the rounded caps.
    function pillClipLocalRect() {
        var pill = pillLocalRect()
        var capPad = dockBg.radius + 8
        return Qt.rect(pill.x - capPad, pill.y, pill.width + 2 * capPad, pill.height)
    }

    function appendIconHitRegions(list) {
        for (var i = 0; i < dockRepeater.count; ++i) {
            var item = dockRepeater.itemAt(i)
            if (item && item.localHitRect)
                list.push(item.localHitRect())
        }
    }

    function refreshClickHitRegions() {
        if (!dockWindow.visible)
            return

        var clipRegions = []
        var hitRegions = []
        var fullWin = dockWindow.dockWindowLocalRect()
        var pill = dockWindow.pillLocalRect()

        if (dockWindow.reordering || dockWindow.externalDropActive || dockWindow.externalPinPreview
                || dockSlideAnim.running) {
            clipRegions.push(fullWin)
            hitRegions.push(fullWin)
        } else {
            var dropBand = Qt.rect(0, pill.y - 6, dockWindow.width, pill.height + 24)
            hitRegions.push(dropBand)
            hitRegions.push(pill)
            dockWindow.appendIconHitRegions(hitRegions)
            if (dockWindow.someVisualOverflow()) {
                clipRegions.push(fullWin)
            } else {
                clipRegions.push(dockWindow.pillClipLocalRect())
                dockWindow.appendIconHitRegions(clipRegions)
            }
        }

        windowEffects.updateDockHitRegions(dockWindow, clipRegions, hitRegions)
    }

    // Defer hit-region refresh — synchronous SetWindowRgn on hover drops containsMouse.
    function requestHitRegionRefresh() {
        hitRegionRefreshDefer.restart()
    }

    function layoutChromeAnimating() {
        return dockPackAnim.running || reordering || externalPinPreview
                || externalPinDropping || externalPinSettling
    }

    function needsImmediateHitRegionRefresh() {
        return externalDropActive || externalPinPreview || reordering || dockPackAnim.running
                || dockSlideAnim.running
    }

    function scheduleClickHitRegionRefresh() {
        // Debounced refresh starves updates while width animates — clip stays narrow and squares the right cap.
        if (needsImmediateHitRegionRefresh()) {
            hitRegionRefreshDefer.stop()
            refreshClickHitRegions()
        } else {
            requestHitRegionRefresh()
        }
    }

    function showTooltipForIndex(index) {
        dockTooltipHideTimer.stop()
        if (activeTooltipIndex !== index) {
            activeTooltipIndex = index
            requestHitRegionRefresh()
        }
    }

    function scheduleHideTooltipForIndex(index) {
        if (activeTooltipIndex !== index)
            return
        dockTooltipHideTimer.pendingIndex = index
        dockTooltipHideTimer.restart()
    }

    function clearActiveTooltip() {
        dockTooltipHideTimer.stop()
        if (activeTooltipIndex >= 0) {
            activeTooltipIndex = -1
            requestHitRegionRefresh()
        }
    }

    function someVisualOverflow() {
        for (var i = 0; i < dockRepeater.count; ++i) {
            var item = dockRepeater.itemAt(i)
            if (item && item.hasVisualOverflow)
                return true
        }
        return false
    }

    onReorderingChanged: {
        if (reordering)
            clearActiveTooltip()
        scheduleClickHitRegionRefresh()
    }
    onExternalDropActiveChanged: {
        if (externalDropActive) {
            externalDragLeaveTimer.stop()
            if (externalPinRetracting) {
                externalPinRetracting = false
                dockPackAnim.stop()
                animateDockPack(0)
                if (hasDropPointer)
                    updateExternalPinTargetFromIconCenter(lastDropIconCenterGlobalX,
                                                          lastDropIconCenterGlobalY)
            } else if (!externalPinPreview) {
                beginExternalPinPreview()
            } else {
                animateDockPack(0)
            }
        } else if (!externalPinDropping && !externalPinSettling) {
            externalDragLeaveTimer.restart()
        }
    }

    function publishDropGeometry() {
        if (!dockWindow.visible)
            return
        var pillPt = dockBg.mapToItem(dockWindow.contentItem, 0, 0)
        windowEffects.updateDockDropLayout(
                    dockWindow,
                    pillPt.x + hoverBleed,
                    dockStride,
                    taskbarController.dockIconSize,
                    dockRepeater.count)
    }

    Connections {
        target: windowEffects
        function onDockDropHoverChanged(window, active) {
            if (window === dockWindow)
                dockWindow.externalDropActive = active
        }
        function onDockDropPointerChanged(window, globalX, globalY, iconCenterGlobalX, iconCenterGlobalY) {
            if (window !== dockWindow)
                return
            dockWindow.lastDropGlobalX = globalX
            dockWindow.lastDropGlobalY = globalY
            dockWindow.lastDropIconCenterGlobalX = iconCenterGlobalX
            dockWindow.lastDropIconCenterGlobalY = iconCenterGlobalY
            dockWindow.hasDropPointer = true
            if (dockWindow.externalPinPreview || dockWindow.externalDropActive)
                dockWindow.updateExternalPinTargetFromIconCenter(iconCenterGlobalX, iconCenterGlobalY)
        }
        function onDockExternalDragPathChanged(window, path) {
            if (window !== dockWindow)
                return
            dockWindow.setExternalPinPath(path)
        }
        function onDockExternalDrop(window, path, index) {
            if (window !== dockWindow)
                return

            // During external pin preview the QML side already tracks the visible
            // insertion slot under the cursor. Do not let the native/OLE fallback
            // index overwrite it on Drop: that is what made the app land at the
            // right edge while the plus placeholder was shown elsewhere.
            if (!dockWindow.externalPinPreview && index >= 0)
                dockWindow.externalPinTo = Math.max(0, Math.min(index, dockRepeater.count))

            dockWindow.setExternalPinPath(path)
            dockWindow.finishExternalPinDrop(path, dockWindow.externalPinTo)
        }
    }

    function armClickThrough() {
        windowEffects.enableDockClickThrough(dockWindow)
        refreshClickHitRegions()
        dockDropTarget.registerDockWindow(dockWindow)
        childHwndResyncTimer.restart()
    }

    DropArea {
        id: dockDropArea
        anchors.fill: parent
        z: 20000
        keys: ["text/uri-list"]

        onEntered: (drag) => {
            dockWindow.externalDropActive = true
            if (drag.hasUrls && drag.urls.length > 0)
                dockWindow.setExternalPinPath(drag.urls[0].toLocalFile())
            drag.accept(Qt.CopyAction)
        }
        onPositionChanged: (drag) => {
            if (!drag.contains(drag.x, drag.y))
                return
            drag.accept(Qt.CopyAction)
            const center = dockWindow.iconCenterGlobalFromDrag(drag, drag.x, drag.y)
            dockWindow.lastDropIconCenterGlobalX = center.x
            dockWindow.lastDropIconCenterGlobalY = center.y
            dockWindow.hasDropPointer = true
            dockWindow.updateExternalPinTargetFromIconCenter(center.x, center.y)
        }
        onExited: dockWindow.externalDropActive = false
        onDropped: (drop) => {
            if (!drop.hasUrls || drop.urls.length === 0) {
                dockWindow.externalDropActive = false
                return
            }
            if (drop.hasUrls && drop.urls.length > 0)
                dockWindow.setExternalPinPath(drop.urls[0].toLocalFile())
            dockWindow.finishExternalPinDrop(
                        drop.urls[0].toLocalFile(),
                        dockWindow.externalPinTo)
        }
    }

    Component.onCompleted: {
        dockSlideY = dockSlideTargetY()
        windowEffects.applyDockGlass(dockWindow)
        Qt.callLater(dockWindow.armClickThrough)
        publishDropGeometry()
    }

    Connections {
        target: dockModel
        function onModelReset() {
            if (!dockWindow.externalPinPreview && !dockWindow.externalPinDropping
                    && !dockWindow.externalPinSettling)
                dockWindow.resetDockChromeLayout()
            else
                dockWindow.publishDropGeometry()
        }
        function onRowsInserted() {
            dockWindow.publishDropGeometry()
        }
        function onRowsRemoved() {
            if (!dockWindow.externalPinPreview && !dockWindow.externalPinDropping
                    && !dockWindow.externalPinSettling)
                dockWindow.resetDockChromeLayout()
            else
                dockWindow.publishDropGeometry()
        }
    }

    Connections {
        target: taskbarController
        function onDockIconSizeChanged() { dockWindow.publishDropGeometry() }
        function onDockAutoHiddenChanged() { dockWindow.animateDockSlide() }
        function onShellActiveChanged() {
            if (!dockSlideAnim.running)
                dockWindow.dockSlideY = dockWindow.dockSlideTargetY()
        }
    }

    onVisibleChanged: {
        if (visible)
            Qt.callLater(dockWindow.armClickThrough)
        else
            windowEffects.clearDockHitRegions(dockWindow)
    }

    Connections {
        target: taskbarController
        function onShellActiveChanged() {
            if (taskbarController.shellActive)
                Qt.callLater(dockWindow.armClickThrough)
            else
                windowEffects.clearDockHitRegions(dockWindow)
        }
    }

    Timer {
        id: hitRegionRefreshDefer
        interval: 16
        repeat: false
        onTriggered: dockWindow.refreshClickHitRegions()
    }

    Timer {
        id: dockTooltipHideTimer
        interval: 120
        repeat: false
        property int pendingIndex: -1
        onTriggered: {
            if (dockWindow.activeTooltipIndex === pendingIndex)
                dockWindow.clearActiveTooltip()
            pendingIndex = -1
        }
    }

    Timer {
        id: clickThroughWarmupTimer
        interval: 250
        repeat: false
        onTriggered: dockWindow.armClickThrough()
    }

    // Qt may create the render child HWND after first layout.
    Timer {
        id: childHwndResyncTimer
        interval: 600
        repeat: false
        onTriggered: dockWindow.refreshClickHitRegions()
    }

    onWidthChanged: {
        scheduleClickHitRegionRefresh()
        if (!layoutChromeAnimating())
            clickThroughWarmupTimer.restart()
    }
    onHeightChanged: {
        if (!dockSlideAnim.running)
            dockSlideY = dockSlideTargetY()
        if (!layoutChromeAnimating())
            clickThroughWarmupTimer.restart()
    }
    onXChanged: scheduleClickHitRegionRefresh()
    onYChanged: scheduleClickHitRegionRefresh()
    onDockPackTChanged: {
        if (externalPinPreview || reordering) {
            animatedPillWidth = dockPillWidth
            if (externalPinPreview)
                publishDropGeometry()
        }
        if (!layoutChromeAnimating())
            clickThroughWarmupTimer.restart()
    }
    // macOS-style: glide the dock down/up when toggling fullscreen.
    NumberAnimation {
        id: dockSlideAnim
        target: dockWindow
        property: "dockSlideY"
        duration: 280
        easing.type: Easing.InOutCubic
        onRunningChanged: {
            if (!running)
                dockWindow.refreshClickHitRegions()
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
        anchors.bottom: parent.bottom
        anchors.bottomMargin: dockWindow.dockBottomGap
        anchors.horizontalCenter: parent.horizontalCenter
        width: dockWindow.animatedPillWidth
        height: taskbarController.dockIconSize + 38
        radius: 22
        onWidthChanged: {
            if (!dockWindow.externalPinPreview)
                clickThroughWarmupTimer.restart()
        }

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

    // Only the dock row — not the airspace/wings above/side (those must pass clicks through).
    Flickable {
        id: flickable
        anchors.left: dockBg.left
        anchors.leftMargin: dockWindow.dockEndCap
        anchors.bottom: parent.bottom
        anchors.bottomMargin: dockWindow.dockBottomGap
        width: dockRowHost.width
        height: taskbarController.dockIconSize + 38
        contentWidth: Math.max(width, dockRowHost.width)
        contentHeight: height
        clip: false
        interactive: dockRowHost.width > width

        Item {
            id: dockRowHost
            width: dockWindow.dockRowWidth
            height: taskbarController.dockIconSize + 38
            x: width < parent.width ? Math.round((parent.width - width) / 2) : 0
            y: 0

            Behavior on x {
                enabled: (dockWindow.reordering || dockWindow.externalPinPreview)
                        && !dockWindow.dockPackAnim.running
                NumberAnimation { duration: dockWindow.dockPackAnimMs; easing.type: Easing.OutCubic }
            }

            // Lifted icon paints here — the slot delegate stays invisible at home X.
            Item {
                id: reorderDragOverlay
                visible: dockWindow.reorderDragOverlayActive
                x: dockWindow.dragLeftX
                y: dockWindow.dragGripY
                z: 200
                width: taskbarController.dockIconSize
                height: taskbarController.dockIconSize + 38

                Item {
                    id: reorderDragBubble
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: (dockWindow.reorderDragLiftMagnified
                                                   && taskbarController.dockHoverBounce) ? -8 : 0
                    width: taskbarController.dockIconSize
                    height: taskbarController.dockIconSize
                    scale: dockWindow.reorderDragLiftMagnified ? 1.18 : 1.0
                    opacity: (taskbarController.dockDragFadeEnabled && dockWindow.reorderDragFaded) ? 0.45 : 1.0
                    transformOrigin: Item.Center

                    Behavior on opacity {
                        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                    }

                    Item {
                        anchors.centerIn: parent
                        width: Math.round(taskbarController.dockIconSize * 0.96)
                        height: width

                        Image {
                            id: reorderDragImage
                            anchors.fill: parent
                            source: dockWindow.reorderDragIconUrl
                            sourceSize: Qt.size(
                                Math.ceil(width * dockWindow.iconDevicePixelRatio * 1.25),
                                Math.ceil(height * dockWindow.iconDevicePixelRatio * 1.25))
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: false
                            visible: status === Image.Ready
                        }

                        Rectangle {
                            anchors.fill: parent
                            radius: 8
                            color: "#D7DDE780"
                            visible: reorderDragImage.status !== Image.Ready
                        }

                        Text {
                            anchors.centerIn: parent
                            text: dockWindow.reorderDragIconHint.length > 0
                                  ? dockWindow.reorderDragIconHint
                                  : dockWindow.reorderDragLabel.slice(0, 1)
                            color: "#1C222B"
                            font.pixelSize: dockWindow.reorderDragLiftMagnified ? 24 : 20
                            font.weight: Font.DemiBold
                            visible: reorderDragImage.status !== Image.Ready
                        }
                    }
                }
            }

            Rectangle {
                id: externalPinSlotGhost
                visible: dockWindow.externalPinPreview && dockWindow.dragFrom < 0
                        && dockWindow.externalPinGhostVisible
                x: dockWindow.externalPinGhostX()
                y: dockWindow.externalPinRestY + dockWindow.externalPinGhostLiftY
                width: taskbarController.dockIconSize
                height: taskbarController.dockIconSize
                radius: 18
                z: 50
                color: "transparent"
                border.width: 0
                opacity: 1.0
                scale: 1.0
                transformOrigin: Item.Center

                Image {
                    id: externalPinPreviewIcon
                    anchors.fill: parent
                    source: dockWindow.externalPinIconUrl
                    fillMode: Image.PreserveAspectFit
                    asynchronous: false
                    smooth: true
                    mipmap: true
                    cache: true
                    opacity: dockWindow.externalPinIconUrl.length > 0 ? 1.0 : 0.0
                }
            }

            Repeater {
                id: dockRepeater
                model: dockModel

                onCountChanged: dockWindow.publishDropGeometry()

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

                    // Grab in item-local coords — cursor stays on the pressed pixel.
                    property bool dragging: false
                    property real grabLocalX: 0
                    property real grabLocalY: 0
                    property real gripX: 0
                    property real gripY: 0
                    property bool suppressClick: false
                    // Becomes true when dragging upward past unpinThreshold — triggers visual feedback.
                    property bool unpinDragPreview: false

                    function syncGripFromPointer() {
                        dockItemRoot.gripY = mouseArea.mouseY - dockItemRoot.grabLocalY
                        dockWindow.dragGripY = dockItemRoot.gripY
                        dockWindow.dragLeftX = dockItemRoot.x + mouseArea.mouseX - dockItemRoot.grabLocalX
                    }

                    function localHitRect() {
                        var pt = iconBubble.mapToItem(dockWindow.contentItem, 0, 0)
                        // Reserve magnify + tooltip room up front — hover must not resize hit regions.
                        var scale = 1.18
                        var pad = dockItemRoot.reorderLifted ? 12 : 8
                        var tooltipHeadroom = 36
                        var w = iconBubble.width * scale + pad * 2
                        var halfH = iconBubble.height * scale / 2 + pad
                        var cx = pt.x + iconBubble.width / 2
                        var cy = pt.y + iconBubble.height / 2
                        return Qt.rect(cx - w / 2, cy - halfH - tooltipHeadroom, w, halfH * 2 + tooltipHeadroom)
                    }

                    function playLaunchBounce() {
                        launchBounceAnim.restart()
                    }

                    function beginDragReorder() {
                        var wasMagnified = dockItemRoot.magnifyHover
                        dockWindow.clearActiveTooltip()
                        dockWindow.reorderDragIconUrl = dockItemRoot.iconUrl
                        dockWindow.reorderDragIconHint = dockItemRoot.iconHint
                        dockWindow.reorderDragLabel = dockItemRoot.label
                        dockWindow.reorderDragLiftMagnified = wasMagnified
                        dockItemRoot.dragging = true
                        dockWindow.neighborsPacked = true
                        dockWindow.dragFrom = dockItemRoot.index
                        dockWindow.dragTo = dockItemRoot.index
                        dockWindow.prevLayoutDragTo = dockItemRoot.index
                        dockWindow.layoutMorphFrom = -1
                        dockWindow.layoutMorph = 1
                        dockModel.setReorderActive(true)
                        if (!wasMagnified)
                            dockItemRoot.grabLocalY += 14
                        dockItemRoot.syncGripFromPointer()
                        dockWindow.reorderDragOverlayActive = true
                        dockWindow.syncDragZone(dockItemRoot.gripY)
                        dockWindow.syncDockPackState()
                    }
                    readonly property bool liftMagnified: dockItemRoot.magnifyHover || dockItemRoot.dragging
                    property bool magnifyHover: mouseArea.containsMouse
                            && !taskbarController.dockStaticIcons
                            && !dockWindow.reordering
                            && !dockWindow.externalPinPreview
                            && !launchBounceAnim.running
                    readonly property bool showTooltip: dockWindow.activeTooltipIndex === dockItemRoot.index
                    onMagnifyHoverChanged: {
                        if (!dockItemRoot.dragging && !dockWindow.reordering)
                            dockWindow.scheduleClickHitRegionRefresh()
                    }
                    readonly property bool reorderLifted: dockItemRoot.dragging
                            || (dockWindow.reorderSettling && dockItemRoot.index === dockWindow.dragFrom)
                    readonly property real fullRestX: dockWindow.slotLeftForIndex(dockItemRoot.index)
                    readonly property real compactRestX: {
                        var packed = dockWindow.packedSlotForIndex(
                            dockItemRoot.index, dockWindow.dragFrom)
                        return packed < 0
                                ? dockItemRoot.fullRestX
                                : dockWindow.slotLeftForIndex(packed)
                    }
                    readonly property int layoutDragTo: dockWindow.dragTo
                    readonly property real previewRestX: {
                        var slot = dockWindow.previewSlotForIndex(
                            dockItemRoot.index, dockWindow.dragFrom, dockItemRoot.layoutDragTo)
                        return slot < 0
                                ? dockItemRoot.fullRestX
                                : dockWindow.slotLeftForIndex(slot)
                    }
                    readonly property real morphedPreviewRestX: {
                        if (dockWindow.layoutMorph >= 1
                                || dockWindow.layoutMorphFrom < 0
                                || dockWindow.dragFrom < 0)
                            return dockItemRoot.previewRestX
                        var oldSlot = dockWindow.previewSlotForIndex(
                            dockItemRoot.index, dockWindow.dragFrom, dockWindow.layoutMorphFrom)
                        var oldX = oldSlot < 0
                                ? dockItemRoot.fullRestX
                                : dockWindow.slotLeftForIndex(oldSlot)
                        return oldX + dockWindow.layoutMorph * (dockItemRoot.previewRestX - oldX)
                    }
                    readonly property real insertPreviewX: {
                        if (!dockWindow.externalPinPreview)
                            return dockItemRoot.fullRestX
                        var slot = dockWindow.previewSlotForInsert(
                            dockItemRoot.index, dockWindow.externalPinTo)
                        return dockWindow.slotLeftForIndex(slot)
                    }
                    readonly property real morphedInsertPreviewX: {
                        if (dockWindow.externalPinPreview)
                            return dockWindow.insertSlotXForItem(
                                dockItemRoot.index, dockWindow.externalPinMorphSlot)
                        if (dockWindow.layoutMorph >= 1
                                || dockWindow.layoutMorphFrom < 0)
                            return dockItemRoot.insertPreviewX
                        var oldSlot = dockWindow.previewSlotForInsert(
                            dockItemRoot.index, dockWindow.layoutMorphFrom)
                        var newSlot = dockWindow.previewSlotForInsert(
                            dockItemRoot.index, dockWindow.externalPinTo)
                        var oldX = dockWindow.slotLeftForIndex(oldSlot)
                        var newX = dockWindow.slotLeftForIndex(newSlot)
                        return oldX + dockWindow.layoutMorph * (newX - oldX)
                    }
                    // Wide layout endpoint: full row on first lift, preview row after hovering.
                    readonly property real expandedRestX: {
                        if (!dockWindow.neighborsPacked || dockWindow.dragFrom < 0)
                            return dockItemRoot.fullRestX
                        if (dockWindow.dragInDockZone || dockWindow.dockZoneEverEntered)
                            return dockItemRoot.morphedPreviewRestX
                        return dockItemRoot.fullRestX
                    }
                    // Single driver with dockPackT — icons and pill width stay in sync.
                    readonly property real restingX: {
                        // Newly pinned icon: insertion gap, not neighbor-shift slot (to+1).
                        if (dockWindow.externalPinHandoffIndex === dockItemRoot.index
                                || (dockWindow.externalPinCommitting
                                    && dockItemRoot.index === dockWindow.externalPinTo))
                            return dockWindow.slotLeftForIndex(dockWindow.externalPinTo)
                        if (dockWindow.externalPinPreview && dockWindow.dragFrom < 0)
                            return dockItemRoot.fullRestX
                                   + (1 - dockWindow.dockPackT)
                                     * (dockItemRoot.morphedInsertPreviewX - dockItemRoot.fullRestX)
                        if (!dockWindow.neighborsPacked || dockWindow.dragFrom < 0)
                            return dockItemRoot.fullRestX
                        return dockItemRoot.compactRestX
                               + (1 - dockWindow.dockPackT)
                                 * (dockItemRoot.expandedRestX - dockItemRoot.compactRestX)
                    }
                    readonly property real liftedDragX: dockWindow.visualDragLeftX()
                    x: dockItemRoot.reorderLifted
                            ? (dockItemRoot.dragging
                               ? dockItemRoot.fullRestX
                               : dockWindow.dragLeftX)
                            : dockItemRoot.restingX
                    property real slideY: {
                        if (dockItemRoot.dragging)
                            return dockItemRoot.gripY
                        if (dockWindow.reorderSettling && dockItemRoot.index === dockWindow.dragFrom)
                            return dockWindow.settleDragY
                        return 0
                    }
                    readonly property real runningDotBase: dockWindow.darkTheme ? 0.42 : 0.35
                    readonly property real runningDotOpacity: dockItemRoot.running && !dockItemRoot.reorderLifted
                            ? dockItemRoot.runningDotBase
                            : 0

                    width: taskbarController.dockIconSize
                    height: taskbarController.dockIconSize + 38

                    opacity: dockItemRoot.dragging ? 0
                            : (dockItemRoot.reorderLifted ? 0.92
                               : (dockItemRoot.running || dockItemRoot.pinned ? 1.0 : 0.55))
                    z: dockItemRoot.reorderLifted ? 100 : (dockItemRoot.magnifyHover ? 10 : 0)

                    Behavior on opacity {
                        enabled: !dockItemRoot.dragging
                        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                    }

                    Behavior on x {
                        // macOS-style reorder: while dragging, neighboring icons must glide
                        // into the newly opened slot instead of snapping through each other.
                        // The lifted icon itself stays under the pointer/settle animation;
                        // only the non-lifted delegates animate their horizontal slot changes.
                        enabled: !dockItemRoot.dragging
                                && !(dockWindow.reorderSettling
                                     && dockItemRoot.index === dockWindow.dragFrom)
                                && !dockWindow.externalPinPreview
                                && !dockWindow.externalPinCommitting
                                && dockWindow.externalPinHandoffIndex !== dockItemRoot.index
                                // Pill width follows dockPackT directly — x must not lag behind.
                                && !dockWindow.dockPackAnim.running
                        NumberAnimation {
                            duration: dockWindow.reorderAnimMs
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on slideY {
                        enabled: !dockItemRoot.dragging
                                && !(dockWindow.reorderSettling
                                     && dockItemRoot.index === dockWindow.dragFrom)
                        NumberAnimation {
                            duration: dockWindow.reorderAnimMs
                            easing.type: Easing.OutCubic
                        }
                    }

                    Rectangle {
                        id: iconBubble
                        width: taskbarController.dockIconSize
                        height: taskbarController.dockIconSize
                        radius: 18
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: (dockItemRoot.liftMagnified && taskbarController.dockHoverBounce) ? -8 : 0
                        color: "transparent"
                        border.width: 0
                        border.color: "transparent"
                        opacity: dockItemRoot.unpinDragPreview ? 0.55 : 1.0

                        transform: [
                            Translate { y: dockItemRoot.slideY },
                            Translate { id: launchTranslate },
                            Scale {
                                id: hoverScale
                                origin.x: iconBubble.width / 2
                                origin.y: iconBubble.height / 2
                                xScale: dockItemRoot.liftMagnified ? 1.18 : 1.0
                                yScale: dockItemRoot.liftMagnified ? 1.18 : 1.0
                                Behavior on xScale {
                                    enabled: !dockItemRoot.dragging && !dockWindow.reorderSettling
                                    SpringAnimation { spring: 4.0; damping: 0.5; epsilon: 0.2 }
                                }
                                Behavior on yScale {
                                    enabled: !dockItemRoot.dragging && !dockWindow.reorderSettling
                                    SpringAnimation { spring: 4.0; damping: 0.5; epsilon: 0.2 }
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
                            enabled: !dockItemRoot.dragging
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
                        id: runningDot
                        width: 4
                        height: 4
                        radius: 2
                        visible: dockItemRoot.running
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 6
                        color: dockWindow.darkTheme ? "#FFFFFF" : "#000000"
                        opacity: dockItemRoot.runningDotOpacity
                        Behavior on color {
                            ColorAnimation { duration: dockWindow.themeAnimMs; easing.type: Easing.InOutCubic }
                        }
                        Behavior on opacity {
                            NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                        }
                        Rectangle {
                            id: unpinnedRunningRing
                            anchors.fill: parent
                            anchors.margins: -1
                            radius: 3
                            color: "transparent"
                            border.width: dockItemRoot.running && !dockItemRoot.pinned ? 1.5 : 0
                            border.color: dockWindow.darkTheme ? "#FFFFFF" : "#000000"
                            opacity: dockItemRoot.running && !dockItemRoot.pinned ? dockItemRoot.runningDotBase : 0
                            visible: dockItemRoot.running && !dockItemRoot.pinned
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
                        opacity: (dockItemRoot.showTooltip && !dockWindow.reordering) ? 1 : 0
                        visible: opacity > 0 && !dockWindow.reordering
                        z: 20

                        Behavior on opacity {
                            enabled: !dockWindow.reordering && !dockItemRoot.dragging
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
                                text: dockItemRoot.pinned ? dockItemRoot.label : dockItemRoot.label + " (не закреплено)"
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

                    readonly property bool launchBouncing: launchBounceAnim.running
                    readonly property bool hasVisualOverflow: dockItemRoot.magnifyHover
                            || dockItemRoot.showTooltip
                            || dockItemRoot.reorderLifted
                            || dockItemRoot.launchBouncing

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        // Keep the press so a horizontal drag is not stolen by the Flickable.
                        preventStealing: true
                        cursorShape: dockItemRoot.dragging ? Qt.ClosedHandCursor : Qt.PointingHandCursor

                        onContainsMouseChanged: {
                            if (mouseArea.containsMouse && !dockWindow.reordering)
                                dockWindow.showTooltipForIndex(dockItemRoot.index)
                            else if (!mouseArea.containsMouse)
                                dockWindow.scheduleHideTooltipForIndex(dockItemRoot.index)
                        }

                        onPressed: function(mouse) {
                            if (mouse.button !== Qt.LeftButton)
                                return
                            dockItemRoot.suppressClick = false
                            dockItemRoot.dragging = false
                            dockItemRoot.grabLocalX = mouse.x
                            dockItemRoot.grabLocalY = mouse.y
                            dockItemRoot.gripX = 0
                            dockItemRoot.gripY = 0
                        }

                        onPositionChanged: function(mouse) {
                            if (!(mouse.buttons & Qt.LeftButton))
                                return
                            if (!dockItemRoot.dragging) {
                                var movedX = Math.abs(mouse.x - dockItemRoot.grabLocalX)
                                var movedY = Math.abs(mouse.y - dockItemRoot.grabLocalY)
                                if (movedX <= 6 && movedY <= 6)
                                    return
                                dockItemRoot.beginDragReorder()
                            }
                            dockItemRoot.syncGripFromPointer()
                            if (dockItemRoot.pinned && dockItemRoot.gripY < dockWindow.unpinThreshold) {
                                dockItemRoot.unpinDragPreview = true
                            } else {
                                dockItemRoot.unpinDragPreview = false
                            }
                            dockWindow.updateDragTarget(
                                dockWindow.dragLeftX,
                                dockItemRoot.gripY,
                                dockRepeater.count,
                                dockWindow.dragFrom)
                        }

                        onReleased: function(mouse) {
                            if (!dockItemRoot.dragging)
                                return
                            var from = dockWindow.dragFrom
                            dockItemRoot.syncGripFromPointer()
                            dockWindow.syncDragZone(dockItemRoot.gripY)
                            if (!dockWindow.dragInDockZone)
                                dockWindow.dragTo = from
                            var to = dockWindow.dragTo
                            var releaseDragY = dockItemRoot.gripY
                            dockItemRoot.dragging = false
                            dockWindow.reorderDragOverlayActive = false
                            dockItemRoot.suppressClick = true

                            if (dockItemRoot.unpinDragPreview && dockItemRoot.pinned) {
                                dockItemRoot.unpinDragPreview = false
                                dockWindow.finishUnpin(from)
                                return
                            }

                            if (from < 0) {
                                dockWindow.cancelReorder()
                                return
                            }

                            dockWindow.beginReorderSettle(from, to, releaseDragY)
                        }

                        onCanceled: {
                            if (!dockItemRoot.dragging)
                                return
                            var from = dockWindow.dragFrom
                            dockItemRoot.syncGripFromPointer()
                            dockWindow.syncDragZone(dockItemRoot.gripY)
                            if (!dockWindow.dragInDockZone)
                                dockWindow.dragTo = from
                            var releaseDragY = dockItemRoot.gripY
                            dockItemRoot.dragging = false
                            dockWindow.reorderDragOverlayActive = false
                            dockItemRoot.suppressClick = true
                            if (dockItemRoot.unpinDragPreview && from >= 0 && dockItemRoot.pinned) {
                                dockItemRoot.unpinDragPreview = false
                                dockWindow.finishUnpin(from)
                                return
                            }
                            if (from >= 0)
                                dockWindow.beginReorderSettle(from, from, releaseDragY)
                            else
                                dockWindow.cancelReorder()
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

            Item {
                id: externalPinGhost
                visible: dockWindow.externalPinPreview && !dockWindow.externalPinGhostVisible
                x: dockWindow.externalPinGhostX()
                width: taskbarController.dockIconSize
                height: taskbarController.dockIconSize + 38
                z: 49
                opacity: dockWindow.externalPinPreview ? 0.85 * (1 - dockWindow.dockPackT) : 0

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    width: taskbarController.dockIconSize
                    height: taskbarController.dockIconSize
                    radius: 18
                    color: dockWindow.darkTheme ? "#FFFFFF14" : "#0000000C"
                    border.width: 2
                    border.color: dockWindow.darkTheme ? "#FFFFFF44" : "#00000022"

                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        font.pixelSize: 22
                        font.weight: Font.Medium
                        color: dockWindow.darkTheme ? "#FFFFFF88" : "#00000055"
                    }
                }
            }
        }
    }
}
