import QtQuick

Item {
    id: gestures
    required property Flickable viewport
    required property Item sheet
    property real zoom: 1
    property bool fitWidth: false
    property bool singleFingerGestures: true
    property string doubleTapAction: "zoom"
    property string swipeAction: "pages"
    property real panSensitivity: 1
    readonly property bool active: pinch.active || drag.active
    signal zoomRequested(real value)
    signal fitPageRequested()
    signal pageRequested(int delta)
    signal interactionStarted()
    signal fullPageRequested()
    signal refreshRequested()

    function clampPosition(x: real, y: real) {
        viewport.contentX = Math.max(0, Math.min(Math.max(0, viewport.contentWidth - viewport.width), x))
        viewport.contentY = Math.max(0, Math.min(Math.max(0, viewport.contentHeight - viewport.height), y))
    }

    function toggleZoomAt(position: point) {
        if (!enabled || !singleFingerGestures || pinch.active || drag.active)
            return
        interactionStarted()
        if (doubleTapAction === "none") return
        if (doubleTapAction === "fullPage") { fullPageRequested(); return }
        if (doubleTapAction === "refresh") { refreshRequested(); return }
        if (zoom > 1.01 || fitWidth) {
            fitPageRequested()
            Qt.callLater(function() { if (gestures.enabled) gestures.clampPosition(0, 0) })
            return
        }
        const point = sheet.mapFromItem(gestures, position.x, position.y)
        const anchorX = point.x / Math.max(1, sheet.width)
        const anchorY = point.y / Math.max(1, sheet.height)
        zoomRequested(2)
        Qt.callLater(function() {
            if (gestures.enabled)
                gestures.clampPosition(gestures.sheet.x + anchorX * gestures.sheet.width * gestures.sheet.scale - position.x,
                                       gestures.sheet.y + anchorY * gestures.sheet.height * gestures.sheet.scale - position.y)
        })
    }

    function updatePinchPosition() {
        if (!gestures.enabled || !pinch.active)
            return
        const center = pinch.centroid.position
        gestures.clampPosition(sheet.x + pinch.anchorX * sheet.width * sheet.scale - center.x,
                               sheet.y + pinch.anchorY * sheet.height * sheet.scale - center.y)
    }

    TapHandler {
        enabled: gestures.singleFingerGestures
        acceptedDevices: PointerDevice.TouchScreen
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.DragThreshold
        onDoubleTapped: point => gestures.toggleZoomAt(point.position)
    }

    PinchHandler {
        id: pinch
        target: null
        acceptedDevices: PointerDevice.TouchScreen | PointerDevice.TouchPad
        minimumPointCount: 2
        maximumPointCount: 2
        minimumScale: 0.001
        maximumScale: 1000
        property real initialZoom: 1
        property real anchorX: 0.5
        property real anchorY: 0.5
        onActiveChanged: {
            if (active) {
                gestures.interactionStarted()
                initialZoom = gestures.zoom
                const point = gestures.sheet.mapFromItem(gestures, centroid.position.x, centroid.position.y)
                anchorX = point.x / Math.max(1, gestures.sheet.width)
                anchorY = point.y / Math.max(1, gestures.sheet.height)
            }
        }
        onActiveScaleChanged: {
            if (!active)
                return
            const next = Math.max(1, Math.min(3, initialZoom * activeScale))
            if (Math.abs(next - gestures.zoom) > 0.0001)
                gestures.zoomRequested(next)
            Qt.callLater(gestures.updatePinchPosition)
        }
        onTranslationChanged: {
            // Both fingers can move in parallel without any change of scale.
            if (active)
                Qt.callLater(gestures.updatePinchPosition)
        }
    }

    DragHandler {
        id: drag
        target: null
        enabled: gestures.singleFingerGestures && !pinch.active
        acceptedDevices: PointerDevice.TouchScreen
        minimumPointCount: 1
        maximumPointCount: 1
        dragThreshold: 18
        property real startX: 0
        property real startY: 0
        property real lastX: 0
        property real lastY: 0
        property bool panning: false
        onActiveChanged: {
            if (active) {
                gestures.interactionStarted()
                startX = gestures.viewport.contentX
                startY = gestures.viewport.contentY
                lastX = 0
                lastY = 0
                panning = gestures.viewport.contentWidth > gestures.viewport.width + 2
                    || gestures.viewport.contentHeight > gestures.viewport.height + 2
            }
        }
        onGrabChanged: (transition, point) => {
            // Losing the drag to another finger or to a modal is not a swipe.
            // Only complete navigation when the grabbed finger is lifted.
            if (transition !== PointerDevice.UngrabExclusive || point.state !== Qt.TouchPointReleased
                || pinch.active || !gestures.enabled || !gestures.singleFingerGestures)
                return
            const threshold = Math.max(70, Math.min(160, gestures.viewport.width * 0.14))
            if (gestures.swipeAction === "pages" && !panning && Math.abs(lastX) > threshold && Math.abs(lastX) > Math.abs(lastY) * 1.5)
                gestures.pageRequested(lastX < 0 ? 1 : -1)
            else if (gestures.swipeAction === "scroll" && Math.abs(lastY) > threshold && Math.abs(lastY) > Math.abs(lastX) * 1.5) {
                const maxY = Math.max(0, gestures.viewport.contentHeight - gestures.viewport.height)
                if (!panning || (lastY < 0 && startY >= maxY - 2) || (lastY > 0 && startY <= 2))
                    gestures.pageRequested(lastY < 0 ? 1 : -1)
            }
        }
        onActiveTranslationChanged: {
            if (!active)
                return
            // DragHandler reports scene-space translation, even when its parent
            // is rotated. Convert the vector before panning or changing pages.
            const origin = gestures.mapFromItem(null, 0, 0)
            const point = gestures.mapFromItem(null, activeTranslation.x, activeTranslation.y)
            lastX = point.x - origin.x
            lastY = point.y - origin.y
            if (panning)
                gestures.clampPosition(startX - lastX * gestures.panSensitivity, startY - lastY * gestures.panSensitivity)
        }
    }
}
