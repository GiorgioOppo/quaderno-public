import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: tools
    objectName: "workspaceTools"
    required property real uiScale
    property bool tabletMode: false
    property string workspace: "writing"
    property string bookId: ""
    property string pageId: ""
    property var pageItems: []
    property bool active: true
    property bool actionsEnabled: true
    property bool canWrite: true
    property bool markdownBook: false
    property bool hasStrokes: false
    property bool diagramReviewAllowed: false
    property bool autoDiagramRecognition: false
    property string penTool: "pen"
    property string syncStatus: ""
    readonly property bool dialogOpen: picker.visible || moreMenu.visible
    readonly property string heading: ({books: qsTr("Libri"), writing: qsTr("Scrittura"),
        charts: qsTr("Grafici"), tables: qsTr("Tabelle"), diagrams: qsTr("Diagrammi")})[workspace] || qsTr("Scrittura")
    readonly property string moreLabel: qsTr("Altro")
    readonly property real actionSpacing: 5 * uiScale
    readonly property real rowWidth: Math.max(0, width - 16 * uiScale)
    readonly property int primaryCount: widthForActions(3) <= rowWidth ? 3 : 2
    readonly property bool compactActions: widthForActions(primaryCount) > rowWidth
    readonly property var actions: workspace === "books" ? [
        {key: "highlighter", text: qsTr("Evidenzia"), icon: "pen"},
        {key: "bookNote", text: qsTr("Post-it"), icon: "text"},
        {key: "underline", text: qsTr("Sottolinea"), icon: "underline"},
        {key: "annotatedPages", text: qsTr("Pagine annotate"), icon: "notebook"},
        {key: "bookSync", text: qsTr("Sincronizza libro"), icon: "sync"}
    ] : workspace === "charts" ? [
        {key: "chartEdit", text: qsTr("Modifica dati"), icon: "chart", kind: "chart"},
        {key: "newChart", text: qsTr("Nuovo grafico"), icon: "plus"},
        {key: "chartFromTable", text: qsTr("Da tabella"), icon: "table", kind: "table"},
        {key: "chartCsv", text: qsTr("Esporta CSV"), icon: "download", kind: "chart"}
    ] : workspace === "tables" ? [
        {key: "tableEdit", text: qsTr("Celle e formule"), icon: "table", kind: "table"},
        {key: "newTable", text: qsTr("Nuova tabella"), icon: "plus"},
        {key: "importCsv", text: qsTr("Importa CSV"), icon: "download"},
        {key: "chartFromTable", text: qsTr("Crea grafico"), icon: "chart", kind: "table"},
        {key: "tableCsv", text: qsTr("Esporta CSV"), icon: "download", kind: "table"}
    ] : workspace === "diagrams" ? [
        {key: "diagramRecognize", text: qsTr("Riconosci diagramma"), icon: "diagram"},
        {key: "diagramNew", text: qsTr("Inserisci forme"), icon: "plus"},
        {key: "diagramEdit", text: qsTr("Modifica diagramma"), icon: "rename", kind: "diagram"},
        {key: "diagramImport", text: qsTr("Importa draw.io"), icon: "download"},
        {key: "diagramAutomatic", text: qsTr("Riconoscimento automatico")}
    ] : [
        {key: "markdown", text: markdownBook ? qsTr("Modifica Markdown") : qsTr("Nuovo Markdown"), icon: "text"},
        {key: "text", text: qsTr("Inserisci testo"), icon: "text"},
        {key: "image", text: qsTr("Inserisci immagine"), icon: "image"},
        {key: "library", text: qsTr("Dalla libreria"), icon: "layers"}
    ]
    signal actionRequested(string action, string itemId)
    signal automaticRecognitionChanged(bool enabled)

    implicitHeight: content.implicitHeight + 16 * uiScale
    radius: 12 * uiScale
    color: "white"

    FontMetrics {
        id: actionFont
        font.pixelSize: 14 * tools.uiScale
        font.weight: Font.DemiBold
    }

    function widthForActions(count: int): real {
        // A PaperButton with text uses an 18 px icon, an 8 px gap and 28 px padding.
        let width = actionFont.advanceWidth(moreLabel) + 54 * uiScale + count * actionSpacing
        for (const action of actions.slice(0, count))
            width += actionFont.advanceWidth(action.text) + 54 * uiScale
        return width
    }

    function itemsOfType(kind: string): var {
        return pageItems.filter(item => item.type === kind)
    }

    function enabledFor(action: var, inMenu: bool): bool {
        if (!active || (!inMenu && !actionsEnabled)) return false
        if (action.key === "diagramRecognize") return diagramReviewAllowed && hasStrokes
        if (["highlighter", "underline", "bookNote"].includes(action.key)) return canWrite
        return !action.kind || itemsOfType(action.kind).length > 0
    }

    function itemTitle(item: var, index: int): string {
        if (item.title && item.title.trim().length) return item.title
        if (item.type === "table") return qsTr("Tabella %1 · %2 × %3").arg(index + 1).arg(item.rows).arg(item.cols)
        if (item.type === "diagram") {
            const labeled = (item.nodes || []).find(node => node.text && node.text.trim().length)
            if (labeled) return labeled.text
            return qsTr("Diagramma %1").arg(index + 1)
        }
        return qsTr("Grafico %1").arg(index + 1)
    }

    function runAction(action: var) {
        if (!enabledFor(action, false)) return
        if (action.key === "diagramAutomatic") {
            automaticRecognitionChanged(!autoDiagramRecognition)
            return
        }
        if (!action.kind) {
            actionRequested(action.key, "")
            return
        }
        const matches = itemsOfType(action.kind)
        if (matches.length === 1) {
            actionRequested(action.key, matches[0].id)
            return
        }
        picker.action = action.key
        picker.kind = action.kind
        picker.sourceBook = bookId
        picker.sourcePage = pageId
        picker.choices = matches.map((item, index) => ({id: item.id, title: itemTitle(item, index)}))
        picker.open()
    }

    function closeDialogs() {
        moreMenu.pendingAction = null
        moreMenu.close()
        picker.pendingItem = ""
        picker.close()
    }
    onBookIdChanged: closeDialogs()
    onPageIdChanged: closeDialogs()
    onWorkspaceChanged: closeDialogs()
    onActiveChanged: if (!active) closeDialogs()
    onVisibleChanged: if (!visible) closeDialogs()
    onPrimaryCountChanged: if (moreMenu.visible) closeDialogs()

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 8 * tools.uiScale
        spacing: 4 * tools.uiScale
        RowLayout {
            Layout.fillWidth: true
            spacing: 10 * tools.uiScale
            Label {
                objectName: "workspaceHeading"
                text: tools.heading
                font.pixelSize: 15 * tools.uiScale
                font.weight: Font.DemiBold
                color: "#1c1c1e"
            }
            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: tools.workspace === "books" ? tools.syncStatus : ""
                font.pixelSize: 12 * tools.uiScale
                color: "#515159"
                elide: Text.ElideRight
                textFormat: Text.PlainText
                visible: text.length > 0
            }
        }
        RowLayout {
            objectName: "workspaceActionsRow"
            Layout.fillWidth: true
            spacing: tools.actionSpacing
            Repeater {
                model: tools.actions
                PaperButton {
                    required property var modelData
                    required property int index
                    readonly property bool primary: index < tools.primaryCount
                    objectName: !primary ? "" : modelData.key === "diagramRecognize" ? "recognizeDiagramButton" : "workspaceAction_" + modelData.key
                    visible: primary
                    uiScale: tools.uiScale
                    text: modelData.text
                    iconName: modelData.icon || ""
                    iconOnly: tools.compactActions
                    filled: (modelData.key === "highlighter" || modelData.key === "underline") && tools.penTool === modelData.key
                    enabled: tools.enabledFor(modelData, false)
                    Accessible.name: text
                    ToolTip.visible: hovered && iconOnly && !tools.tabletMode
                    ToolTip.text: text
                    onClicked: tools.runAction(modelData)
                }
            }
            Item { Layout.fillWidth: true }
            PaperButton {
                id: moreButton
                objectName: "workspaceMoreButton"
                uiScale: tools.uiScale
                text: tools.moreLabel
                iconName: "more"
                quiet: true
                enabled: tools.active && tools.actionsEnabled
                onClicked: moreMenu.begin()
            }
        }
    }

    Popup {
        id: moreMenu
        objectName: "workspaceMoreMenu"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.max(0, Math.min(290 * tools.uiScale, parent ? parent.width - 16 * tools.uiScale : 290 * tools.uiScale))
        padding: 8 * tools.uiScale
        modal: true
        dim: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property var pendingAction: null
        property string sourceBook: ""
        property string sourcePage: ""
        property string sourceWorkspace: ""

        function begin() {
            if (!parent || !tools.active || !tools.actionsEnabled) return
            pendingAction = null
            sourceBook = tools.bookId
            sourcePage = tools.pageId
            sourceWorkspace = tools.workspace
            open()
        }

        function moveFocus(step: int) {
            const count = menuOptions.count
            let focused = -1
            for (let i = 0; i < count; ++i) {
                if (menuOptions.itemAt(i)?.activeFocus) {
                    focused = i
                    break
                }
            }
            if (focused < 0) focused = step > 0 ? count - 1 : 0
            for (let offset = 1; offset <= count; ++offset) {
                const button = menuOptions.itemAt((focused + offset * step + count) % count)
                if (button?.enabled) {
                    button.forceActiveFocus(Qt.TabFocusReason)
                    return
                }
            }
        }

        onOpened: moveFocus(1)

        onClosed: {
            const action = pendingAction
            const book = sourceBook
            const page = sourcePage
            const workspace = sourceWorkspace
            pendingAction = null
            if (!action) return
            Qt.callLater(function() {
                if (tools.visible && tools.active && tools.bookId === book && tools.pageId === page
                    && tools.workspace === workspace) tools.runAction(action)
            })
        }

        background: Rectangle {
            color: "white"
            border.color: "#c7c7cc"
            border.width: Math.max(1, tools.uiScale)
            radius: 12 * tools.uiScale
        }

        contentItem: Column {
            spacing: 4 * tools.uiScale
            Keys.onUpPressed: moreMenu.moveFocus(-1)
            Keys.onDownPressed: moreMenu.moveFocus(1)
            Repeater {
                id: menuOptions
                model: tools.actions.slice(tools.primaryCount)
                PaperButton {
                    required property var modelData
                    objectName: modelData.key === "diagramAutomatic" ? "diagramAutoRecognition" : "workspaceAction_" + modelData.key
                    width: moreMenu.availableWidth
                    uiScale: tools.uiScale
                    text: modelData.text
                    iconName: modelData.icon || ""
                    alignLeft: true
                    quiet: true
                    filled: checkable && tools.autoDiagramRecognition
                    enabled: tools.enabledFor(modelData, true)
                    checkable: modelData.key === "diagramAutomatic"
                    checked: checkable && tools.autoDiagramRecognition
                    onClicked: {
                        moreMenu.pendingAction = modelData
                        moreMenu.close()
                    }
                }
            }
        }
    }

    Dialog {
        id: picker
        objectName: "workspaceItemPicker"
        property string action: ""
        property string kind: ""
        property string sourceBook: ""
        property string sourcePage: ""
        property string pendingItem: ""
        property var choices: []
        title: kind === "table" ? qsTr("Scegli una tabella")
            : kind === "chart" ? qsTr("Scegli un grafico") : qsTr("Scegli un diagramma")
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(470 * tools.uiScale, (parent ? parent.width : 470 * tools.uiScale) - 24 * tools.uiScale)
        modal: true
        dim: !tools.tabletMode
        padding: 16 * tools.uiScale
        closePolicy: Popup.CloseOnEscape

        function choose(id: string) {
            if (!tools.active || sourceBook !== tools.bookId || sourcePage !== tools.pageId
                || !tools.pageItems.some(item => item.id === id && item.type === kind)) {
                close()
                return
            }
            pendingItem = id
            close()
        }

        onAboutToShow: pendingItem = ""
        onClosed: {
            const selectedAction = action
            const selectedItem = pendingItem
            const contextCurrent = tools.active && sourceBook === tools.bookId && sourcePage === tools.pageId
                && tools.pageItems.some(item => item.id === selectedItem && item.type === kind)
            action = ""
            kind = ""
            sourceBook = ""
            sourcePage = ""
            choices = []
            pendingItem = ""
            if (selectedItem.length && contextCurrent) tools.actionRequested(selectedAction, selectedItem)
        }
        contentItem: ColumnLayout {
            spacing: 8 * tools.uiScale
            Label {
                Layout.fillWidth: true
                text: qsTr("La pagina contiene più elementi. Scegli quello da usare.")
                wrapMode: Text.WordWrap
                font.pixelSize: 14 * tools.uiScale
                color: "#515159"
            }
            ListView {
                id: options
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(count, 5) * 48 * tools.uiScale
                model: picker.choices
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: PaperButton {
                    required property var modelData
                    objectName: "workspaceItemChoice_" + modelData.id
                    width: options.width
                    uiScale: tools.uiScale
                    text: modelData.title
                    alignLeft: true
                    quiet: true
                    onClicked: picker.choose(modelData.id)
                }
            }
            PaperButton {
                Layout.alignment: Qt.AlignRight
                uiScale: tools.uiScale
                text: qsTr("Annulla")
                onClicked: picker.close()
            }
        }
    }
}
