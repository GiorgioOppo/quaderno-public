import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: editor

    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    required property Notebook notebook
    property string errorText: ""

    signal saveRequested(var item)
    signal saveOnNewPageRequested(var item)
    signal replaceTranscriptRequested(var selection, string text)
    signal deleteRequested(string id)
    signal tableChartRequested(var item)
    signal csvImportRequested()
    signal csvExportRequested(var item)

    property string itemType: "text"
    property var originalItem: null
    property int rowCount: 3
    property int columnCount: 3
    property int selectedRow: 0
    property int selectedColumn: 0
    property var cellDraft: []
    property string csvErrorText: ""
    property bool confirmDelete: false
    property var transcriptSelection: null

    readonly property bool transcript: transcriptSelection !== null
    readonly property bool editingTable: itemType === "table"
    readonly property bool existing: originalItem !== null && Boolean(originalItem.id)
    readonly property bool canSave: !confirmDelete && (editingTable || textInput.text.trim().length > 0)
    readonly property var cellValues: visible && editingTable ? notebook.tableCellValues(tableDraft()) : []
    readonly property string selectedValue: cellValues[selectedRow * columnCount + selectedColumn] || ""
    readonly property bool canInsertFormula: selectedRow > 0 && cellInput.text.length === 0
    readonly property real hairline: Math.max(1, Math.round(uiScale))
    readonly property color ink: "#242723"
    readonly property color muted: "#535862"
    readonly property color blue: "#155cc2"
    readonly property color border: "#d7dce4"
    readonly property color surface: "#f6f7f9"
    readonly property color selected: "#e8f1fd"

    title: transcript ? qsTr("Rivedi la trascrizione")
          : editingTable ? (existing ? qsTr("Modifica tabella") : qsTr("Inserisci tabella"))
          : (existing ? qsTr("Modifica testo") : qsTr("Inserisci testo"))
    anchors.centerIn: Overlay.overlay
    width: Math.min(550 * uiScale, (parent ? parent.width : 550 * uiScale) - 24 * uiScale)
    padding: (width < 450 * uiScale ? 14 : 20) * uiScale
    spacing: 12 * uiScale
    modal: true
    dim: !tabletMode
    focus: true
    closePolicy: Popup.CloseOnEscape

    onOpened: {
        if (editingTable)
            cellInput.forceActiveFocus()
        else
            textInput.forceActiveFocus()
    }

    onClosed: {
        originalItem = null
        confirmDelete = false
        transcriptSelection = null
        cellDraft = []
        textInput.clear()
        cellInput.clear()
    }

    function openFor(type: string, item: var, initialText: string, selection: var) {
        if (type !== "text" && type !== "table")
            return

        itemType = type
        originalItem = item && item.id ? item : null
        confirmDelete = false
        transcriptSelection = selection || null
        errorText = ""
        csvErrorText = ""
        selectedRow = 0
        selectedColumn = 0
        const draft = []

        if (editingTable) {
            rowCount = originalItem ? originalItem.rows : 3
            columnCount = originalItem ? originalItem.cols : 3
            if (originalItem) {
                for (let row = 0; row < rowCount; ++row) {
                    for (let column = 0; column < columnCount; ++column)
                        draft[cellIndex(row, column)] = originalItem.cells[row * columnCount + column]
                }
            }
        } else {
            textInput.text = originalItem ? originalItem.text : initialText
        }

        cellDraft = draft
        cellInput.text = draft[0] || ""
        grid.contentX = 0
        grid.contentY = 0
        scroll.contentItem.contentY = 0
        open()
    }

    function cellIndex(row: int, column: int): int {
        return row * 8 + column
    }

    function importCsv(result: var) {
        if (!editingTable || existing)
            return
        csvErrorText = result.error || ""
        if (csvErrorText.length > 0)
            return

        rowCount = result.rows
        columnCount = result.cols
        const draft = []
        for (let row = 0; row < rowCount; ++row) {
            for (let column = 0; column < columnCount; ++column)
                draft[cellIndex(row, column)] = result.cells[row * columnCount + column]
        }
        cellDraft = draft
        selectedRow = 0
        selectedColumn = 0
        cellInput.text = draft[0]
        grid.contentX = 0
        grid.contentY = 0
        cellInput.forceActiveFocus()
    }

    function storeCell() {
        const index = cellIndex(selectedRow, selectedColumn)
        if ((cellDraft[index] || "") !== cellInput.text) {
            const draft = cellDraft.slice()
            draft[index] = cellInput.text
            cellDraft = draft
        }
    }

    function showSelectedCell() {
        if (grid.width <= 0 || grid.height <= 0)
            return
        const x = selectedColumn * grid.cellWidth
        const y = selectedRow * grid.cellHeight
        grid.contentX = Math.max(0, Math.min(x, Math.max(grid.contentX, x + grid.cellWidth - grid.width)))
        grid.contentY = Math.max(0, Math.min(y, Math.max(grid.contentY, y + grid.cellHeight - grid.height)))
    }

    function selectCell(row: int, column: int, selectText: bool) {
        storeCell()
        selectedRow = row
        selectedColumn = column
        cellInput.text = cellDraft[cellIndex(row, column)] || ""
        showSelectedCell()
        cellInput.forceActiveFocus()
        if (selectText)
            cellInput.selectAll()
    }

    function setDimensions(rows: int, cols: int) {
        storeCell()
        rowCount = rows
        columnCount = cols
        selectedRow = Math.min(selectedRow, rowCount - 1)
        selectedColumn = Math.min(selectedColumn, columnCount - 1)
        cellInput.text = cellDraft[cellIndex(selectedRow, selectedColumn)] || ""
        grid.returnToBounds()
        showSelectedCell()
    }

    function moveCell(delta: int): bool {
        const index = selectedRow * columnCount + selectedColumn + delta
        if (index < 0 || index >= rowCount * columnCount)
            return false

        selectCell(Math.floor(index / columnCount), index % columnCount, true)
        return true
    }

    function submit(replaceInk: bool) {
        if (!canSave)
            return

        if (transcript && replaceInk) {
            replaceTranscriptRequested(transcriptSelection, textInput.text)
            return
        }

        const payload = {type: itemType}
        if (existing) {
            payload.id = originalItem.id
            payload.x = originalItem.x
            payload.y = originalItem.y
            payload.width = originalItem.width
            payload.height = originalItem.height
        }

        if (editingTable) {
            Object.assign(payload, tableDraft())
        } else {
            payload.text = textInput.text
        }
        if (transcript)
            saveOnNewPageRequested(payload)
        else
            saveRequested(payload)
    }

    function tableDraft(): var {
        const cells = []
        for (let row = 0; row < rowCount; ++row) {
            for (let column = 0; column < columnCount; ++column) {
                cells.push(row === selectedRow && column === selectedColumn
                    ? cellInput.text : cellDraft[cellIndex(row, column)] || "")
            }
        }
        return { type: "table", rows: rowCount, cols: columnCount, cells }
    }

    function columnLetter(column: int): string {
        return String.fromCharCode(65 + column)
    }

    function insertFormula(name: string) {
        if (!canInsertFormula)
            return
        const column = columnLetter(selectedColumn)
        cellInput.text = "=" + name + "(" + column + "1:" + column + selectedRow + ")"
        cellInput.forceActiveFocus()
        cellInput.cursorPosition = cellInput.length
    }

    function handleCellKey(event: KeyEvent) {
        const shift = (event.modifiers & Qt.ShiftModifier) !== 0
        if (event.key === Qt.Key_Backtab || (event.key === Qt.Key_Tab && shift))
            event.accepted = moveCell(-1)
        else if (event.key === Qt.Key_Tab)
            event.accepted = moveCell(1)
        else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !shift)
            event.accepted = moveCell(columnCount)
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

    component Area: TextArea {
        id: area

        required property int textLimit

        color: editor.ink
        placeholderTextColor: editor.muted
        selectionColor: editor.blue
        selectedTextColor: "white"
        font.pixelSize: 16 * editor.uiScale
        padding: 10 * editor.uiScale
        textFormat: TextEdit.PlainText
        wrapMode: TextEdit.Wrap
        selectByMouse: true
        onTextChanged: if (length > textLimit) remove(textLimit, length)

        background: Rectangle {
            color: editor.surface
            radius: 7 * editor.uiScale
            border.color: area.activeFocus ? editor.blue : editor.border
            border.width: area.activeFocus ? Math.max(2, editor.uiScale) : editor.hairline
        }
    }

    component Dimension: ColumnLayout {
        id: dimension

        required property string label
        required property string decreaseLabel
        required property string increaseLabel
        required property int value
        required property int maximum
        signal valueRequested(int value)

        spacing: 4 * editor.uiScale

        Caption {
            text: dimension.label
            font.weight: Font.DemiBold
        }

        RowLayout {
            spacing: 2 * editor.uiScale

            PaperButton {
                uiScale: editor.uiScale
                text: "−"
                Accessible.name: dimension.decreaseLabel
                enabled: dimension.value > 1
                onClicked: dimension.valueRequested(dimension.value - 1)
            }

            Caption {
                text: dimension.value.toString()
                Layout.minimumWidth: 22 * editor.uiScale
                horizontalAlignment: Text.AlignHCenter
                color: editor.ink
            }

            PaperButton {
                uiScale: editor.uiScale
                text: "+"
                Accessible.name: dimension.increaseLabel
                enabled: dimension.value < dimension.maximum
                onClicked: dimension.valueRequested(dimension.value + 1)
            }
        }
    }

    contentItem: ScrollView {
        id: scroll

        clip: true
        contentWidth: availableWidth
        contentHeight: form.implicitHeight
        implicitHeight: Math.min(form.implicitHeight, Math.max(120 * editor.uiScale,
            (editor.parent ? editor.parent.height : 500) - (editor.transcript ? 222 : 170) * editor.uiScale))
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: form

            width: scroll.availableWidth
            spacing: 10 * editor.uiScale

            ScrollView {
                visible: !editor.editingTable
                Layout.fillWidth: true
                Layout.preferredHeight: 155 * editor.uiScale
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                Area {
                    id: textInput

                    textLimit: 4000
                    placeholderText: qsTr("Scrivi il testo")
                    Accessible.name: placeholderText
                }
            }

            Flow {
                visible: editor.editingTable && !editor.confirmDelete
                Layout.fillWidth: true
                spacing: 5 * editor.uiScale

                PaperButton {
                    visible: !editor.existing
                    uiScale: editor.uiScale
                    text: qsTr("Importa CSV")
                    iconName: "download"
                    onClicked: editor.csvImportRequested()
                }

                PaperButton {
                    uiScale: editor.uiScale
                    text: qsTr("Esporta CSV")
                    iconName: "upload"
                    onClicked: editor.csvExportRequested(editor.tableDraft())
                }
            }

            Caption {
                visible: editor.editingTable && !editor.existing && editor.csvErrorText.length > 0
                Layout.fillWidth: true
                text: editor.csvErrorText
                color: "#87332c"
            }

            RowLayout {
                visible: editor.editingTable
                Layout.fillWidth: true
                spacing: 12 * editor.uiScale

                Dimension {
                    Layout.fillWidth: true
                    label: qsTr("Righe")
                    decreaseLabel: qsTr("Togli una riga")
                    increaseLabel: qsTr("Aggiungi una riga")
                    value: editor.rowCount
                    maximum: 12
                    onValueRequested: value => editor.setDimensions(value, editor.columnCount)
                }

                Dimension {
                    Layout.fillWidth: true
                    label: qsTr("Colonne")
                    decreaseLabel: qsTr("Togli una colonna")
                    increaseLabel: qsTr("Aggiungi una colonna")
                    value: editor.columnCount
                    maximum: 8
                    onValueRequested: value => editor.setDimensions(editor.rowCount, value)
                }
            }

            Flickable {
                id: grid

                readonly property real cellWidth: Math.max(88 * editor.uiScale, width / editor.columnCount)
                readonly property real cellHeight: 44 * editor.uiScale

                visible: editor.editingTable
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(editor.rowCount, 4) * cellHeight
                contentWidth: editor.columnCount * cellWidth
                contentHeight: editor.rowCount * cellHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: contentWidth > width || contentHeight > height
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                Grid {
                    columns: editor.columnCount

                    Repeater {
                        model: editor.editingTable ? editor.rowCount * editor.columnCount : 0

                        ItemDelegate {
                            id: cell

                            required property int index
                            readonly property int row: Math.floor(index / editor.columnCount)
                            readonly property int column: index % editor.columnCount
                            readonly property bool selected: row === editor.selectedRow && column === editor.selectedColumn

                            width: grid.cellWidth
                            height: grid.cellHeight
                            padding: 6 * editor.uiScale
                            focusPolicy: Qt.ClickFocus
                            text: editor.cellValues[index] || ""
                            Accessible.name: qsTr("Cella %1%2").arg(editor.columnLetter(column)).arg(row + 1)
                            onClicked: editor.selectCell(row, column, false)

                            background: Rectangle {
                                color: cell.selected ? editor.selected : cell.down ? editor.surface : "white"
                                border.color: cell.selected ? editor.blue : editor.border
                                border.width: editor.hairline
                            }

                            contentItem: Text {
                                text: cell.text.replace(/\n/g, " ")
                                textFormat: Text.PlainText
                                color: cell.selected ? editor.blue : editor.ink
                                font.pixelSize: 14 * editor.uiScale
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }

            Caption {
                id: cellCaption

                visible: editor.editingTable
                text: qsTr("Cella %1%2").arg(editor.columnLetter(editor.selectedColumn)).arg(editor.selectedRow + 1)
                font.weight: Font.DemiBold
            }

            ScrollView {
                visible: editor.editingTable
                Layout.fillWidth: true
                Layout.preferredHeight: 64 * editor.uiScale
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                Area {
                    id: cellInput

                    textLimit: 500
                    placeholderText: qsTr("Testo della cella")
                    Accessible.name: cellCaption.text

                    Keys.onPressed: event => editor.handleCellKey(event)
                }
            }

            Flow {
                visible: editor.editingTable && !editor.confirmDelete
                Layout.fillWidth: true
                spacing: 5 * editor.uiScale

                PaperButton {
                    uiScale: editor.uiScale
                    text: qsTr("Somma")
                    enabled: editor.canInsertFormula
                    onClicked: editor.insertFormula(qsTr("SOMMA", "Table formula name"))
                }

                PaperButton {
                    uiScale: editor.uiScale
                    text: qsTr("Media")
                    enabled: editor.canInsertFormula
                    onClicked: editor.insertFormula(qsTr("MEDIA", "Table formula name"))
                }
            }

            Caption {
                visible: editor.editingTable && editor.selectedValue !== cellInput.text
                Layout.fillWidth: true
                text: qsTr("Risultato %1").arg(editor.selectedValue)
            }

            PaperButton {
                visible: editor.showTouchKeyboard
                Layout.alignment: Qt.AlignRight
                uiScale: editor.uiScale
                text: qsTr("A capo")
                focusPolicy: Qt.NoFocus
                quiet: true
                onClicked: keyboard.typeKey("\n")
            }

            TouchKeyboard {
                id: keyboard

                visible: editor.showTouchKeyboard
                Layout.fillWidth: true
                uiScale: editor.uiScale
                target: editor.editingTable ? cellInput : textInput
            }

            Caption {
                visible: editor.errorText.length > 0
                Layout.fillWidth: true
                text: editor.errorText
                color: "#87332c"
            }
        }
    }

    footer: ColumnLayout {
        spacing: 6 * editor.uiScale

        PaperButton {
            visible: editor.transcript
            Layout.alignment: Qt.AlignRight
            Layout.rightMargin: editor.rightPadding
            uiScale: editor.uiScale
            text: qsTr("Salva in una nuova pagina")
            enabled: editor.canSave
            onClicked: editor.submit(false)
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6 * editor.uiScale

            PaperButton {
                visible: editor.existing
                Layout.leftMargin: editor.leftPadding
                Layout.bottomMargin: editor.bottomPadding
                uiScale: editor.uiScale
                text: editor.confirmDelete ? qsTr("Conferma") : qsTr("Elimina")
                Accessible.name: editor.confirmDelete ? qsTr("Conferma eliminazione") : text
                onClicked: {
                    if (editor.confirmDelete)
                        editor.deleteRequested(editor.originalItem.id)
                    else
                        editor.confirmDelete = true
                }
            }

            Item { Layout.fillWidth: true }

            PaperButton {
                uiScale: editor.uiScale
                visible: editor.existing && editor.editingTable && !editor.confirmDelete && editor.columnCount >= 2
                text: qsTr("Grafico")
                iconName: "chart"
                iconOnly: editor.width < 410 * editor.uiScale
                Layout.bottomMargin: editor.bottomPadding
                onClicked: {
                    const payload = editor.tableDraft()
                    payload.id = editor.originalItem.id
                    editor.tableChartRequested(payload)
                }
            }

            PaperButton {
                Layout.bottomMargin: editor.bottomPadding
                uiScale: editor.uiScale
                text: qsTr("Annulla")
                onClicked: {
                    if (editor.confirmDelete)
                        editor.confirmDelete = false
                    else
                        editor.close()
                }
            }

            PaperButton {
                Layout.rightMargin: editor.rightPadding
                Layout.bottomMargin: editor.bottomPadding
                uiScale: editor.uiScale
                text: editor.transcript ? qsTr("Sostituisci sulla pagina") : qsTr("Salva")
                enabled: editor.canSave
                filled: true
                onClicked: editor.submit(true)
            }
        }
    }
}
