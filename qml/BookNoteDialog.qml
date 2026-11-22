import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: editor
    objectName: "bookNoteDialog"
    required property Notebook notebook
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    property bool editingAllowed: true
    property var originalItem: null
    property var noteInk: []
    property string noteText: ""
    property string noteColor: "yellow"
    property point anchorPoint: Qt.point(0, 0)
    property string sourceBookId: ""
    property string sourcePageId: ""
    property string sourceBookTitle: ""
    property int sourcePageNumber: 0
    property string initialDraft: ""
    property var undoInk: []
    property var redoInk: []
    property string errorText: ""
    property string confirmation: ""
    property bool closeApplication: false
    property bool suspended: false
    property bool keyboardOpen: false
    property string mode: "ink"
    property string inkTool: "pen"
    readonly property bool hasDraft: sourceBookId.length > 0
    readonly property bool drawing: noteCanvas.drawing
    readonly property bool dirty: hasDraft && JSON.stringify({text: noteText, ink: noteInk, color: noteColor}) !== initialDraft
    readonly property Item remoteSurface: opened && !suspended && editingAllowed
        && !noteCanvas.drawing && !colorChoice.popup?.visible && contentItem ? contentItem.parent : null
    readonly property bool currentContext: sourceBookId === notebook.bookId && sourcePageId === notebook.pageId
    readonly property int pointCount: noteInk.reduce((total, stroke) => total + (stroke.points || []).length, 0)
    readonly property bool withinLimits: noteText.length <= 4000 && noteInk.length <= 2000 && pointCount <= 60000
    readonly property bool canSave: visible && !suspended && editingAllowed && hasDraft && currentContext && notebook.canWriteLayer && withinLimits
        && !noteCanvas.drawing && !confirmation.length && (noteText.trim().length > 0 || noteInk.length > 0)
    readonly property Item markerTarget: visible && mode === "ink" && !confirmation.length ? noteCanvas : null
    readonly property color paperColor: ({yellow: "#fff2a6", blue: "#d8eaff", green: "#dff0d5", pink: "#f7dce8"})[noteColor] || "#fff2a6"
    signal activity()
    signal applicationCloseRequested()

    title: originalItem ? qsTr("Modifica post-it") : qsTr("Nuovo post-it")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(650 * uiScale, (parent ? parent.width : 650 * uiScale) - 24 * uiScale)
    height: Math.min(880 * uiScale, (parent ? parent.height : 880 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    spacing: 10 * uiScale
    modal: true
    dim: !tabletMode
    focus: true
    closePolicy: Popup.NoAutoClose
    background: Rectangle { color: "white"; border.color: "#d1d1d6"; radius: 14 * editor.uiScale }

    function resetDraft() {
        noteCanvas.cancelDrawing()
        sourceBookId = ""; sourcePageId = ""; originalItem = null
        noteText = ""; noteInk = []; undoInk = []; redoInk = []
        initialDraft = ""; confirmation = ""; keyboardOpen = false; closeApplication = false
        errorText = ""; suspended = false
    }
    function openFor(item: var, point: point): bool {
        if (!editingAllowed) return false
        if (hasDraft && dirty) { resume(); return false }
        resetDraft()
        originalItem = item && item.id ? JSON.parse(JSON.stringify(item)) : null
        sourceBookId = notebook.bookId; sourcePageId = notebook.pageId; sourcePageNumber = notebook.page + 1
        sourceBookTitle = notebook.title
        anchorPoint = point
        noteText = originalItem ? originalItem.text || "" : ""
        noteInk = originalItem ? originalItem.noteInk || [] : []
        noteColor = originalItem ? originalItem.noteColor || "yellow" : "yellow"
        initialDraft = JSON.stringify({text: noteText, ink: noteInk, color: noteColor})
        mode = noteInk.length || !noteText.length ? "ink" : "text"
        inkTool = "pen"
        open()
        return true
    }
    function suspend() {
        if (!visible) return
        noteCanvas.cancelDrawing(); keyboardOpen = false; suspended = true; close()
    }
    function resume() {
        if (!hasDraft || !editingAllowed) return
        suspended = false; open()
    }
    function requestClose(application: bool) {
        noteCanvas.cancelDrawing()
        closeApplication = application
        if (dirty) { confirmation = "discard"; keyboardOpen = false; if (!visible) resume() }
        else finishClose()
    }
    function finishClose() {
        const application = closeApplication
        close()
        if (application) {
            resetDraft()
            applicationCloseRequested()
        }
    }
    function replaceInk(value: var) {
        const history = undoInk.concat([noteInk])
        while (history.length > 1 && (history.length > 20 || JSON.stringify(history).length > 2 * 1024 * 1024)) history.shift()
        undoInk = history; redoInk = []; noteInk = value; errorText = ""; activity()
    }
    function stepInk(backwards: bool) {
        noteCanvas.cancelDrawing()
        const from = backwards ? undoInk : redoInk
        if (!from.length) return
        const previous = from[from.length - 1]
        if (backwards) { redoInk = redoInk.concat([noteInk]); undoInk = from.slice(0, -1) }
        else { undoInk = undoInk.concat([noteInk]); redoInk = from.slice(0, -1) }
        noteInk = previous; activity()
    }
    function save() {
        if (!canSave) return
        const item = Object.assign({}, originalItem || {}, {type: "note", text: noteText, noteInk: noteInk, noteColor: noteColor})
        if (!originalItem) {
            item.width = 0.05; item.height = 0.0375
            item.x = Math.max(0, Math.min(1 - item.width, anchorPoint.x - item.width / 2))
            item.y = Math.max(0, Math.min(1 - item.height, anchorPoint.y - item.height / 2))
        }
        if (notebook.savePageNote(item, sourceBookId, sourcePageId, originalItem || ({}))) finishClose()
        else errorText = notebook.error
    }
    function deleteNote() {
        if (!visible || suspended || !editingAllowed || !originalItem || !currentContext || !notebook.canWriteLayer) return
        if (notebook.removePageNote(originalItem.id, sourceBookId, sourcePageId, originalItem)) close()
        else { confirmation = ""; errorText = notebook.error }
    }
    onClosed: if (!suspended) resetDraft()
    Shortcut { sequence: "Escape"; enabled: editor.visible; onActivated: editor.requestClose(false) }
    component Action: PaperButton { uiScale: editor.uiScale }
    component Caption: Label {
        Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText
        font.pixelSize: 14 * editor.uiScale; color: "#515159"
    }

    contentItem: ColumnLayout {
        spacing: 10 * editor.uiScale
        Caption { text: qsTr("%1 · Pagina %2. Il post-it rimane nel punto scelto; puoi usare insieme testo e scrittura a mano.").arg(editor.sourceBookTitle).arg(editor.sourcePageNumber) }
        RowLayout {
            Layout.fillWidth: true
            Action { text: qsTr("Penna"); filled: editor.mode === "ink"; onClicked: { noteCanvas.cancelDrawing(); editor.mode = "ink"; editor.keyboardOpen = false } }
            Action {
                property bool remoteClickable: !editor.confirmation.length
                text: qsTr("Testo")
                filled: editor.mode === "text"
                onClicked: { noteCanvas.cancelDrawing(); editor.mode = "text"; textInput.forceActiveFocus(); editor.keyboardOpen = true }
            }
            Item { Layout.fillWidth: true }
            ComboBox {
                id: colorChoice
                objectName: "bookNoteColor"
                model: [{id: "yellow", title: qsTr("Giallo")}, {id: "blue", title: qsTr("Blu")}, {id: "green", title: qsTr("Verde")}, {id: "pink", title: qsTr("Rosa")}]
                textRole: "title"; valueRole: "id"
                currentIndex: model.findIndex(value => value.id === editor.noteColor)
                Accessible.name: qsTr("Colore del post-it")
                onActivated: { editor.noteColor = currentValue; editor.activity() }
            }
        }
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: editor.mode === "ink"
            Rectangle {
                anchors.centerIn: parent
                width: Math.max(1, Math.min(parent.width, parent.height * 1.5))
                height: width / 1.5
                color: editor.paperColor; border.color: "#6a6041"
                InkItem {
                    id: noteCanvas
                    objectName: "bookNoteInk"
                    anchors.fill: parent; anchors.margins: 1
                    nativeMarkerEnabled: false
                    enabled: editor.visible && editor.mode === "ink" && !editor.confirmation.length
                        && (editor.withinLimits || editor.inkTool === "eraser")
                    acceptsMouse: !editor.tabletMode
                    strokes: editor.noteInk
                    inkTool: editor.inkTool
                    inkWidth: editor.inkTool === "eraser" ? 10 : 2
                    inkColor: "#242723"
                    onInputActivity: editor.activity()
                    onStrokeCompleted: stroke => editor.replaceInk(editor.noteInk.concat([stroke]))
                    onStrokesErased: indexes => editor.replaceInk(editor.noteInk.filter((stroke, index) => !indexes.includes(index)))
                }
            }
        }
        RowLayout {
            visible: editor.mode === "ink"
            Action { text: qsTr("Penna"); filled: editor.inkTool === "pen"; onClicked: editor.inkTool = "pen" }
            Action { text: qsTr("Gomma"); filled: editor.inkTool === "eraser"; onClicked: editor.inkTool = "eraser" }
            Action { objectName: "bookNoteUndo"; text: qsTr("Annulla tratto"); iconName: "undo"; iconOnly: editor.availableWidth < 420 * editor.uiScale; enabled: editor.undoInk.length > 0; onClicked: editor.stepInk(true) }
            Action { text: qsTr("Ripeti"); iconName: "redo"; iconOnly: editor.availableWidth < 420 * editor.uiScale; enabled: editor.redoInk.length > 0; onClicked: editor.stepInk(false) }
        }
        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: editor.mode === "text"
            clip: true
            TextArea {
                id: textInput
                objectName: "bookNoteText"
                property bool remoteEditable: !editor.confirmation.length
                text: editor.noteText
                placeholderText: qsTr("Scrivi la nota…")
                wrapMode: TextEdit.Wrap; selectByMouse: true
                font.pixelSize: 17 * editor.uiScale
                background: Rectangle { color: editor.paperColor }
                onTextChanged: { editor.noteText = text; if (editor.visible) editor.activity() }
                onActiveFocusChanged: if (activeFocus) editor.keyboardOpen = true
            }
        }
        Caption {
            objectName: "bookNoteError"
            color: "#942922"
            visible: text.length > 0
            text: !editor.currentContext && editor.hasDraft ? qsTr("La pagina è cambiata. La bozza è conservata: torna alla pagina originale prima di salvarla.")
                : !editor.withinLimits ? qsTr("La nota supera il limite: 4000 caratteri, 2000 tratti o 60000 punti. Riduci il contenuto prima di salvare.")
                : editor.errorText
        }
        Caption {
            visible: editor.confirmation.length > 0
            text: editor.confirmation === "delete" ? qsTr("Eliminare questo post-it? Potrai ripristinarlo con Annulla sulla pagina.") : qsTr("Scartare le modifiche al post-it?")
        }
        Action {
            property bool remoteClickable: true
            visible: editor.showTouchKeyboard && editor.keyboardOpen && editor.mode === "text"
            text: qsTr("Nascondi tastiera")
            onClicked: editor.keyboardOpen = false
        }
        TouchKeyboard {
            objectName: "bookNoteKeyboard"
            Layout.fillWidth: true
            visible: editor.showTouchKeyboard && editor.keyboardOpen && editor.mode === "text" && !editor.confirmation.length
            uiScale: editor.uiScale; target: textInput
        }
        RowLayout {
            Layout.fillWidth: true
            Action {
                property bool remoteClickable: true
                visible: !editor.confirmation.length
                text: qsTr("Annulla")
                onClicked: editor.requestClose(false)
            }
            Action { visible: Boolean(editor.originalItem) && !editor.confirmation.length; text: qsTr("Elimina"); onClicked: { noteCanvas.cancelDrawing(); editor.confirmation = "delete"; editor.keyboardOpen = false } }
            Action {
                property bool remoteClickable: true
                visible: editor.confirmation.length > 0
                text: qsTr("Continua a modificare")
                onClicked: { editor.confirmation = ""; editor.closeApplication = false }
            }
            Item { Layout.fillWidth: true }
            Action { visible: editor.confirmation.length > 0; text: editor.confirmation === "delete" ? qsTr("Elimina post-it") : qsTr("Scarta modifiche"); onClicked: editor.confirmation === "delete" ? editor.deleteNote() : editor.finishClose() }
            Action {
                objectName: "bookNoteSave"
                property bool remoteClickable: true
                visible: !editor.confirmation.length
                text: qsTr("Salva post-it")
                filled: true
                enabled: editor.canSave
                onClicked: editor.save()
            }
        }
    }
}
