import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Item {
    id: overlay
    required property Notebook notebook
    required property real uiScale
    required property Handwriting handwriting
    property Item toolbarParent: overlay
    property real toolbarScale: uiScale
    property bool acceptingInput: true
    property bool showTouchKeyboard: false
    property bool keepInk: false
    property var selection: null
    property real dx: 0
    property real dy: 0
    property real factor: 1
    property real angle: 0
    property bool previewTranscript: false
    property bool precisionControls: false
    property string errorText: ""
    readonly property bool active: selection !== null
    readonly property var selectedItems: selection ? selection.items || [] : []
    readonly property var displayItems: selectedItems.map(item => notebook.pageItems.find(candidate => candidate.id === item.id) || item)
    readonly property bool quarterTurnsOnly: selection && selection.strokes.some(stroke =>
        (stroke.tool === "rectangle" || stroke.tool === "ellipse") && !!stroke.original)
    readonly property var selectedIndexes: selection ? selection.indexes || [] : []
    readonly property var bounds: selection ? selection.bounds : Qt.rect(0, 0, 0, 0)
    readonly property point pivot: Qt.point((bounds.x + bounds.width / 2) * width,
                                           (bounds.y + bounds.height / 2) * height)
    readonly property bool hasHandwriting: selection && selection.strokes.some(stroke =>
        !stroke.tool || ["pen", "pencil", "marker"].includes(stroke.tool))
    readonly property var previewItem: previewTranscript ? notebook.handwritingPreview(selection, transcript.text) : ({})
    readonly property bool changed: dx !== 0 || dy !== 0 || factor !== 1 || angle !== 0
    signal selectionClosed()
    signal extendRequested(string operation)
    signal recognizeRequested()
    signal moreRequested()
    signal editTranscriptRequested(string text)
    signal tableTranscriptionRequested()
    signal diagramRecognitionRequested()
    signal restoreRequested(string id)

    onChangedChanged: if (changed) handwriting.cancel()
    visible: active
    enabled: acceptingInput

    // Only handle keys while the selection itself has focus. Text editors and
    // modal popups retain their normal arrow, Return and Escape behavior.
    Keys.onPressed: event => {
        if (!activeFocus || !active || !acceptingInput || previewTranscript) return
        if (event.modifiers & (Qt.ControlModifier | Qt.MetaModifier | Qt.AltModifier)) return
        const coarse = Boolean(event.modifiers & Qt.ShiftModifier)
        if (event.key === Qt.Key_Left) nudge(-1, 0, coarse)
        else if (event.key === Qt.Key_Right) nudge(1, 0, coarse)
        else if (event.key === Qt.Key_Up) nudge(0, -1, coarse)
        else if (event.key === Qt.Key_Down) nudge(0, 1, coarse)
        else if (event.key === Qt.Key_Escape) clear()
        else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && changed) apply()
        else return
        event.accepted = true
    }

    function resetTransform() {
        dx = 0; dy = 0; factor = 1; angle = 0
        errorText = ""
    }
    function nudge(horizontal: int, vertical: int, coarse: bool) {
        if (!active || previewTranscript) return
        // One logical page point; independent of zoom or screen resolution.
        const step = coarse ? 10 : 1
        dx += horizontal * step / 600
        dy += vertical * step / 800
        errorText = ""
    }
    function turn(direction: int) {
        if (!active || previewTranscript) return
        angle = (angle + direction * (quarterTurnsOnly ? 90 : 15) + 360) % 360
        errorText = ""
    }
    function resizeBy(amount: real) {
        if (!active || previewTranscript) return
        factor = Math.max(.15, Math.min(6, Math.round(factor * amount * 10000) / 10000))
        errorText = ""
    }

    function begin(value: var) {
        selection = value
        resetTransform()
        previewTranscript = false
        precisionControls = false
        errorText = ""
        transcript.text = ""
        forceActiveFocus()
    }
    function clear() {
        handwriting.cancel()
        selection = null
        previewTranscript = false
        transcript.text = ""
        errorText = ""
        selectionClosed()
    }
    function apply() {
        if (notebook.transformSelection(selection, dx, dy, factor, angle))
            clear()
        else
            errorText = notebook.error
    }
    function showTranscription() {
        if (!active || handwriting.result.length === 0)
            return
        // A transcription applies to the original selection, before any pending transform.
        dx = 0; dy = 0; factor = 1; angle = 0
        transcript.text = handwriting.result
        previewTranscript = true
    }
    function controlsContain(point: point): bool {
        if (!visible || !enabled)
            return false
        for (const control of [toolbar, selectedFrame, resizeHandle, rotateHandle, transcriptBox]) {
            const local = control.mapFromItem(overlay, point)
            if (control.visible && local.x >= 0 && local.y >= 0 && local.x <= control.width && local.y <= control.height)
                return true
        }
        return false
    }

    Item {
        id: transformed
        anchors.fill: parent
        transform: [
            Scale { origin.x: overlay.pivot.x; origin.y: overlay.pivot.y; xScale: overlay.factor; yScale: overlay.factor },
            Rotation { origin.x: overlay.pivot.x; origin.y: overlay.pivot.y; angle: overlay.angle },
            Translate { x: overlay.dx * overlay.width; y: overlay.dy * overlay.height }
        ]
        SelectionContentItem {
            anchors.fill: parent
            content: ({
                items: overlay.displayItems,
                strokes: !overlay.selection ? [] : !overlay.previewTranscript ? overlay.selection.strokes
                    : overlay.selection.strokes.filter(stroke => stroke.tool && !["pen", "pencil", "marker"].includes(stroke.tool))
            })
        }
        Rectangle {
            id: selectedFrame
            objectName: "selectedFrame"
            property bool blocksInk: true
            visible: !overlay.previewTranscript
            x: overlay.bounds.x * overlay.width
            y: overlay.bounds.y * overlay.height
            width: Math.max(8 * overlay.uiScale, overlay.bounds.width * overlay.width)
            height: Math.max(8 * overlay.uiScale, overlay.bounds.height * overlay.height)
            color: "#180059bd"
            border.color: "#0059bd"
            border.width: Math.max(1, overlay.uiScale) / overlay.factor
            MouseArea {
                anchors.fill: parent
                preventStealing: true
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                property point origin
                property point previous
                onPressed: event => {
                    overlay.forceActiveFocus()
                    origin = mapToItem(overlay, event.x, event.y)
                    previous = Qt.point(overlay.dx, overlay.dy)
                }
                onPositionChanged: event => {
                    if (!pressed) return
                    const point = mapToItem(overlay, event.x, event.y)
                    overlay.dx = previous.x + (point.x - origin.x) / overlay.width
                    overlay.dy = previous.y + (point.y - origin.y) / overlay.height
                }
            }
        }
        Rectangle {
            id: resizeHandle
            objectName: "resizeHandle"
            property bool blocksInk: true
            visible: !overlay.previewTranscript
            width: 28 * overlay.uiScale / overlay.factor
            height: width
            radius: width / 2
            x: selectedFrame.x + selectedFrame.width - width
            y: selectedFrame.y + selectedFrame.height - height
            color: "white"; border.color: "#0059bd"; border.width: 2 / overlay.factor
            Text { anchors.centerIn: parent; text: "↔"; font.pixelSize: 18 * overlay.uiScale / overlay.factor }
            MouseArea {
                anchors.fill: parent; preventStealing: true
                property real initialDistance
                property real initialFactor
                onPressed: event => {
                    overlay.forceActiveFocus()
                    const point = mapToItem(overlay, event.x, event.y)
                    initialDistance = Math.max(1, Math.hypot(point.x - overlay.pivot.x - overlay.dx * overlay.width,
                                                           point.y - overlay.pivot.y - overlay.dy * overlay.height))
                    initialFactor = overlay.factor
                }
                onPositionChanged: event => {
                    if (!pressed) return
                    const point = mapToItem(overlay, event.x, event.y)
                    const distance = Math.hypot(point.x - overlay.pivot.x - overlay.dx * overlay.width,
                                                point.y - overlay.pivot.y - overlay.dy * overlay.height)
                    overlay.factor = Math.max(.15, Math.min(6, initialFactor * distance / initialDistance))
                }
            }
        }
        Rectangle {
            id: rotateHandle
            objectName: "rotateHandle"
            property bool blocksInk: true
            visible: !overlay.previewTranscript
            width: 28 * overlay.uiScale / overlay.factor
            height: width
            radius: width / 2
            x: selectedFrame.x + selectedFrame.width - width
            y: selectedFrame.y
            color: "white"; border.color: "#0059bd"; border.width: 2 / overlay.factor
            Text { anchors.centerIn: parent; text: "↻"; font.pixelSize: 18 * overlay.uiScale / overlay.factor }
            MouseArea {
                anchors.fill: parent; preventStealing: true
                property real initialAngle
                property real previousAngle
                onPressed: event => {
                    overlay.forceActiveFocus()
                    const point = mapToItem(overlay, event.x, event.y)
                    initialAngle = Math.atan2(point.y - overlay.pivot.y - overlay.dy * overlay.height,
                                             point.x - overlay.pivot.x - overlay.dx * overlay.width) * 180 / Math.PI
                    previousAngle = overlay.angle
                }
                onPositionChanged: event => {
                    if (!pressed) return
                    const point = mapToItem(overlay, event.x, event.y)
                    const angle = Math.atan2(point.y - overlay.pivot.y - overlay.dy * overlay.height,
                                            point.x - overlay.pivot.x - overlay.dx * overlay.width) * 180 / Math.PI
                    const step = overlay.quarterTurnsOnly ? 90 : 15
                    overlay.angle = Math.round((previousAngle + angle - initialAngle) / step) * step
                }
            }
        }
    }

    Rectangle {
        id: transcriptBox
            objectName: "transcriptBox"
        property bool blocksInk: true
        visible: overlay.previewTranscript
        x: (overlay.previewItem.x || overlay.bounds.x) * overlay.width
        y: (overlay.previewItem.y || overlay.bounds.y) * overlay.height
        width: Math.max(30, (overlay.previewItem.width || overlay.bounds.width) * overlay.width)
        height: Math.max(30, (overlay.previewItem.height || overlay.bounds.height) * overlay.height)
        color: "white"; border.color: "#0059bd"
        border.width: Math.max(1, overlay.uiScale)
        ScrollView {
            anchors.fill: parent
            TextArea {
                id: transcript
                objectName: "selectionTranscript"
                wrapMode: TextEdit.WrapAnywhere
                padding: 3 * overlay.width / 600
                textFormat: TextEdit.PlainText
                selectByMouse: true
                font.pixelSize: Math.max(12, overlay.width / 600 * 16)
            }
        }
    }

    Rectangle {
        id: toolbar
        property bool blocksInk: true
        parent: overlay.toolbarParent
        visible: overlay.visible
        enabled: overlay.enabled
        z: 6
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, 520 * overlay.toolbarScale)
        height: controls.implicitHeight + 12 * overlay.toolbarScale
        color: "#f6f6f8"; radius: 8 * overlay.toolbarScale
        border.color: "#a0a0a8"; border.width: Math.max(1, overlay.toolbarScale)
        ColumnLayout {
            id: controls
            anchors.fill: parent; anchors.margins: 6 * overlay.toolbarScale
            spacing: 2 * overlay.toolbarScale
            Flow {
                Layout.fillWidth: true
                spacing: 3 * overlay.toolbarScale
                PaperButton { uiScale: overlay.toolbarScale; text: qsTr("Aggiungi"); enabled: !overlay.changed; visible: !overlay.previewTranscript; onClicked: overlay.extendRequested("add") }
                PaperButton { uiScale: overlay.toolbarScale; text: qsTr("Sottrai"); enabled: !overlay.changed; visible: !overlay.previewTranscript; onClicked: overlay.extendRequested("subtract") }
                PaperButton { uiScale: overlay.toolbarScale; text: qsTr("Trascrivi"); visible: overlay.hasHandwriting && !overlay.previewTranscript; enabled: !overlay.handwriting.busy && !overlay.changed; onClicked: overlay.recognizeRequested() }
                PaperButton {
                    objectName: "selectionInsertIntoCells"
                    uiScale: overlay.toolbarScale
                    text: qsTr("Inserisci nelle celle")
                    visible: overlay.hasHandwriting && overlay.notebook.pageItems.some(item => item.type === "table")
                    enabled: !overlay.changed
                    onClicked: overlay.tableTranscriptionRequested()
                }
                PaperButton {
                    objectName: "selectionRecognizeDiagram"
                    uiScale: overlay.toolbarScale
                    text: qsTr("Diagramma")
                    visible: overlay.active && overlay.selection.strokes.length > 0
                    enabled: !overlay.changed
                    onClicked: overlay.diagramRecognitionRequested()
                }
                PaperButton { uiScale: overlay.toolbarScale; text: qsTr("Azioni"); visible: !overlay.previewTranscript; enabled: !overlay.changed; onClicked: overlay.moreRequested() }
                PaperButton {
                    objectName: "selectionPrecision"
                    uiScale: overlay.toolbarScale; text: qsTr("Precisione"); visible: !overlay.previewTranscript
                    focusPolicy: Qt.NoFocus; checkable: true; checked: overlay.precisionControls; filled: checked
                    onClicked: { overlay.precisionControls = !overlay.precisionControls; overlay.forceActiveFocus() }
                }
                PaperButton { uiScale: overlay.toolbarScale; text: qsTr("Correggi"); visible: overlay.previewTranscript; onClicked: overlay.editTranscriptRequested(transcript.text) }
                PaperButton { uiScale: overlay.toolbarScale; text: qsTr("Originale"); visible: overlay.previewTranscript; onClicked: overlay.previewTranscript = false }
                PaperButton {
                    uiScale: overlay.toolbarScale; text: qsTr("Inserisci qui"); filled: true; visible: overlay.previewTranscript
                    enabled: transcript.text.trim().length > 0
                    onClicked: { if (overlay.notebook.replaceHandwriting(overlay.selection, transcript.text, overlay.keepInk)) overlay.clear(); else overlay.errorText = overlay.notebook.error }
                }
                PaperButton { uiScale: overlay.toolbarScale; text: qsTr("Applica"); filled: true; visible: !overlay.previewTranscript && overlay.changed; onClicked: overlay.apply() }
                PaperButton {
                    objectName: "selectionReset"
                    uiScale: overlay.toolbarScale; text: qsTr("Reimposta"); visible: !overlay.previewTranscript && overlay.changed
                    focusPolicy: Qt.NoFocus; onClicked: { overlay.resetTransform(); overlay.forceActiveFocus() }
                }
                PaperButton { uiScale: overlay.toolbarScale; text: overlay.changed ? qsTr("Annulla") : qsTr("Chiudi"); onClicked: overlay.clear() }
            }
            Text {
                objectName: "selectionSummary"
                Layout.fillWidth: true
                text: qsTr("%1 tratti · %2 oggetti").arg(overlay.selectedIndexes.length).arg(overlay.selectedItems.length)
                color: "#535862"; font.pixelSize: 12 * overlay.toolbarScale
                wrapMode: Text.WordWrap
            }
            Flow {
                visible: overlay.precisionControls && !overlay.previewTranscript
                Layout.fillWidth: true
                spacing: 3 * overlay.toolbarScale
                PaperButton {
                    uiScale: overlay.toolbarScale
                    focusPolicy: Qt.NoFocus
                    text: "←"
                    Accessible.name: qsTr("Sposta a sinistra")
                    onClicked: overlay.nudge(-1, 0, false)
                }
                PaperButton {
                    uiScale: overlay.toolbarScale
                    focusPolicy: Qt.NoFocus
                    text: "↑"
                    Accessible.name: qsTr("Sposta in alto")
                    onClicked: overlay.nudge(0, -1, false)
                }
                PaperButton {
                    uiScale: overlay.toolbarScale
                    focusPolicy: Qt.NoFocus
                    text: "↓"
                    Accessible.name: qsTr("Sposta in basso")
                    onClicked: overlay.nudge(0, 1, false)
                }
                PaperButton {
                    uiScale: overlay.toolbarScale
                    focusPolicy: Qt.NoFocus
                    text: "→"
                    Accessible.name: qsTr("Sposta a destra")
                    onClicked: overlay.nudge(1, 0, false)
                }
                PaperButton {
                    uiScale: overlay.toolbarScale
                    focusPolicy: Qt.NoFocus
                    text: "−"
                    Accessible.name: qsTr("Riduci selezione")
                    enabled: overlay.factor > .15
                    onClicked: overlay.resizeBy(1 / 1.1)
                }
                PaperButton {
                    uiScale: overlay.toolbarScale
                    focusPolicy: Qt.NoFocus
                    text: "+"
                    Accessible.name: qsTr("Ingrandisci selezione")
                    enabled: overlay.factor < 6
                    onClicked: overlay.resizeBy(1.1)
                }
                PaperButton {
                    uiScale: overlay.toolbarScale
                    focusPolicy: Qt.NoFocus
                    text: "↶"
                    Accessible.name: qsTr("Ruota a sinistra")
                    onClicked: overlay.turn(-1)
                }
                PaperButton {
                    uiScale: overlay.toolbarScale
                    focusPolicy: Qt.NoFocus
                    text: "↷"
                    Accessible.name: qsTr("Ruota a destra")
                    onClicked: overlay.turn(1)
                }
            }
            Text {
                visible: overlay.precisionControls && !overlay.previewTranscript
                Layout.fillWidth: true
                text: qsTr("Scala %1% · Rotazione %2° · Spostamento %3, %4 pt")
                    .arg(Math.round(overlay.factor * 100)).arg(overlay.angle)
                    .arg(Math.round(overlay.dx * 600)).arg(Math.round(overlay.dy * 800))
                    + "\n" + qsTr("Frecce: 1 pt · Maiusc + frecce: 10 pt · Invio: applica · Esc: chiudi")
                color: "#535862"; font.pixelSize: 12 * overlay.toolbarScale; wrapMode: Text.WordWrap
            }
            PaperButton {
                objectName: "selectionCancelRecognition"
                uiScale: overlay.toolbarScale; text: qsTr("Interrompi riconoscimento")
                visible: overlay.handwriting.busy
                onClicked: overlay.handwriting.cancel()
            }
            Text {
                Layout.fillWidth: true
                text: overlay.handwriting.busy ? qsTr("Riconoscimento in corso…") : overlay.errorText || overlay.handwriting.error
                visible: text.length > 0
                wrapMode: Text.WordWrap; color: "#87332c"; font.pixelSize: 12 * overlay.toolbarScale
            }
            Flow {
                Layout.fillWidth: true
                spacing: 3 * overlay.toolbarScale
                visible: overlay.previewTranscript && overlay.handwriting.alternatives.length > 1
                Repeater {
                    model: overlay.handwriting.alternatives
                    PaperButton {
                        required property string modelData
                        uiScale: overlay.toolbarScale; text: modelData
                        width: Math.min(implicitWidth, toolbar.width - 16 * overlay.toolbarScale)
                        onClicked: transcript.text = modelData
                    }
                }
            }
        }
    }
}
