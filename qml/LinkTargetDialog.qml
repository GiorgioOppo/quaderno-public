import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: picker
    objectName: "linkTargetDialog"
    required property Notebook notebook
    required property real uiScale
    required property real logicalWidth
    required property bool tabletMode
    property point anchorPoint
    property string sourceBookId: ""
    property string sourcePageId: ""
    property string kind: "diagram"
    property string existingId: ""
    property string errorText: ""
    property string pendingOpenId: ""

    readonly property var categories: [
        { kind: "diagram", label: qsTr("Diagrammi") },
        { kind: "table", label: qsTr("Tabelle") },
        { kind: "chart", label: qsTr("Grafici") }
    ]
    readonly property var targets: visible ? notebook.libraryItems.filter(entry => !entry.empty) : []
    readonly property var entries: targets.filter(entry => entry.type === kind)
    readonly property bool validContext: sourceBookId === notebook.bookId && sourcePageId === notebook.pageId
    readonly property var existing: existingId.length > 0 && validContext
        ? notebook.pageItems.find(item => item.id === existingId && item.type === "link") : null
    readonly property var existingTarget: existing ? targets.find(entry =>
        entry.bookId === existing.targetBookId && entry.pageId === existing.targetPageId
        && entry.itemId === existing.targetItemId && entry.type === existing.targetType) : null

    signal openRequested(string linkId)

    title: existingId.length ? qsTr("Collegamento") : qsTr("Collega a un elemento")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(540 * uiScale, logicalWidth - 24 * uiScale)
    padding: 14 * uiScale
    modal: true
    dim: !tabletMode
    focus: true
    standardButtons: Dialog.Cancel
    onClosed: {
        const id = pendingOpenId
        const book = sourceBookId
        const page = sourcePageId
        pendingOpenId = ""
        if (!id.length) return
        Qt.callLater(function() {
            if (book === notebook.bookId && page === notebook.pageId)
                openRequested(id)
        })
    }

    function rememberSource() {
        sourceBookId = notebook.bookId
        sourcePageId = notebook.pageId
        errorText = ""
        pendingOpenId = ""
    }

    function openAt(point: point) {
        rememberSource()
        existingId = ""
        anchorPoint = point
        const available = notebook.libraryItems.find(entry => !entry.empty)
        kind = available ? available.type : "diagram"
        open()
    }

    function openExisting(item: var) {
        rememberSource()
        existingId = item.id
        open()
    }

    function save(entry: var) {
        if (notebook.savePageLink(entry, anchorPoint, sourceBookId, sourcePageId))
            close()
        else
            errorText = notebook.error
    }

    function targetLabel(entry: var): string {
        return entry.title && entry.title !== entry.bookTitle
            ? qsTr("%1 · %2, pagina %3").arg(entry.title).arg(entry.bookTitle).arg(entry.pageNumber)
            : qsTr("%1, pagina %2").arg(entry.bookTitle).arg(entry.pageNumber)
    }

    contentItem: ColumnLayout {
        spacing: 9 * picker.uiScale

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 14 * picker.uiScale
            textFormat: Text.PlainText
            text: picker.existingId.length
                ? (picker.existingTarget ? picker.targetLabel(picker.existingTarget) : qsTr("Destinazione non disponibile."))
                : qsTr("Scegli un elemento della libreria. Tocca il collegamento sulla pagina per aprire l’originale.")
        }

        RowLayout {
            visible: !picker.existingId.length
            Layout.fillWidth: true
            spacing: 4 * picker.uiScale

            Repeater {
                model: picker.categories
                PaperButton {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    uiScale: picker.uiScale
                    text: modelData.label
                    filled: picker.kind === modelData.kind
                    onClicked: picker.kind = modelData.kind
                }
            }
        }

        ListView {
            id: targetList
            visible: !picker.existingId.length && count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(count * 46 * picker.uiScale,
                picker.parent ? picker.parent.height * .38 : 230 * picker.uiScale)
            model: picker.entries
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: PaperButton {
                required property var modelData
                width: targetList.width
                height: 46 * picker.uiScale
                uiScale: picker.uiScale
                iconName: modelData.type
                text: picker.targetLabel(modelData)
                alignLeft: true
                quiet: true
                enabled: picker.validContext && picker.notebook.canWriteLayer
                onClicked: picker.save(modelData)
            }
        }

        Label {
            visible: !picker.existingId.length && targetList.count === 0
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 14 * picker.uiScale
            text: qsTr("Nessun elemento disponibile in questa categoria.")
        }

        RowLayout {
            visible: picker.existingId.length > 0
            Layout.fillWidth: true
            PaperButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                uiScale: picker.uiScale
                iconName: "link"
                text: qsTr("Apri destinazione")
                filled: true
                enabled: !!picker.existingTarget
                onClicked: {
                    picker.pendingOpenId = picker.existingId
                    picker.close()
                }
            }
            PaperButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                uiScale: picker.uiScale
                text: qsTr("Rimuovi collegamento")
                enabled: !!picker.existing && picker.notebook.canWriteLayer
                onClicked: {
                    if (picker.notebook.removePageLink(picker.existingId, picker.sourceBookId, picker.sourcePageId))
                        picker.close()
                    else
                        picker.errorText = picker.notebook.error
                }
            }
        }

        Label {
            visible: picker.errorText.length > 0 || !picker.validContext
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 14 * picker.uiScale
            text: picker.validContext ? picker.errorText : qsTr("La pagina del collegamento è cambiata.")
        }
    }
}
