import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: editor
    objectName: "diagramHandwritingDialog"
    required property Notebook notebook
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    property bool defaultKeepInk: false
    property string recognitionLanguage: "auto"
    property var preferredWords: []
    property var preferredCorrections: []
    property var selection: null
    property var plan: ({})
    property var labelEntries: []
    property int selectedLabelIndex: -1
    property bool removeInk: true
    property string errorText: ""
    property var recognitionQueue: []
    property int recognitionIndex: -1
    property int recognitionGeneration: 0
    property int recognitionTotal: 0
    property int recognitionCompleted: 0
    property bool running: false
    property bool loadingEditor: false
    property bool keyboardOpen: false
    readonly property var labelOrientations: [
        {id: "auto", title: qsTr("Orientamento automatico")},
        {id: "horizontal", title: qsTr("Orizzontale")},
        {id: "stacked", title: qsTr("Lettere in colonna")},
        {id: "clockwise", title: qsTr("Ruota a destra")},
        {id: "counterclockwise", title: qsTr("Ruota a sinistra")}
    ]
    readonly property bool processing: running || recognizer.busy
    readonly property var selectedLabel: selectedLabelIndex >= 0 && selectedLabelIndex < labelEntries.length
        ? labelEntries[selectedLabelIndex] : null
    readonly property int acceptedLabelCount: labelEntries.filter(label => label.hasInk && label.included && label.text.trim().length > 0).length
    readonly property bool canApply: !processing && !Boolean(plan.error) && !Boolean(plan.requiresTarget)
        && Boolean(plan.hasChanges) && Boolean(plan.diagram)
        && Boolean(plan.diagram.nodes) && plan.diagram.nodes.length > 0 && labelEntries.every(label =>
            (!label.included && label.hasInk) || label.text.length <= (label.kind === "edge" ? 120 : 200))
    readonly property var previewDiagram: reviewedDiagram()
    readonly property color ink: "#242723"
    readonly property color muted: "#535862"
    readonly property color blue: "#155cc2"
    readonly property color border: "#d7dce4"
    signal diagramApplied()

    title: qsTr("Riconosci diagramma")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(760 * uiScale, (parent ? parent.width : 760 * uiScale) - 24 * uiScale)
    height: Math.min(1080 * uiScale, (parent ? parent.height : 1080 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    spacing: 10 * uiScale
    modal: true
    dim: !tabletMode
    focus: true
    closePolicy: Popup.CloseOnEscape

    function replaceEntry(index: int, changes: var) {
        if (index < 0 || index >= labelEntries.length) return
        const entries = labelEntries.slice()
        entries[index] = Object.assign({}, entries[index], changes)
        labelEntries = entries
    }

    function loadEditor() {
        loadingEditor = true
        labelInput.text = selectedLabel ? selectedLabel.text : ""
        loadingEditor = false
    }

    function setLabelText(index: int, text: string) {
        if (index < 0 || index >= labelEntries.length || labelEntries[index].preserved) return
        replaceEntry(index, {text: text, status: "manual", error: ""})
        if (index === selectedLabelIndex && labelInput.text !== text) loadEditor()
        errorText = ""
    }

    function setLabelIncluded(index: int, included: bool) {
        replaceEntry(index, {included: included})
        errorText = ""
    }

    function selectLabel(index: int) {
        if (index < 0 || index >= labelEntries.length) return
        // Changing the maximum length must not copy the previous label into
        // the newly selected entry through TextField.onTextChanged.
        loadingEditor = true
        selectedLabelIndex = index
        loadEditor()
    }

    function stopRecognition() {
        ++recognitionGeneration
        recognitionQueue = []
        recognitionIndex = -1
        running = false
        recognizer.cancel()
        labelEntries = labelEntries.map(label => label.status === "recognizing" || label.status === "queued"
            ? Object.assign({}, label, {status: "pending", error: ""}) : label)
    }

    function clearState() {
        stopRecognition()
        selection = null
        plan = ({})
        labelEntries = []
        selectedLabelIndex = -1
        recognitionTotal = 0
        recognitionCompleted = 0
        removeInk = !defaultKeepInk
        errorText = ""
        keyboardOpen = false
        loadEditor()
    }

    function clearPrivateState() {
        clearState()
        close()
    }

    function begin(value: var) {
        clearState()
        selection = value
        open()
        chooseTarget("")
        scroll.contentItem.contentY = 0
    }

    function chooseTarget(targetId: string) {
        stopRecognition()
        keyboardOpen = false
        recognitionTotal = 0
        recognitionCompleted = 0
        plan = notebook.diagramHandwritingPlan(selection, targetId)
        errorText = plan.error || ""
        const groups = plan.labelGroups || []
        const nodes = plan.diagram ? plan.diagram.nodes || [] : []
        const edges = plan.diagram ? plan.diagram.edges || [] : []
        labelEntries = nodes.map((node, index) => ({item: node, kind: "node", number: index + 1}))
            .concat(edges.map((edge, index) => ({item: edge, kind: "edge", number: index + 1})))
            .map(entry => {
                const existing = (entry.kind === "node" ? plan.existingNodeIds || [] : plan.existingEdgeIds || [])
                    .indexOf(entry.item.id) >= 0
                const group = groups.find(candidate => candidate.id === entry.item.id && candidate.kind === entry.kind)
                const preserved = existing
                return {id: entry.item.id, kind: entry.kind, number: entry.number,
                    text: entry.item.text || "", included: true, hasInk: Boolean(group) && !preserved,
                    preserved: preserved, existing: existing, orientation: "auto",
                    strokes: group && !preserved ? group.strokes : [], status: group && !preserved ? "pending" : "empty", error: ""}
            })
        selectedLabelIndex = labelEntries.length ? 0 : -1
        loadEditor()
        if (!errorText.length) startRecognition(labelEntries.map((label, index) => ({label: label, index: index}))
            .filter(entry => entry.label.hasInk).map(entry => entry.index))
    }

    function openFor(value: var) { begin(value) }

    function reviewedDiagram() {
        if (!plan.diagram) return ({nodes: [], edges: []})
        const textFor = function(item, kind) {
            const label = labelEntries.find(entry => entry.id === item.id && entry.kind === kind)
            return label && (!label.hasInk || label.included) ? label.text : item.text || ""
        }
        return Object.assign({}, plan.diagram, {
            nodes: (plan.diagram.nodes || []).map(node => Object.assign({}, node, {text: textFor(node, "node")})),
            edges: (plan.diagram.edges || []).map(edge => Object.assign({}, edge, {text: textFor(edge, "edge")}))
        })
    }

    function startRecognition(indexes: var) {
        stopRecognition()
        if (!indexes.length || !visible) return
        if (!recognizer.available) {
            for (const index of indexes) replaceEntry(index, {status: "error",
                error: qsTr("Riconoscimento del testo non disponibile. Puoi scrivere l’etichetta; i tratti non trascritti restano sulla pagina.")})
            return
        }
        recognitionQueue = indexes.slice()
        recognitionTotal = indexes.length
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
            const label = labelEntries[index]
            if (!label || !label.included || label.status === "manual") {
                if (label && label.status === "queued") replaceEntry(index, {status: "pending"})
                ++recognitionCompleted
                continue
            }
            recognitionIndex = index
            replaceEntry(index, {status: "recognizing", error: ""})
            recognizer.recognizeDiagramLabel(label.strokes,
                recognitionLanguage === "auto" ? notebook.handwritingLanguage : recognitionLanguage, label.orientation)
            return
        }
        recognitionIndex = -1
        running = false
    }

    function retryLabel(index: int) {
        if (processing || index < 0 || index >= labelEntries.length || !labelEntries[index].hasInk) return
        startRecognition([index])
    }

    function retryMissing() {
        if (processing) return
        startRecognition(labelEntries.map((label, index) => ({label: label, index: index}))
            .filter(entry => entry.label.hasInk && entry.label.included && !entry.label.text.trim().length)
            .map(entry => entry.index))
    }

    function submit() {
        if (!canApply) return
        const accepted = labelEntries.filter(label => label.hasInk && label.included && label.text.trim().length > 0)
            .map(label => label.id)
        if (notebook.applyDiagramHandwriting(selection, plan, reviewedDiagram(), accepted, removeInk)) {
            close()
            diagramApplied()
        } else errorText = notebook.error
    }

    function checkSelectionContext() {
        if (selection && (selection.bookId !== notebook.bookId || selection.pageId !== notebook.pageId
                          || selection.layerId !== notebook.activeLayer)) clearPrivateState()
    }

    function labelName(label: var): string {
        if (!label) return ""
        return label.kind === "edge" ? qsTr("Collegamento %1").arg(label.number) : qsTr("Forma %1").arg(label.number)
    }

    function labelStatus(label: var): string {
        if (label.hasInk && !label.included) return qsTr("Mantieni scrittura originale")
        if (label.status === "recognizing") return qsTr("Riconoscimento…")
        if (label.status === "queued") return qsTr("In attesa")
        if (label.text.trim().length) return label.text
        return label.hasInk ? qsTr("Scrittura non trascritta") : qsTr("Aggiungi un’etichetta")
    }

    onClosed: clearState()

    Handwriting {
        id: recognizer
        objectName: "diagramHandwritingRecognizer"
        wordList: editor.preferredWords
        corrections: editor.preferredCorrections
        onFinished: success => {
            const index = editor.recognitionIndex
            if (!editor.running || index < 0 || !editor.visible) return
            editor.recognitionIndex = -1
            if (editor.labelEntries[index] && editor.labelEntries[index].status === "recognizing") {
                editor.replaceEntry(index, success ? {text: result, status: "recognized", error: ""}
                    : {status: "error", error: error || qsTr("Nessun testo riconosciuto. Correggi l’etichetta oppure conserva la scrittura originale.")})
                if (index === editor.selectedLabelIndex) editor.loadEditor()
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

    background: Rectangle { color: "white"; border.color: editor.border; radius: 14 * editor.uiScale }
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
        Layout.fillWidth: true
        color: editor.muted
        textFormat: Text.PlainText
        font.pixelSize: 14 * editor.uiScale
        wrapMode: Text.Wrap
    }
    component Action: PaperButton { uiScale: editor.uiScale }

    contentItem: ColumnLayout {
        spacing: 10 * editor.uiScale
        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 10 * editor.uiScale
                Caption {
                    text: qsTr("Controlla forme, frecce ed etichette prima di applicare. I collegamenti ambigui e i tratti non trascritti rimangono a mano libera.")
                }
                Caption {
                    visible: targetChoice.visible
                    text: qsTr("Scegli il diagramma da aggiornare oppure crea un diagramma separato.")
                }
                ComboBox {
                    id: targetChoice
                    objectName: "diagramRecognitionTarget"
                    Layout.fillWidth: true
                    visible: model.length > 0
                    model: (editor.plan.targetChoices || []).length
                        ? editor.plan.targetChoices.concat([{id: "new", title: qsTr("Crea un diagramma separato")}]) : []
                    textRole: "title"
                    valueRole: "id"
                    currentIndex: model.findIndex(choice => choice.id === editor.plan.targetChoice)
                    displayText: currentIndex < 0 ? qsTr("Scegli un diagramma") : currentText
                    Accessible.name: qsTr("Diagramma da aggiornare")
                    onActivated: editor.chooseTarget(currentValue)
                }
                Caption {
                    objectName: "diagramRecognitionChanges"
                    visible: Boolean(editor.plan.extendsExisting) || Boolean(editor.plan.requiresTarget)
                    text: editor.plan.requiresTarget ? qsTr("Sono presenti più diagrammi: scegli a quale aggiungere i nuovi tratti.")
                        : editor.plan.hasChanges
                            ? qsTr("Aggiornamento del diagramma esistente: %1 nuove forme e %2 nuovi collegamenti. Le etichette esistenti sono conservate.")
                                .arg(editor.plan.addedNodeCount || 0).arg(editor.plan.addedEdgeCount || 0)
                            : qsTr("Nessuna nuova forma o collegamento riconosciuto da aggiungere al diagramma.")
                }
                Rectangle {
                    id: previewFrame
                    readonly property real previewAspect: editor.plan.diagram && editor.plan.diagram.width > 0
                        ? editor.plan.diagram.height * 800 / (editor.plan.diagram.width * 600) : 4 / 3
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(300 * editor.uiScale, width * 0.75)
                    border.color: editor.border
                    color: "white"
                    visible: Boolean(editor.plan.diagram) && Boolean(editor.plan.diagram.nodes) && editor.plan.diagram.nodes.length > 0
                    DiagramItem {
                        objectName: "diagramRecognitionPreview"
                        anchors.centerIn: parent
                        width: Math.max(0, Math.min(parent.width - 16 * editor.uiScale,
                            (parent.height - 16 * editor.uiScale) / previewFrame.previewAspect))
                        height: width * previewFrame.previewAspect
                        diagram: editor.previewDiagram
                        selectedNode: editor.selectedLabel && editor.selectedLabel.kind === "node" ? editor.selectedLabel.id : ""
                        selectedEdge: editor.selectedLabel && editor.selectedLabel.kind === "edge" ? editor.selectedLabel.id : ""
                    }
                }
                Caption {
                    objectName: "diagramRecognitionWarnings"
                    visible: text.length > 0
                    text: (editor.plan.warnings || []).join("\n")
                }
                Caption {
                    objectName: "diagramRecognitionProgress"
                    visible: editor.processing
                    text: qsTr("Etichetta %1 di %2").arg(Math.min(editor.recognitionCompleted + 1, editor.recognitionTotal)).arg(editor.recognitionTotal)
                }
                ProgressBar {
                    Layout.fillWidth: true
                    visible: editor.processing
                    from: 0
                    to: Math.max(1, editor.recognitionTotal)
                    value: editor.recognitionCompleted
                }
                ListView {
                    id: labels
                    objectName: "diagramRecognitionLabels"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, 210 * editor.uiScale)
                    clip: true
                    model: editor.labelEntries
                    spacing: 4 * editor.uiScale
                    delegate: ItemDelegate {
                        id: labelRow
                        required property var modelData
                        required property int index
                        width: labels.width
                        height: 64 * editor.uiScale
                        highlighted: editor.selectedLabelIndex === index
                        Accessible.name: editor.labelName(modelData) + ": " + editor.labelStatus(modelData)
                        contentItem: ColumnLayout {
                            spacing: 3 * editor.uiScale
                            Label {
                                Layout.fillWidth: true
                                text: editor.labelName(labelRow.modelData)
                                textFormat: Text.PlainText
                                font.weight: Font.DemiBold
                                font.pixelSize: 15 * editor.uiScale
                            }
                            Label {
                                Layout.fillWidth: true
                                text: editor.labelStatus(labelRow.modelData)
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                color: editor.muted
                                font.pixelSize: 13 * editor.uiScale
                            }
                        }
                        onClicked: editor.selectLabel(index)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: editor.selectedLabel !== null
                    spacing: 6 * editor.uiScale
                    Caption { text: editor.labelName(editor.selectedLabel) }
                    Caption {
                        visible: editor.selectedLabel !== null && editor.selectedLabel.preserved
                        text: qsTr("Etichetta esistente conservata. Puoi modificarla nell’editor del diagramma dopo la conversione.")
                    }
                    CheckBox {
                        objectName: "diagramRecognitionIncludeLabel"
                        text: qsTr("Inserisci l’etichetta trascritta")
                        visible: editor.selectedLabel !== null && editor.selectedLabel.hasInk
                        checked: editor.selectedLabel !== null && editor.selectedLabel.included
                        onToggled: editor.setLabelIncluded(editor.selectedLabelIndex, checked)
                    }
                    TextField {
                        id: labelInput
                        objectName: "diagramRecognitionLabelText"
                        Layout.fillWidth: true
                        placeholderText: qsTr("Scrivi o correggi l’etichetta")
                        maximumLength: editor.selectedLabel && editor.selectedLabel.kind === "edge" ? 120 : 200
                        selectByMouse: true
                        enabled: editor.selectedLabel !== null && !editor.selectedLabel.preserved
                            && (!editor.selectedLabel.hasInk || editor.selectedLabel.included)
                        Accessible.name: placeholderText
                        onTextChanged: if (!editor.loadingEditor) editor.setLabelText(editor.selectedLabelIndex, text)
                        onActiveFocusChanged: if (activeFocus) editor.keyboardOpen = true
                        onAccepted: editor.keyboardOpen = false
                    }
                    Caption {
                        visible: text.length > 0
                        text: editor.selectedLabel && editor.selectedLabel.text.length > (editor.selectedLabel.kind === "edge" ? 120 : 200)
                            ? qsTr("Accorcia l’etichetta: massimo %1 caratteri.").arg(editor.selectedLabel.kind === "edge" ? 120 : 200)
                            : editor.selectedLabel ? editor.selectedLabel.error : ""
                        color: "#942922"
                    }
                    ComboBox {
                        objectName: "diagramRecognitionLabelOrientation"
                        Layout.fillWidth: true
                        model: editor.labelOrientations
                        textRole: "title"
                        valueRole: "id"
                        currentIndex: editor.selectedLabel
                            ? indexOfValue(editor.selectedLabel.orientation) : 0
                        visible: editor.selectedLabel !== null && editor.selectedLabel.hasInk
                        enabled: !editor.processing && editor.selectedLabel !== null && editor.selectedLabel.included
                        Accessible.name: qsTr("Orientamento per il riconoscimento dell’etichetta")
                        onActivated: editor.replaceEntry(editor.selectedLabelIndex, {orientation: currentValue})
                    }
                    Action {
                        text: qsTr("Riconosci di nuovo questa etichetta")
                        visible: editor.selectedLabel !== null && editor.selectedLabel.hasInk
                        enabled: !editor.processing && editor.selectedLabel !== null && editor.selectedLabel.included
                        onClicked: editor.retryLabel(editor.selectedLabelIndex)
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Action {
                        text: qsTr("Ferma riconoscimento")
                        visible: editor.processing
                        onClicked: editor.stopRecognition()
                    }
                    Action {
                        text: qsTr("Riprova le etichette mancanti")
                        visible: !editor.processing && editor.labelEntries.some(label => label.hasInk && label.included && !label.text.trim().length)
                        onClicked: editor.retryMissing()
                    }
                }
                CheckBox {
                    objectName: "diagramRecognitionRemoveInk"
                    text: qsTr("Sostituisci i tratti convertiti con forme e testo")
                    checked: editor.removeInk
                    onToggled: editor.removeInk = checked
                }
                Caption {
                    text: editor.removeInk
                        ? qsTr("I tratti originali convertiti saranno conservati nel diagramma. Annulla sulla pagina ripristina la conversione nella sessione corrente.")
                        : qsTr("Tutti i tratti originali restano sulla pagina insieme al diagramma modificabile.")
                }
                Caption {
                    objectName: "diagramRecognitionError"
                    visible: text.length > 0
                    text: editor.errorText
                    color: "#942922"
                }
            }
        }
        Action {
            visible: editor.showTouchKeyboard && editor.keyboardOpen
            text: qsTr("Nascondi tastiera")
            onClicked: editor.keyboardOpen = false
        }
        TouchKeyboard {
            Layout.fillWidth: true
            visible: editor.showTouchKeyboard && editor.keyboardOpen && editor.selectedLabel !== null && labelInput.enabled
            uiScale: editor.uiScale
            target: labelInput
        }
        RowLayout {
            Layout.fillWidth: true
            Action { text: qsTr("Annulla"); onClicked: editor.close() }
            Item { Layout.fillWidth: true }
            Action {
                objectName: "diagramRecognitionApply"
                text: editor.plan.extendsExisting ? qsTr("Aggiorna diagramma") : qsTr("Applica diagramma")
                filled: true
                enabled: editor.canApply
                onClicked: editor.submit()
            }
        }
    }
}
