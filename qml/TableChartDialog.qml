pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: editor

    required property Notebook notebook
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    signal saveRequested(var chart)
    signal deleteRequested(string id)

    property string targetBookId: ""
    property string targetPageId: ""
    property bool newNotebook: false
    property bool choosingSource: false
    property bool libraryScope: false
    property var selectedSource: null
    property var original: ({})
    property string errorText: ""
    property bool confirmingDelete: false
    readonly property bool samePage: notebook.bookId === targetBookId && notebook.pageId === targetPageId
    readonly property bool sourceIsSnapshot: !!selectedSource && (newNotebook || !isOnTargetPage(selectedSource))
    readonly property var sourceEntries: {
        if (!choosingSource)
            return []
        const duplicateLabels = new Map()
        return notebook.libraryItems
            .filter(entry => entry.type === "table" && !entry.empty
                && (newNotebook || libraryScope !== isOnTargetPage(entry)))
            .map(entry => {
                const label = sourceLabel(entry)
                const occurrence = (duplicateLabels.get(label) || 0) + 1
                duplicateLabels.set(label, occurrence)
                return { entry: entry, label: occurrence === 1 ? label : qsTr("%1 (%2)").arg(label).arg(occurrence) }
            })
    }
    readonly property var source: {
        if (!selectedSource)
            return ({})
        if (!sourceIsSnapshot)
            return notebook.pageItems.find(entry => entry.type === "table" && entry.id === selectedSource.itemId) || ({})
        const entry = notebook.libraryItems.find(candidate => matchesSource(candidate, selectedSource))
        return entry ? notebook.libraryItemCopy(entry.bookId, entry.pageId, entry.itemId) : ({})
    }
    readonly property var options: ({
        id: original.id || "", sourceTable: selectedSource?.itemId || "", chartKind: kind.currentIndex === 1 ? "bars" : "line",
        title: titleField.text.trim(), hasHeader: headerToggle.checked, xColumn: axisX.currentIndex + 1,
        yColumn: axisY.currentIndex + 1
    })
    readonly property var preview: visible && samePage && source.cols ? buildChart() : ({})

    title: original.id ? qsTr("Modifica grafico da tabella") : qsTr("Grafico da tabella")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(490 * uiScale, parent.width - 24 * uiScale)
    padding: 14 * uiScale
    modal: true
    dim: !tabletMode

    function isOnTargetPage(entry: var): bool {
        return entry.bookId === targetBookId && entry.pageId === targetPageId
    }

    function matchesSource(left: var, right: var): bool {
        return !!left && !!right && left.bookId === right.bookId && left.pageId === right.pageId && left.itemId === right.itemId
    }

    function sourceLabel(entry: var): string {
        const name = entry.title || qsTr("Tabella senza nome")
        return isOnTargetPage(entry) && !newNotebook
            ? name
            : qsTr("%1 · %2, pag. %3").arg(name).arg(entry.bookTitle).arg(entry.pageNumber)
    }

    function load(item: var, draftTitle: string) {
        targetBookId = notebook.bookId
        targetPageId = notebook.pageId
        original = item || ({})
        errorText = ""
        confirmingDelete = false
        titleField.text = original.title || draftTitle
        headerToggle.checked = original.hasHeader === undefined ? true : original.hasHeader
        kind.currentIndex = original.chartKind === "bars" ? 1 : 0
    }

    function openFor(table: var, item: var) {
        load(item, "")
        newNotebook = false
        libraryScope = false
        const id = table?.id || original.sourceTable
        selectedSource = notebook.libraryItems.find(entry => isOnTargetPage(entry) && entry.itemId === id) || null
        choosingSource = !selectedSource
        if (choosingSource)
            libraryScope = libraryTablesOnly()
        axisX.currentIndex = (original.xColumn || 1) - 1
        axisY.currentIndex = (original.yColumn || 2) - 1
        open()
    }

    function openSources(draftTitle: string, createNotebook: bool) {
        load(null, draftTitle)
        newNotebook = createNotebook
        choosingSource = true
        selectedSource = null
        setScope(createNotebook || libraryTablesOnly())
        open()
    }

    function libraryTablesOnly(): bool {
        return !notebook.libraryItems.some(entry => entry.type === "table" && isOnTargetPage(entry))
    }

    function setScope(useLibrary: bool) {
        libraryScope = useLibrary
        selectSource(sourceEntries.length > 0 ? sourceEntries[0].entry : null)
    }

    function selectSource(entry: var) {
        selectedSource = entry
        axisX.currentIndex = source.cols > 0 ? 0 : -1
        axisY.currentIndex = source.cols > 1 ? 1 : -1
        errorText = ""
    }

    function columnLabel(index: int): string {
        if (index < 0)
            return ""
        const column = String.fromCharCode(65 + index)
        const heading = headerToggle.checked ? String(source.cells?.[index] || "").trim().slice(0, 40) : ""
        return heading.length > 0 ? column + " · " + heading : column
    }

    function buildChart(): var {
        if (!samePage || !selectedSource)
            return ({})
        if (sourceIsSnapshot)
            return notebook.tableChartSnapshot(selectedSource.bookId, selectedSource.pageId, selectedSource.itemId, options)
        return notebook.tableChartPreview(options)
    }

    function save() {
        const chart = buildChart()
        if (chart.points)
            saveRequested(chart)
        else
            errorText = qsTr("La pagina o la tabella è cambiata. Controlla l’origine dei dati.")
    }

    onClosed: selectedSource = null

    contentItem: ScrollView {
        id: scroll
        clip: true
        implicitHeight: Math.min(form.implicitHeight, Math.max(92 * editor.uiScale,
            editor.parent.height - 155 * editor.uiScale))
        contentWidth: availableWidth
        contentHeight: form.implicitHeight
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: form
            width: scroll.availableWidth
            spacing: 8 * editor.uiScale

            RowLayout {
                Layout.fillWidth: true
                visible: !editor.choosingSource && Boolean(editor.selectedSource)
                Label {
                    Layout.fillWidth: true
                    text: editor.selectedSource ? qsTr("Origine dati · %1").arg(editor.sourceLabel(editor.selectedSource)) : ""
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                }
                PaperButton {
                    uiScale: editor.uiScale
                    text: qsTr("Cambia")
                    Accessible.name: qsTr("Cambia tabella di origine")
                    onClicked: editor.choosingSource = true
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: editor.choosingSource && !editor.newNotebook
                PaperButton {
                    objectName: "chartSourceThisPage"
                    Layout.fillWidth: true
                    uiScale: editor.uiScale
                    text: qsTr("Questa pagina")
                    filled: !editor.libraryScope
                    onClicked: if (editor.libraryScope) editor.setScope(false)
                }
                PaperButton {
                    objectName: "chartSourceLibrary"
                    Layout.fillWidth: true
                    uiScale: editor.uiScale
                    text: qsTr("Altre pagine")
                    filled: editor.libraryScope
                    onClicked: if (!editor.libraryScope) editor.setScope(true)
                }
            }

            ComboBox {
                id: sourcePicker
                objectName: "chartSourceTable"
                Layout.fillWidth: true
                Layout.preferredHeight: 42 * editor.uiScale
                visible: editor.choosingSource && count > 0
                Accessible.name: qsTr("Tabella di origine")
                textRole: "label"
                model: editor.sourceEntries
                currentIndex: editor.sourceEntries.findIndex(option => editor.matchesSource(option.entry, editor.selectedSource))
                onActivated: index => editor.selectSource(editor.sourceEntries[index].entry)
            }

            Label {
                Layout.fillWidth: true
                visible: editor.source.cols > 0
                wrapMode: Text.WordWrap
                text: editor.sourceIsSnapshot ? qsTr("Copia i valori attuali. Per includere modifiche future, crea un nuovo grafico dalla tabella.")
                    : qsTr("Il grafico si aggiorna quando modifichi la tabella.")
            }

            ChartItem {
                Layout.fillWidth: true
                Layout.preferredHeight: 132 * editor.uiScale
                visible: !!editor.preview.points
                chart: editor.preview
            }

            TextField {
                id: titleField
                Layout.fillWidth: true
                visible: editor.source.cols > 0
                placeholderText: qsTr("Titolo del grafico")
                maximumLength: 80
            }

            RowLayout {
                Layout.fillWidth: true
                visible: editor.source.cols > 0
                Label { text: qsTr("Tipo") }
                ComboBox {
                    id: kind
                    Layout.fillWidth: true
                    Accessible.name: qsTr("Tipo di grafico")
                    model: [qsTr("Linee"), qsTr("Barre")]
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: editor.source.cols > 0
                Label { text: qsTr("Colonna X") }
                ComboBox {
                    id: axisX
                    objectName: "chartSourceAxisX"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Accessible.name: qsTr("Colonna X")
                    model: editor.source.cols || 0
                    delegate: ItemDelegate { required property int index; text: editor.columnLabel(index); width: axisX.width }
                    displayText: editor.columnLabel(currentIndex)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: editor.source.cols > 0
                Label { text: qsTr("Colonna Y") }
                ComboBox {
                    id: axisY
                    objectName: "chartSourceAxisY"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Accessible.name: qsTr("Colonna Y")
                    model: axisX.model
                    delegate: ItemDelegate { required property int index; text: editor.columnLabel(index); width: axisY.width }
                    displayText: editor.columnLabel(currentIndex)
                }
            }

            CheckBox {
                id: headerToggle
                visible: editor.source.cols > 0
                text: qsTr("Prima riga come intestazione")
            }

            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                wrapMode: Text.WordWrap
                text: editor.errorText || (!editor.samePage ? qsTr("La pagina o la tabella è cambiata. Controlla l’origine dei dati.")
                    : !editor.source.cols ? (editor.choosingSource && editor.sourceEntries.length > 0 ? qsTr("Scegli una tabella di origine.")
                        : qsTr("Nessuna tabella disponibile. Crea o importa prima una tabella."))
                    : editor.preview.points ? "" : qsTr("Scegli due colonne con valori numerici validi."))
            }

            TouchKeyboard {
                Layout.fillWidth: true
                visible: editor.showTouchKeyboard && titleField.activeFocus
                uiScale: editor.uiScale
                target: titleField
            }
        }
    }

    footer: RowLayout {
        spacing: 4 * editor.uiScale

        PaperButton {
            uiScale: editor.uiScale
            visible: !!editor.original.id
            text: editor.confirmingDelete ? qsTr("Conferma") : qsTr("Elimina")
            onClicked: {
                if (editor.confirmingDelete)
                    editor.deleteRequested(editor.original.id)
                else
                    editor.confirmingDelete = true
            }
        }
        Item { Layout.fillWidth: true }
        PaperButton { uiScale: editor.uiScale; text: qsTr("Annulla"); onClicked: editor.close() }
        PaperButton {
            objectName: "saveTableChart"
            uiScale: editor.uiScale
            text: qsTr("Salva")
            filled: true
            enabled: !!editor.preview.points && !editor.confirmingDelete
            onClicked: editor.save()
        }
    }
}
