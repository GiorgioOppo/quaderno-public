import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: editor
    objectName: "tableHandwritingDialog"

    required property Notebook notebook
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    property var selection: null
    property var tableChoices: []
    property int selectedTableIndex: -1
    property var plan: ({})
    property var cellEntries: []
    property int selectedCellIndex: -1
    property bool removeInk: true
    property bool defaultKeepInk: false
    property string recognitionLanguage: "auto"
    property bool numericRecognition: false
    property var preferredWords: []
    property var preferredCorrections: []
    property bool overwriteExisting: false
    property string errorText: ""
    property var recognitionQueue: []
    property int recognitionIndex: -1
    property int recognitionGeneration: 0
    property int recognitionTotal: 0
    property int recognitionCompleted: 0
    property bool running: false
    property bool loadingEditor: false
    readonly property bool processing: running || recognizer.busy
    readonly property var selectedCell: selectedCellIndex >= 0 && selectedCellIndex < cellEntries.length
        ? cellEntries[selectedCellIndex] : null
    readonly property int includedCount: cellEntries.filter(cell => cell.included).length
    readonly property bool needsOverwrite: cellEntries.some(cell => cell.included && cell.previous.length > 0)
    readonly property bool validUpdates: includedCount > 0 && cellEntries.every(cell =>
        !cell.included || (cell.text.trim().length > 0 && cell.text.length <= 500))
    readonly property bool canApply: !processing && !plan.error && validUpdates && (!needsOverwrite || overwriteExisting)
    readonly property color ink: "#242723"
    readonly property color muted: "#535862"
    readonly property color blue: "#155cc2"
    readonly property color border: "#d7dce4"
    readonly property color surface: "#f6f7f9"
    signal cellsApplied()

    title: qsTr("Scrittura nelle celle")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(620 * uiScale, (parent ? parent.width : 620 * uiScale) - 24 * uiScale)
    height: Math.min(860 * uiScale, (parent ? parent.height : 900 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    spacing: 10 * uiScale
    modal: true
    dim: !tabletMode
    focus: true
    closePolicy: Popup.CloseOnEscape

    function replaceEntry(index: int, changes: var) {
        if (index < 0 || index >= cellEntries.length) return
        const entries = cellEntries.slice()
        entries[index] = Object.assign({}, entries[index], changes)
        cellEntries = entries
    }

    function loadEditor() {
        loadingEditor = true
        cellInput.text = selectedCell ? selectedCell.text : ""
        loadingEditor = false
    }

    function selectCell(index: int) {
        if (index < 0 || index >= cellEntries.length) return
        selectedCellIndex = index
        loadEditor()
        if (recognitionIndex !== index) cellInput.forceActiveFocus()
    }

    function setCellText(index: int, text: string) {
        if (index < 0 || index >= cellEntries.length) return
        replaceEntry(index, {text, status: "manual", error: ""})
        errorText = ""
        if (index === selectedCellIndex && cellInput.text !== text) loadEditor()
    }

    function setCellIncluded(index: int, included: bool) {
        replaceEntry(index, {included})
        errorText = ""
    }

    function stopRecognition() {
        ++recognitionGeneration
        recognitionQueue = []
        recognitionIndex = -1
        running = false
        recognizer.cancel()
        cellEntries = cellEntries.map(cell => cell.status === "recognizing" || cell.status === "queued"
            ? Object.assign({}, cell, {status: "pending", error: ""}) : cell)
    }

    function clearState() {
        stopRecognition()
        selection = null
        tableChoices = []
        selectedTableIndex = -1
        plan = ({})
        cellEntries = []
        selectedCellIndex = -1
        recognitionTotal = 0
        recognitionCompleted = 0
        removeInk = !defaultKeepInk
        overwriteExisting = false
        errorText = ""
        loadEditor()
    }

    function clearPrivateState() {
        clearState()
        close()
    }

    function openFor(value: var) {
        clearState()
        selection = value
        tableChoices = notebook.pageItems.filter(item => item.type === "table").map((item, index) => ({
            id: item.id,
            label: qsTr("Tabella %1 · %2 × %3").arg(index + 1).arg(item.rows).arg(item.cols)
                + (item.cells.slice(0, item.cols).filter(cell => String(cell).trim().length).length
                   ? " — " + item.cells.slice(0, item.cols).filter(cell => String(cell).trim().length).join(" · ").slice(0, 60) : "")
        }))
        let best = -1
        let mapped = -1
        for (let index = 0; index < tableChoices.length; ++index) {
            const candidate = notebook.tableHandwritingPlan(selection, tableChoices[index].id)
            const count = candidate.error ? -1 : (candidate.cells || []).reduce((total, cell) => total + cell.indexes.length, 0)
            if (count > mapped) { best = index; mapped = count }
        }
        if (best < 0 && tableChoices.length) best = 0
        open()
        if (best >= 0) chooseTable(best)
        else errorText = qsTr("Aggiungi una tabella alla pagina, scrivi nelle celle e seleziona i tratti con il lazo.")
    }

    function chooseTable(index: int) {
        stopRecognition()
        selectedTableIndex = index
        overwriteExisting = false
        errorText = ""
        if (!selection || index < 0 || index >= tableChoices.length) return
        plan = notebook.tableHandwritingPlan(selection, tableChoices[index].id)
        cellEntries = (plan.cells || []).map(cell => Object.assign({}, cell,
            {text: "", included: true, status: "pending", error: ""}))
        selectedCellIndex = cellEntries.length ? 0 : -1
        loadEditor()
        errorText = plan.error || ""
        if (!errorText.length && !cellEntries.length)
            errorText = qsTr("Nessun tratto assegnabile a questa tabella. Scrivi all’interno delle celle e ripeti la selezione.")
        scroll.contentItem.contentY = 0
        startRecognition(cellEntries.map((cell, cellIndex) => cellIndex))
    }

    function startRecognition(indexes: var) {
        stopRecognition()
        if (!indexes.length || !visible) return
        if (!recognizer.available) {
            for (const index of indexes)
                replaceEntry(index, {status: "error", error: qsTr("Riconoscimento non disponibile. Inserisci il valore manualmente.")})
            return
        }
        recognitionQueue = indexes.slice()
        recognitionTotal = recognitionQueue.length
        recognitionCompleted = 0
        for (const index of indexes) replaceEntry(index, {status: "queued", error: ""})
        running = true
        const generation = recognitionGeneration
        Qt.callLater(function() { editor.nextRecognition(generation) })
    }

    function nextRecognition(generation: int) {
        if (generation !== recognitionGeneration || !visible || !running) return
        while (recognitionQueue.length) {
            const index = recognitionQueue[0]
            recognitionQueue = recognitionQueue.slice(1)
            const cell = cellEntries[index]
            // A manual correction made while another cell was processing takes priority.
            if (!cell || cell.status === "manual" || !cell.included) {
                if (cell && cell.status === "queued") replaceEntry(index, {status: "pending"})
                ++recognitionCompleted
                continue
            }
            recognitionIndex = index
            replaceEntry(index, {status: "recognizing", error: ""})
            recognizer.recognize(cell.strokes, recognitionLanguage === "auto" ? notebook.handwritingLanguage : recognitionLanguage)
            return
        }
        recognitionIndex = -1
        running = false
    }

    function retryRecognition(index: int) {
        if (processing || index < 0 || index >= cellEntries.length) return
        startRecognition([index])
    }

    function retryMissing() {
        if (processing) return
        startRecognition(cellEntries.map((cell, index) => ({cell, index}))
            .filter(entry => entry.cell.included && !entry.cell.text.trim().length).map(entry => entry.index))
    }

    function submit() {
        if (!canApply) return
        const updates = cellEntries.filter(cell => cell.included).map(cell =>
            ({row: cell.row, column: cell.column, text: cell.text}))
        if (notebook.applyTableHandwriting(selection, plan, updates, removeInk, overwriteExisting)) {
            close()
            cellsApplied()
        } else errorText = notebook.error
    }

    function checkSelectionContext() {
        if (selection && (selection.bookId !== notebook.bookId || selection.pageId !== notebook.pageId
                          || selection.layerId !== notebook.activeLayer))
            clearPrivateState()
    }

    function cellStatus(cell: var): string {
        if (!cell.included) return qsTr("Esclusa")
        if (cell.status === "recognizing") return qsTr("Riconoscimento…")
        if (cell.status === "queued") return qsTr("In attesa")
        if (cell.text.length > 500) return qsTr("Oltre 500 caratteri")
        if (cell.text.trim().length) return qsTr("Da rivedere")
        return cell.error.length ? qsTr("Da correggere") : qsTr("Da completare")
    }

    onClosed: clearState()

    Handwriting {
        id: recognizer
        numericMode: editor.numericRecognition
        wordList: editor.preferredWords
        corrections: editor.preferredCorrections
        objectName: "tableHandwritingRecognizer"
        onFinished: success => {
            const index = editor.recognitionIndex
            if (!editor.running || index < 0 || !editor.visible) return
            editor.recognitionIndex = -1
            if (editor.cellEntries[index] && editor.cellEntries[index].status === "recognizing") {
                editor.replaceEntry(index, success
                    ? {text: result, status: "recognized", error: ""}
                    : {status: "error", error: error || qsTr("Nessun testo riconosciuto. Correggi il valore oppure escludi la cella.")})
                if (index === editor.selectedCellIndex) editor.loadEditor()
            }
            ++editor.recognitionCompleted
            const generation = editor.recognitionGeneration
            Qt.callLater(function() { editor.nextRecognition(generation) })
        }
    }

    Connections {
        target: editor.notebook
        function onDocumentChanged() { editor.checkSelectionContext() }
        function onLayersChanged() { editor.checkSelectionContext() }
    }

    background: Rectangle {
        color: "white"
        border.color: editor.border
        radius: 14 * editor.uiScale
    }

    header: Label {
        text: editor.title
        color: editor.ink
        font.pixelSize: 22 * editor.uiScale
        font.weight: Font.DemiBold
        leftPadding: editor.leftPadding
        rightPadding: editor.rightPadding
        topPadding: editor.topPadding
    }

    component Caption: Label {
        color: editor.muted
        textFormat: Text.PlainText
        font.pixelSize: 14 * editor.uiScale
        wrapMode: Text.Wrap
    }

    contentItem: ScrollView {
        id: scroll
        clip: true
        contentWidth: availableWidth
        contentHeight: form.implicitHeight
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: form
            width: scroll.availableWidth
            spacing: 9 * editor.uiScale

            Caption {
                Layout.fillWidth: true
                text: qsTr("I tratti vengono riconosciuti separatamente per cella. Rivedi valori e destinazioni prima di applicare.")
            }

            ComboBox {
                objectName: "tableHandwritingTarget"
                Layout.fillWidth: true
                model: editor.tableChoices
                textRole: "label"
                currentIndex: editor.selectedTableIndex
                enabled: !editor.processing && editor.tableChoices.length > 1
                font.pixelSize: 14 * editor.uiScale
                Accessible.name: qsTr("Tabella di destinazione")
                onActivated: index => editor.chooseTable(index)
            }

            Caption {
                Layout.fillWidth: true
                visible: (editor.plan.unassignedCount || 0) > 0
                text: qsTr("%1 tratti fuori dalle celle o sui bordi restano sulla pagina.").arg(editor.plan.unassignedCount || 0)
            }

            RowLayout {
                Layout.fillWidth: true
                visible: editor.processing
                Caption {
                    Layout.fillWidth: true
                    text: qsTr("Riconoscimento: %1 di %2 celle").arg(editor.recognitionCompleted).arg(editor.recognitionTotal)
                        + (editor.recognitionIndex >= 0 ? " · " + editor.cellEntries[editor.recognitionIndex].address : "")
                }
                PaperButton {
                    objectName: "tableHandwritingStop"
                    uiScale: editor.uiScale
                    text: qsTr("Interrompi")
                    onClicked: editor.stopRecognition()
                }
            }

            ListView {
                id: cells
                objectName: "tableHandwritingCells"
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(4, editor.cellEntries.length) * 62 * editor.uiScale
                model: editor.cellEntries
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: ItemDelegate {
                    id: cellRow
                    required property int index
                    required property var modelData
                    width: cells.width
                    height: 62 * editor.uiScale
                    padding: 7 * editor.uiScale
                    Accessible.name: qsTr("Cella %1").arg(modelData.address)
                    onClicked: editor.selectCell(index)
                    background: Rectangle {
                        color: cellRow.index === editor.selectedCellIndex ? "#e8f1fd" : "white"
                        border.color: cellRow.index === editor.selectedCellIndex ? editor.blue : editor.border
                    }
                    contentItem: RowLayout {
                        spacing: 8 * editor.uiScale
                        CheckBox {
                            checked: cellRow.modelData.included
                            Accessible.name: qsTr("Includi cella %1").arg(cellRow.modelData.address)
                            onToggled: editor.setCellIncluded(cellRow.index, checked)
                        }
                        Caption {
                            text: cellRow.modelData.address
                            color: editor.ink
                            font.weight: Font.DemiBold
                            Layout.minimumWidth: 28 * editor.uiScale
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2 * editor.uiScale
                            Text {
                                Layout.fillWidth: true
                                text: cellRow.modelData.text.length ? cellRow.modelData.text.replace(/\n/g, " ") : "—"
                                textFormat: Text.PlainText
                                color: editor.ink
                                font.pixelSize: 14 * editor.uiScale
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: editor.cellStatus(cellRow.modelData)
                                textFormat: Text.PlainText
                                color: cellRow.modelData.error.length || cellRow.modelData.text.length > 500 ? "#87332c" : editor.muted
                                font.pixelSize: 12 * editor.uiScale
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            Caption {
                Layout.fillWidth: true
                visible: editor.selectedCell !== null
                text: editor.selectedCell ? qsTr("Cella %1 · valore attuale: %2")
                    .arg(editor.selectedCell.address).arg(editor.selectedCell.previous || qsTr("vuota")) : ""
                font.weight: Font.DemiBold
            }

            Caption {
                Layout.fillWidth: true
                visible: editor.selectedCell !== null && editor.selectedCell.previous.trim().startsWith("=")
                text: qsTr("Questa cella contiene una formula: applicare un nuovo valore la sostituirà.")
                color: "#87332c"
            }

            ScrollView {
                visible: editor.selectedCell !== null
                Layout.fillWidth: true
                Layout.preferredHeight: 76 * editor.uiScale
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                TextArea {
                    id: cellInput
                    objectName: "tableHandwritingValue"
                    readOnly: editor.recognitionIndex === editor.selectedCellIndex
                    color: editor.ink
                    placeholderText: qsTr("Correggi o inserisci il valore della cella")
                    placeholderTextColor: editor.muted
                    selectionColor: editor.blue
                    selectedTextColor: "white"
                    font.pixelSize: 16 * editor.uiScale
                    padding: 10 * editor.uiScale
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    Accessible.name: editor.selectedCell ? qsTr("Nuovo valore della cella %1").arg(editor.selectedCell.address) : placeholderText
                    onTextChanged: if (!editor.loadingEditor && editor.selectedCellIndex >= 0)
                        editor.setCellText(editor.selectedCellIndex, text)
                    background: Rectangle {
                        color: editor.surface
                        radius: 7 * editor.uiScale
                        border.color: cellInput.activeFocus ? editor.blue : editor.border
                        border.width: cellInput.activeFocus ? Math.max(2, editor.uiScale) : Math.max(1, editor.uiScale)
                    }
                }
            }

            Caption {
                Layout.fillWidth: true
                visible: editor.selectedCell !== null && (editor.selectedCell.error.length > 0 || editor.selectedCell.text.length > 500)
                text: editor.selectedCell ? (editor.selectedCell.text.length > 500
                    ? qsTr("La cella accetta al massimo 500 caratteri. Riduci il testo prima di applicare.") : editor.selectedCell.error) : ""
                color: "#87332c"
            }

            Flow {
                Layout.fillWidth: true
                visible: editor.cellEntries.length > 0
                spacing: 5 * editor.uiScale
                PaperButton {
                    objectName: "tableHandwritingRetryCell"
                    uiScale: editor.uiScale
                    text: qsTr("Riconosci questa cella")
                    enabled: !editor.processing && recognizer.available && editor.selectedCell !== null
                    onClicked: editor.retryRecognition(editor.selectedCellIndex)
                }
                PaperButton {
                    uiScale: editor.uiScale
                    text: qsTr("Riprova celle vuote")
                    enabled: !editor.processing && recognizer.available
                        && editor.cellEntries.some(cell => cell.included && !cell.text.trim().length)
                    onClicked: editor.retryMissing()
                }
            }

            CheckBox {
                id: overwriteOption
                objectName: "tableHandwritingOverwrite"
                Layout.fillWidth: true
                visible: editor.needsOverwrite
                checked: editor.overwriteExisting
                text: qsTr("Sostituisci i valori esistenti, incluse le formule, nelle celle incluse")
                onToggled: editor.overwriteExisting = checked
                contentItem: Caption {
                    leftPadding: parent.indicator.width + parent.spacing
                    text: overwriteOption.text
                    color: editor.ink
                }
            }

            CheckBox {
                id: removeInkOption
                objectName: "tableHandwritingRemoveInk"
                Layout.fillWidth: true
                visible: editor.cellEntries.length > 0
                checked: editor.removeInk
                text: qsTr("Rimuovi dalla pagina i tratti convertiti")
                onToggled: editor.removeInk = checked
                contentItem: Caption {
                    leftPadding: parent.indicator.width + parent.spacing
                    text: removeInkOption.text
                    color: editor.ink
                }
            }

            Caption {
                Layout.fillWidth: true
                visible: editor.cellEntries.length > 0
                text: editor.removeInk
                    ? qsTr("Annulla ripristina celle e tratti insieme prima di cambiare pagina o livello o chiudere l’app. Deseleziona l’opzione per conservare anche la scrittura.")
                    : qsTr("La scrittura originale rimane sulla pagina. Le celle escluse e gli altri contenuti non vengono modificati.")
            }

            Caption {
                Layout.fillWidth: true
                visible: editor.errorText.length > 0
                text: editor.errorText
                color: "#87332c"
            }

            Caption {
                Layout.fillWidth: true
                visible: !editor.processing && editor.includedCount > 0 && !editor.validUpdates
                text: qsTr("Completa il valore di ogni cella inclusa oppure escludila dalla conversione.")
            }
        }
    }

    footer: ColumnLayout {
        spacing: 8 * editor.uiScale
        TouchKeyboard {
            objectName: "tableHandwritingKeyboard"
            Layout.fillWidth: true
            Layout.leftMargin: editor.leftPadding
            Layout.rightMargin: editor.rightPadding
            visible: editor.showTouchKeyboard && editor.selectedCell !== null && !cellInput.readOnly
            uiScale: editor.uiScale
            target: cellInput
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: editor.leftPadding
            Layout.rightMargin: editor.rightPadding
            Layout.bottomMargin: editor.bottomPadding
            spacing: 6 * editor.uiScale
            Caption {
                Layout.fillWidth: true
                text: qsTr("%1 celle incluse").arg(editor.includedCount)
            }
            PaperButton {
                uiScale: editor.uiScale
                text: qsTr("Annulla")
                onClicked: editor.close()
            }
            PaperButton {
                objectName: "tableHandwritingApply"
                uiScale: editor.uiScale
                text: qsTr("Applica alle celle")
                filled: true
                enabled: editor.canApply
                onClicked: editor.submit()
            }
        }
    }
}
