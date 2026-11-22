import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Rectangle {
    id: tools
    objectName: "workspaceTools"
    required property real uiScale
    property bool tabletMode: false
    property string workspace: "writing"
    property string bookId: ""
    property string pageId: ""
    property var pageItems: []
    property Notebook notebook: null
    property bool internalSheetsAvailable: false
    property string linkedItemId: ""
    property bool active: true
    property bool actionsEnabled: true
    property bool canWrite: true
    property bool markdownBook: false
    property bool hasStrokes: false
    property bool diagramReviewAllowed: false
    property bool autoDiagramRecognition: false
    property string penTool: "pen"
    property string placing: ""
    property bool lassoActive: false
    property bool editingItems: false
    property bool canUndo: false
    property bool canRedo: false
    property bool hasReaderControls: false
    property bool allowMixedPageImport: false
    property string syncStatus: ""
    readonly property bool dialogOpen: picker.visible || moreMenu.visible || linkedItems.visible
    readonly property Item remoteSurface: {
        const popup = picker.visible ? picker : moreMenu.visible ? moreMenu : linkedItems.visible ? linkedItems : null
        if (!popup || !popup.opened || (popup === picker && !remoteActions.includes(picker.action)))
            return null
        return popup.contentItem ? popup.contentItem.parent : null
    }
    readonly property var remoteActions: [
        "markdown", "text", "bookNote", "linkedEdit", "sheetTable", "sheetChart", "sheetDiagram",
        "newChart", "chartEdit", "newTable", "tableEdit", "diagramNew", "diagramEdit"
    ]
    readonly property var pageLinks: {
        const links = itemsOfType("link")
        if (!internalSheetsAvailable || !notebook || !links.length || !notebook.libraryItems.length)
            return links
        return links.filter(link => link.targetBookId !== bookId || !notebook.linkedPageItem(link.id).internalObjectSheet)
    }
    readonly property string heading: ({books: qsTr("Libri"), writing: qsTr("Scrittura"),
        charts: qsTr("Grafici"), tables: qsTr("Tabelle"), diagrams: qsTr("Diagrammi")})[workspace] || qsTr("Scrittura")
    readonly property string moreLabel: qsTr("Altro")
    readonly property real actionSpacing: 5 * uiScale
    readonly property real rowWidth: Math.max(0, width - 16 * uiScale)
    readonly property int primaryCount: Math.max(2, Math.min(workspace === "books" ? 4 : 3,
        Math.floor((rowWidth - moreWidth) / (44 * uiScale + actionSpacing))))
    readonly property bool compactActions: widthForActions(primaryCount) > rowWidth
    readonly property bool iconOnlyMore: rowWidth < 300 * uiScale
    readonly property real moreWidth: iconOnlyMore ? 44 * uiScale : actionFont.advanceWidth(moreLabel) + 54 * uiScale
    readonly property bool hasLinkedItem: linkedItemId.length > 0 && pageItems.some(item => item.id === linkedItemId)
    readonly property var sectionActions: workspace === "books" ? [
        {key: "highlighter", text: qsTr("Evidenzia"), icon: "pen"},
        {key: "bookNote", text: qsTr("Post-it"), icon: "note"},
        {key: "underline", text: qsTr("Sottolinea"), icon: "underline"},
        {key: "bookLink", text: qsTr("Collega elemento"), icon: "link"},
        {key: "annotatedPages", text: qsTr("Pagine annotate"), icon: "notebook"},
        {key: "bookSync", text: qsTr("Sincronizza libro"), icon: "sync"},
        {key: "mixedPage", text: qsTr("Riconosci pagina mista"), icon: "paper"},
        {key: "read", text: qsTr("Leggi"), icon: "notebook"},
        {key: "eraser", text: qsTr("Gomma"), icon: "eraser"}
    ] : workspace === "charts" ? [
        {key: "newChart", text: qsTr("Nuovo grafico"), icon: "plus"},
        {key: "chartSources", text: qsTr("Da tabella"), icon: "table"},
        {key: "chartEdit", text: qsTr("Dati e aspetto"), icon: "chart", kind: "chart"},
        {key: "chartInk", text: qsTr("Schizzo a mano"), icon: "pen"},
        {key: "lasso", text: qsTr("Lazo"), icon: "lasso"},
        {key: "chartCsv", text: qsTr("Esporta CSV"), icon: "download", kind: "chart"}
    ] : workspace === "tables" ? [
        {key: "tableEdit", text: qsTr("Celle e formule"), icon: "table", kind: "table"},
        {key: "newTable", text: qsTr("Nuova tabella"), icon: "plus"},
        {key: "importCsv", text: qsTr("Importa CSV"), icon: "download"},
        {key: "tableInk", text: qsTr("Scrivi nelle celle"), icon: "pen"},
        {key: "lasso", text: qsTr("Lazo per le celle"), icon: "lasso"},
        {key: "chartFromTable", text: qsTr("Crea grafico"), icon: "chart", kind: "table"},
        {key: "tableCsv", text: qsTr("Esporta CSV"), icon: "download", kind: "table"}
    ] : workspace === "diagrams" ? [
        {key: "diagramNew", text: qsTr("Forme e connettori"), icon: "plus"},
        {key: "diagramRecognize", text: qsTr("Riconosci diagramma"), icon: "diagram"},
        {key: "diagramEdit", text: qsTr("Modifica diagramma"), icon: "rename", kind: "diagram"},
        {key: "penTools", text: qsTr("Schizzo a mano"), icon: "pen"},
        {key: "lasso", text: qsTr("Lazo"), icon: "lasso"},
        {key: "diagramImport", text: qsTr("Importa draw.io"), icon: "download"},
        {key: "diagramExport", text: qsTr("Esporta draw.io"), icon: "download", kind: "diagram"},
        {key: "diagramAutomatic", text: qsTr("Riconoscimento automatico")}
    ] : [
        {key: "markdown", text: markdownBook ? qsTr("Modifica Markdown") : qsTr("Nuovo Markdown"), icon: "text"},
        {key: "text", text: qsTr("Inserisci testo"), icon: "text"},
        {key: "bookLink", text: qsTr("Collega elemento"), icon: "link"},
        {key: "image", text: qsTr("Inserisci immagine"), icon: "image"},
        {key: "library", text: qsTr("Dalla libreria"), icon: "layers"},
        {key: "sheetTable", text: qsTr("Aggiungi foglio tabella"), icon: "table"},
        {key: "sheetChart", text: qsTr("Aggiungi foglio grafico"), icon: "chart"},
        {key: "sheetDiagram", text: qsTr("Aggiungi foglio diagramma"), icon: "diagram"},
        {key: "mixedPage", text: qsTr("Riconosci pagina mista"), icon: "paper"}
    ]
    readonly property var actions: (hasLinkedItem
        ? [{key: "linkedEdit", text: qsTr("Modifica originale"), icon: "rename"}] : []).concat(sectionActions, [
        {key: "elements", text: editingItems ? qsTr("Fine modifica") : workspace === "books" ? qsTr("Gestisci annotazioni") : qsTr("Sposta elementi"), icon: "rename"}
    ], workspace === "writing" ? [] : [
        {key: "layers", text: qsTr("Livelli"), icon: "layers"}
    ])
    signal actionRequested(string action, string itemId)
    signal linkedItemRequested(string linkId, bool editOriginal)
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
        let width = moreWidth + (count + 1) * actionSpacing
        for (const action of actions.slice(0, count))
            width += actionFont.advanceWidth(action.text) + 54 * uiScale
        return width
    }

    function itemsOfType(kind: string): var {
        return pageItems.filter(item => item.type === kind)
    }

    function enabledFor(action: var, inMenu: bool): bool {
        if (!active || (!inMenu && !actionsEnabled)) return false
        if (action.key === "linkedEdit") return hasLinkedItem
        if (action.key === "mixedPage") return allowMixedPageImport
        if (action.key === "diagramRecognize") return diagramReviewAllowed && hasStrokes
        if (["highlighter", "underline", "bookNote", "bookLink", "eraser", "tableInk", "chartInk", "lasso", "penTools"].includes(action.key)) return canWrite
        if (action.key === "elements") return editingItems || pageItems.length > 0
        return !action.kind || itemsOfType(action.kind).length > 0
    }

    function actionSelected(key: string): bool {
        return key === penTool || ((key === "tableInk" || key === "chartInk") && penTool === "pen")
            || (key === "bookNote" && placing === "note") || (key === "bookLink" && placing === "link")
            || (key === "lasso" && lassoActive) || (key === "elements" && editingItems)
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
        if (action.key === "linkedEdit") {
            actionRequested(action.key, linkedItemId)
            return
        }
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

    function openToolsMenu(fullPage: bool) {
        moreMenu.begin(fullPage)
    }

    function closeDialogs() {
        moreMenu.pendingAction = null
        moreMenu.close()
        picker.pendingItem = ""
        picker.close()
        linkedItems.pendingLink = ""
        linkedItems.close()
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
            Item { Layout.fillWidth: true; visible: tools.workspace !== "books" || tools.syncStatus.length === 0 }
            PaperButton {
                objectName: "workspaceLinkedItemsButton"
                property bool remoteClickable: true
                uiScale: tools.uiScale
                text: qsTr("Elementi collegati")
                iconName: "link"
                iconOnly: tools.width < 580 * tools.uiScale
                quiet: true
                visible: tools.workspace === "writing" && tools.pageLinks.length > 0
                enabled: tools.active && tools.actionsEnabled
                ToolTip.visible: hovered && iconOnly && !tools.tabletMode
                ToolTip.text: text
                onClicked: linkedItems.open()
            }
            Repeater {
                model: [
                    {key: "undo", label: qsTr("Annulla"), icon: "undo", enabled: tools.canUndo},
                    {key: "redo", label: qsTr("Ripeti"), icon: "redo", enabled: tools.canRedo},
                    {key: "fullPage", label: qsTr("Pagina intera"), icon: "expand", enabled: !tools.hasReaderControls}
                ]
                PaperButton {
                    required property var modelData
                    objectName: "workspaceAction_" + modelData.key
                    uiScale: tools.uiScale
                    text: modelData.label
                    iconName: modelData.icon
                    iconOnly: true
                    quiet: true
                    visible: modelData.key !== "fullPage" || !tools.hasReaderControls
                    enabled: tools.active && tools.actionsEnabled && modelData.enabled
                    ToolTip.visible: hovered && !tools.tabletMode
                    ToolTip.text: text
                    onClicked: tools.actionRequested(modelData.key, "")
                }
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
                    property bool remoteClickable: tools.remoteActions.includes(modelData.key)
                    readonly property bool primary: index < tools.primaryCount
                    objectName: !primary ? "" : modelData.key === "diagramRecognize" ? "recognizeDiagramButton" : "workspaceAction_" + modelData.key
                    visible: primary
                    uiScale: tools.uiScale
                    text: modelData.text
                    iconName: modelData.icon || ""
                    iconOnly: tools.compactActions
                    filled: tools.actionSelected(modelData.key)
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
                property bool remoteClickable: true
                uiScale: tools.uiScale
                text: tools.moreLabel
                iconName: "more"
                iconOnly: tools.iconOnlyMore
                quiet: true
                enabled: tools.active && tools.actionsEnabled
                ToolTip.visible: hovered && iconOnly && !tools.tabletMode
                ToolTip.text: text
                onClicked: tools.openToolsMenu(false)
            }
        }
    }

    Dialog {
        id: linkedItems
        objectName: "workspaceLinkedItems"
        title: qsTr("Elementi collegati")
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(520 * tools.uiScale, (parent ? parent.width : 520 * tools.uiScale) - 24 * tools.uiScale)
        modal: true
        dim: !tools.tabletMode
        padding: 14 * tools.uiScale
        standardButtons: Dialog.Close
        property string pendingLink: ""
        property bool pendingEdit: false
        readonly property var entries: {
            if (!visible || !tools.notebook) return []
            const hasCatalog = tools.notebook.libraryItems.length > 0
            return tools.pageLinks.map(link => ({
                id: link.id, kind: link.targetType,
                target: hasCatalog ? tools.notebook.linkedPageItem(link.id) : ({})
            }))
        }

        function choose(id: string, edit: bool) {
            pendingLink = id
            pendingEdit = edit
            close()
        }

        onClosed: {
            const id = pendingLink
            const edit = pendingEdit
            const book = tools.bookId
            const page = tools.pageId
            pendingLink = ""
            if (!id) return
            Qt.callLater(function() {
                if (tools.visible && tools.active && tools.bookId === book && tools.pageId === page
                    && tools.pageLinks.some(link => link.id === id))
                    tools.linkedItemRequested(id, edit)
            })
        }

        contentItem: ListView {
            id: linkedEntries
            implicitHeight: Math.min(count * 205 * tools.uiScale,
                linkedItems.parent ? linkedItems.parent.height * .65 : 460 * tools.uiScale)
            model: linkedItems.entries
            clip: true
            spacing: 12 * tools.uiScale
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: ColumnLayout {
                required property var modelData
                width: linkedEntries.width
                spacing: 5 * tools.uiScale
                readonly property var targetItem: modelData.target
                readonly property bool resolved: Boolean(targetItem.id)
                readonly property string kindName: modelData.kind === "table" ? qsTr("Tabella")
                    : modelData.kind === "chart" ? qsTr("Grafico") : qsTr("Diagramma")
                readonly property int previewColumns: resolved && modelData.kind === "table" ? Math.min(targetItem.cols, 4) : 0
                readonly property int previewRows: previewColumns ? Math.min(targetItem.rows, 3) : 0
                readonly property var previewCells: previewRows ? tools.notebook.tableCellValues(targetItem) : []

                RowLayout {
                    Layout.fillWidth: true
                    PaperIcon {
                        name: modelData.kind
                        tint: "#515159"
                        Layout.preferredWidth: 18 * tools.uiScale
                        Layout.preferredHeight: Layout.preferredWidth
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 1 * tools.uiScale
                        Label {
                            Layout.fillWidth: true
                            text: resolved ? targetItem.displayTitle || targetItem.bookTitle : qsTr("Destinazione non disponibile.")
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: resolved
                            text: qsTr("%1 · %2, pagina %3").arg(kindName)
                                .arg(targetItem.bookTitle || "").arg(targetItem.pageNumber || "")
                            font.pixelSize: 12 * tools.uiScale
                            color: "#515159"
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }
                    }
                }
                GridLayout {
                    Layout.fillWidth: true
                    visible: previewRows > 0
                    columns: Math.max(1, previewColumns)
                    columnSpacing: 4 * tools.uiScale
                    rowSpacing: 1 * tools.uiScale
                    Repeater {
                        model: previewColumns * previewRows
                        Label {
                            required property int index
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.minimumWidth: 0
                            padding: 3 * tools.uiScale
                            text: previewCells[Math.floor(index / previewColumns) * targetItem.cols
                                + index % previewColumns] || ""
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            background: Rectangle { color: "#f2f2f7" }
                        }
                    }
                }
                DiagramItem {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 105 * tools.uiScale
                    visible: resolved && modelData.kind === "diagram"
                    diagram: visible ? targetItem : ({})
                }
                RowLayout {
                    Layout.fillWidth: true
                    visible: resolved
                    Item { Layout.fillWidth: true }
                    PaperButton {
                        objectName: "workspaceLinkedOpen_" + modelData.id
                        property bool remoteClickable: true
                        uiScale: tools.uiScale
                        text: qsTr("Apri")
                        quiet: true
                        onClicked: linkedItems.choose(modelData.id, false)
                    }
                    PaperButton {
                        objectName: "workspaceLinkedEdit_" + modelData.id
                        property bool remoteClickable: true
                        uiScale: tools.uiScale
                        text: qsTr("Modifica")
                        onClicked: linkedItems.choose(modelData.id, true)
                    }
                }
            }
        }
    }

    Popup {
        id: moreMenu
        objectName: "workspaceMoreMenu"
        parent: Overlay.overlay
        x: parent ? Math.round((parent.width - width) / 2) : 0
        y: !parent ? 0 : Math.round(fullPage ? parent.height - height - 12 * tools.uiScale
            : (parent.height - height) / 2)
        width: Math.max(0, Math.min((fullPage ? 350 : 290) * tools.uiScale,
            parent ? parent.width - 16 * tools.uiScale : 290 * tools.uiScale))
        padding: 8 * tools.uiScale
        modal: true
        dim: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property bool fullPage: false
        property var pendingAction: null
        property string sourceBook: ""
        property string sourcePage: ""
        property string sourceWorkspace: ""

        function begin(showAll: bool) {
            if (!parent || !tools.active || !tools.actionsEnabled) return
            pendingAction = null
            fullPage = showAll
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
                    if (button.y < menuScroller.contentY)
                        menuScroller.contentY = button.y
                    else if (button.y + button.height > menuScroller.contentY + menuScroller.height)
                        menuScroller.contentY = button.y + button.height - menuScroller.height
                    return
                }
            }
        }

        onOpened: {
            menuScroller.contentY = 0
            moveFocus(1)
        }

        onClosed: {
            const action = pendingAction
            const book = sourceBook
            const page = sourcePage
            const workspace = sourceWorkspace
            pendingAction = null
            if (!action) return
            Qt.callLater(function() {
                if (tools.active && tools.bookId === book && tools.pageId === page
                    && tools.workspace === workspace) tools.runAction(action)
            })
        }

        background: Rectangle {
            color: "white"
            border.color: "#c7c7cc"
            border.width: Math.max(1, tools.uiScale)
            radius: 12 * tools.uiScale
        }

        contentItem: Flickable {
            id: menuScroller
            implicitHeight: Math.min(menuColumn.implicitHeight,
                Math.max(44 * tools.uiScale, (moreMenu.parent ? moreMenu.parent.height : 520)
                    * (moreMenu.fullPage ? .72 : 1) - 44 * tools.uiScale))
            clip: true
            contentHeight: menuColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            Column {
                id: menuColumn
                width: menuScroller.width
                spacing: 4 * tools.uiScale
                Keys.onUpPressed: moreMenu.moveFocus(-1)
                Keys.onDownPressed: moreMenu.moveFocus(1)
                PaperButton {
                    objectName: "workspaceToolsCloseButton"
                    property bool remoteClickable: true
                    visible: moreMenu.fullPage
                    width: menuColumn.width
                    uiScale: tools.uiScale
                    iconName: "close"
                    text: qsTr("Chiudi strumenti")
                    alignLeft: true
                    onClicked: moreMenu.close()
                }
                Repeater {
                    id: menuOptions
                    model: moreMenu.fullPage ? tools.actions : tools.actions.slice(tools.primaryCount)
                    PaperButton {
                        required property var modelData
                        property bool remoteClickable: tools.remoteActions.includes(modelData.key)
                        objectName: moreMenu.fullPage ? "workspaceToolsMenuAction_" + modelData.key
                            : modelData.key === "diagramAutomatic" ? "diagramAutoRecognition" : "workspaceAction_" + modelData.key
                        width: menuColumn.width
                        uiScale: tools.uiScale
                        text: modelData.text
                        iconName: modelData.icon || ""
                        alignLeft: true
                        quiet: true
                        filled: checkable ? tools.autoDiagramRecognition : tools.actionSelected(modelData.key)
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
                    property bool remoteClickable: tools.remoteActions.includes(picker.action)
                    width: options.width
                    uiScale: tools.uiScale
                    text: modelData.title
                    alignLeft: true
                    quiet: true
                    onClicked: picker.choose(modelData.id)
                }
            }
            PaperButton {
                property bool remoteClickable: true
                Layout.alignment: Qt.AlignRight
                uiScale: tools.uiScale
                text: qsTr("Annulla")
                onClicked: picker.close()
            }
        }
    }
}
