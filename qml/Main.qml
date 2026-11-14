import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import Quaderno

ApplicationWindow {
    id: window

    required property Notebook notebook
    required property AppleShare appleShare
    required property WifiSettings wifi
    required property PinLock pinLock
    required property DeviceInfo deviceInfo
    required property UsbSsh usbSsh
    required property WirelessSync wireless
    property var bluetooth: null
    property var externalKeyboard: null
    required property MacDocumentExchange documentExchange
    required property MarkdownWeb markdownWeb
    required property ObsidianIntegration obsidian
    required property ObsidianSync obsidianSync
    required property EpaperRefresh epaperRefresh
    required property bool tabletMode
    required property rect epaperGeometry
    required property string fontFamily
    readonly property bool showTouchKeyboard: tabletMode && !Boolean(externalKeyboard?.connected)

    readonly property real uiScale: tabletMode ? Math.max(1, Math.min(width, height) / 790) : 1
    readonly property real gutter: (width < 480 * uiScale ? 12 : 24) * uiScale
    readonly property real line: Math.max(1, Math.round(uiScale))
    readonly property bool wideLibrary: width >= 690 * uiScale
    readonly property bool compactLibraryActions: width < 890 * uiScale
    readonly property bool stackedInkToolbar: width < 440 * uiScale
    readonly property color ink: "#1c1c1e"
    readonly property color muted: "#515159"
    readonly property color border: "#d1d1d6"
    readonly property color surface: "#f2f2f7"
    readonly property color secondarySurface: "#e9e9ef"
    readonly property color accent: "#0059bd"
    readonly property color accentLight: "#e6effb"
    readonly property var libraryBooks: organizer.filteredBooks("")
    readonly property var filteredBooks: organizer.filteredBooks(search.text)
    readonly property bool editorPopupOpen: nameDialog.visible || appleShareView.visible
        || pageTemplateDialog.visible || pageItemDialog.visible || pageOverview.visible
        || penTools.visible || documentPicker.visible || pdfSavePicker.visible || pageSearch.visible
        || pageLayers.visible || pageTags.visible || chartDialog.visible || selectionDialog.visible || csvSavePicker.visible
        || inkTransform.visible || usbTransferDialog.visible || usbImportDialog.visible || tableChartDialog.visible
        || bookmarksDialog.visible || wirelessDialog.visible || markdownDialog.visible || markdownSavePicker.visible
        || obsidianSyncDialog.visible || diagramEditor.visible || diagramSavePicker.visible || diagramOpenPicker.visible
        || csvOpenPicker.visible || appleBooksDialog.visible || markdownEditor.visible || imagePicker.visible
        || insertLibraryItemDialog.visible || tableCsvSavePicker.visible || imageProperties.visible
    readonly property bool wirelessEditorBusy: inkPage.drawing || editingItems || (editorPopupOpen && !wirelessDialog.visible)
    onWirelessEditorBusyChanged: wireless.transfer.setEditorBusy(wirelessEditorBusy)
    readonly property bool fastPanelRefresh: pinLock.locked || !editing || showingSettings || editorPopupOpen
        || (!notebook.hasPageBackground && !notebook.pageItems.some(item =>
            item.type === "image" || item.type === "chart" || item.type === "diagram"))
    onFastPanelRefreshChanged: epaperRefresh.setFastRefresh(fastPanelRefresh)
    Component.onCompleted: epaperRefresh.setFastRefresh(fastPanelRefresh)
    readonly property string displayedPageKey: visible && editing && !showingSettings && !pinLock.locked
        && notebook.bookId.length > 0 && notebook.pageId.length > 0 ? notebook.bookId + "/" + notebook.pageId : ""
    readonly property bool canRefreshPage: epaperRefresh.available && displayedPageKey.length > 0
        && !inkPage.drawing && !paperViewport.moving && !editorPopupOpen && !organizer.dialogOpen && !settingsView.dialogOpen
    onDisplayedPageKeyChanged: epaperRefresh.watchPageChange(displayedPageKey.length > 0)
    readonly property var currentSync: {
        const version = wireless.transfer.bookStatusVersion
        return notebook.bookId.length ? wireless.transfer.bookStatus(notebook.bookId) : ({})
    }
    readonly property bool hasResumeOffer: Boolean(wireless.transfer.resumeOffer.pageId)

    property bool editing: false
    property bool showingSettings: false
    property bool fullPage: false
    property bool editingItems: false
    property bool lassoActive: false
    property var inkClipboard: []
    property real readingZoom: 1
    property bool readingFitWidth: false
    property bool smartShapesEnabled: false
    property string selectionMessage: ""
    property string selectedPageItem: ""
    property var newLibraryItemBook: null
    property string penColor: "#222622"
    property string penTool: "pen"
    property int penThickness: 1
    readonly property var penToolsModel: [
        { id: "pen", label: qsTr("Penna"), width: 2.5 },
        { id: "pencil", label: qsTr("Matita"), width: 2 },
        { id: "marker", label: qsTr("Pennarello"), width: 4 },
        { id: "highlighter", label: qsTr("Evidenziatore"), width: 9 },
        { id: "underline", label: qsTr("Sottolinea"), width: 2.5 },
        { id: "eraser", label: qsTr("Gomma"), width: 8 },
        { id: "line", label: qsTr("Linea"), width: 2.5 },
        { id: "rectangle", label: qsTr("Rettangolo"), width: 2.5 },
        { id: "ellipse", label: qsTr("Ellisse"), width: 2.5 }
    ]
    readonly property var selectedTool: penToolsModel.find(tool => tool.id === penTool)

    title: qsTr("Quaderno")
    // The e-paper platform does not resize the window on a fullscreen request.
    width: epaperGeometry.width > 0 ? epaperGeometry.width : Math.max(380, Math.min(960, Screen.desktopAvailableWidth - 56))
    height: epaperGeometry.height > 0 ? epaperGeometry.height : Math.max(500, Math.min(790, Screen.desktopAvailableHeight - 52))
    minimumWidth: 380
    minimumHeight: 500
    visibility: tabletMode ? Window.FullScreen : Window.Windowed
    font.family: fontFamily
    color: surface
    palette.window: surface
    palette.base: "white"
    palette.windowText: ink
    palette.text: ink
    palette.buttonText: ink
    palette.highlight: accent
    palette.highlightedText: "white"

    function refreshPage() {
        if (canRefreshPage)
            epaperRefresh.request()
    }

    function showBook(id: string) {
        if (notebook.openBook(id))
            showCurrentBook()
    }

    function openLibraryItem(entry: var) {
        const item = notebook.openLibraryItem(entry.bookId, entry.pageId, entry.itemId)
        if (!item.id)
            return
        showCurrentBook()
        editPageItem(item.type, item)
    }

    function showCurrentBook() {
        librarySearch.active = false
        settingsView.closeDialogs()
        showingSettings = false
        editing = true
        pageView.forceActiveFocus()
    }

    function syncStateLabel(state: string): string {
        switch (state) {
        case "sending": return qsTr("Sincronizzazione in corso…")
        case "upToDate": return qsTr("Ultima versione condivisa")
        case "localChanges": return qsTr("Modifiche da sincronizzare")
        case "conflict": return qsTr("Conflitto di sincronizzazione")
        case "error": return qsTr("Sincronizzazione da verificare")
        default: return ""
        }
    }

    Binding {
        target: window.documentExchange
        property: "importPaused"
        value: window.showingSettings || window.editorPopupOpen || organizer.dialogOpen || inkPage.drawing
    }

    Connections {
        target: window.documentExchange
        function onImported(bookId) {
            const book = window.notebook.books.find(entry => entry.id === bookId)
            if (book)
                organizer.section = book.document ? "books" : "writing"
            window.showBook(bookId)
        }
    }

    function selectTool(tool: string) {
        inkPage.cancelDrawing()
        penTool = tool
        editingItems = false
        lassoActive = false
        selectedPageItem = ""
    }

    function showLibrary() {
        inkPage.cancelDrawing()
        settingsView.closeDialogs()
        editing = false
        fullPage = false
        editingItems = false
        lassoActive = false
        selectionMessage = ""
        selectedPageItem = ""
        showingSettings = false
        if (newBook.visible)
            newBook.forceActiveFocus()
    }

    function showSettings() {
        inkPage.cancelDrawing()
        librarySearch.active = false
        showingSettings = true
        settingsView.open()
    }

    function shareBook() {
        inkPage.cancelDrawing()
        if (appleShare.available) {
            appleShareView.open()
        } else {
            pdfSavePicker.selectedFile = notebook.suggestedPdfName()
            pdfSavePicker.open()
        }
    }

    function nameBook(renaming: bool) {
        inkPage.cancelDrawing()
        librarySearch.active = false
        if (!renaming && organizer.section === "writing") {
            markdownEditor.begin(true)
            return
        }
        nameDialog.renaming = renaming
        nameDialog.createType = renaming ? "" : organizer.section === "tables" ? "table"
            : "chart"
        nameField.text = renaming ? notebook.title : ""
        nameDialog.open()
    }

    function saveEditedItem(item: var): bool {
        if (!newLibraryItemBook)
            return notebook.savePageItem(item)
        if (!notebook.createBookWithItem(newLibraryItemBook.title, newLibraryItemBook.folderId, item))
            return false
        newLibraryItemBook = null
        showCurrentBook()
        return true
    }

    function openDocumentPicker() {
        documentPicker.nameFilters = organizer.section === "books"
            ? [qsTr("Libri (*.pdf *.PDF *.epub *.EPUB)")]
            : [qsTr("Documenti (*.pdf *.epub *.md *.markdown *.MD *.MARKDOWN)"),
               qsTr("PDF (*.pdf)"), qsTr("EPUB (*.epub)"), qsTr("Markdown (*.md *.markdown *.MD *.MARKDOWN)")]
        documentPicker.open()
    }

    function editPageItem(type: string, item: var) {
        inkPage.cancelDrawing()
        lassoActive = false
        selectedPageItem = item && item.id ? item.id : ""
        if (type === "image") {
            imageProperties.item = item
            imageProperties.open()
        } else if (type === "diagram") {
            diagramEditor.openFor(item)
        } else if (type === "chart" && item.sourceTable) {
            const source = notebook.pageItems.find(element => element.id === item.sourceTable)
            tableChartDialog.openFor(source, item)
        } else if (type === "chart")
            chartDialog.openItem(item)
        else
            pageItemDialog.openFor(type, item, "", null)
    }

    function editPageText(id: string) {
        const item = notebook.pageItems.find(entry => entry.id === id && entry.type === "text")
        if (item && editing && !showingSettings && !pinLock.locked && !editorPopupOpen) {
            pinLock.recordActivity()
            editPageItem("text", item)
        }
    }

    function openInkTransform(strokes: var, pasting: bool, indexes: var) {
        inkPage.cancelDrawing()
        lassoActive = false
        inkTransform.openFor(strokes, pasting, notebook.pagePreview(notebook.page), indexes)
    }

    function setLasso(active: bool) {
        inkPage.cancelDrawing()
        editingItems = false
        selectedPageItem = ""
        selectionMessage = ""
        lassoActive = active
    }

    function useLasso(polygon: var) {
        const selection = notebook.selectInk(polygon)
        if (!selection.strokes || selection.strokes.length === 0) {
            selectionMessage = qsTr("Nessun tratto selezionato. Circonda i tratti del livello attivo.")
            return
        }
        lassoActive = false
        selectionDialog.selection = selection
        selectionDialog.open()
        if (tabletMode && handwriting.available && selectionDialog.hasHandwriting)
            handwriting.recognize(selection.strokes)
    }

    function openPageSearch() {
        inkPage.cancelDrawing()
        pageQuery.clear()
        pageSearch.open()
    }

    function setReadingZoom(zoom: real, fitWidth: bool) {
        inkPage.cancelDrawing()
        const x = (paperViewport.contentX + paperViewport.width / 2) / Math.max(1, paperViewport.contentWidth)
        const y = (paperViewport.contentY + paperViewport.height / 2) / Math.max(1, paperViewport.contentHeight)
        readingZoom = zoom
        readingFitWidth = fitWidth
        Qt.callLater(function() {
            paperViewport.contentX = Math.max(0, Math.min(paperViewport.contentWidth - paperViewport.width,
                                                         x * paperViewport.contentWidth - paperViewport.width / 2))
            paperViewport.contentY = Math.max(0, Math.min(paperViewport.contentHeight - paperViewport.height,
                                                         y * paperViewport.contentHeight - paperViewport.height / 2))
        })
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: !window.editing && !window.showingSettings && !window.pinLock.locked
                 && !window.editorPopupOpen && !organizer.dialogOpen && window.notebook.books.length > 0
        onActivated: search.forceActiveFocus()
    }

    Shortcut {
        sequence: "Escape"
        enabled: (window.editing || window.showingSettings || librarySearch.active)
                 && !window.pinLock.locked
                 && !window.editorPopupOpen && !settingsView.dialogOpen && !organizer.dialogOpen
        onActivated: {
            if (librarySearch.active) {
                librarySearch.active = false
                newBook.forceActiveFocus()
            } else if (window.fullPage) {
                window.fullPage = false
            } else if (window.lassoActive) {
                window.setLasso(false)
            } else if (window.editingItems) {
                window.editingItems = false
            } else {
                window.showLibrary()
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: window.editing && !window.editingItems && !window.pinLock.locked && !window.editorPopupOpen
        onActivated: window.notebook.undo()
    }

    Shortcut {
        sequences: [StandardKey.Redo]
        enabled: window.editing && !window.editingItems && !window.pinLock.locked && !window.editorPopupOpen
        onActivated: window.notebook.redo()
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: window.editing && !window.pinLock.locked && !window.editorPopupOpen
        onActivated: window.openPageSearch()
    }

    ButtonGroup { id: penColors }

    UsbTransfer { id: usbTransfer }

    Handwriting {
        id: handwriting
        onFinished: success => {
            if (success && !window.tabletMode)
                selectionDialog.reviewTranscription()
        }
    }

    Connections {
        target: window.pinLock
        function onLockedChanged() {
            if (window.pinLock.locked) {
                inkPage.cancelDrawing()
                librarySearch.active = false
                nameDialog.close()
                appleShareView.close()
                appleBooksDialog.close()
                pageTemplateDialog.close()
                pageItemDialog.close()
                pageOverview.close()
                penTools.close()
                documentPicker.close()
                pdfSavePicker.close()
                markdownSavePicker.close()
                markdownEditor.close()
                imagePicker.close()
                imageProperties.close()
                insertLibraryItemDialog.close()
                tableCsvSavePicker.close()
                window.newLibraryItemBook = null
                markdownDialog.clearPrivateState()
                obsidianSyncDialog.close()
                pageSearch.close()
                pageLayers.close()
                pageTags.close()
                window.setLasso(false)
                selectionDialog.close()
                chartDialog.close()
                diagramSavePicker.close()
                diagramOpenPicker.close()
                diagramSavePicker.draft = null
                diagramEditor.close()
                inkTransform.close()
                inkClipboard = []
                tableChartDialog.close()
                usbTransferDialog.close()
                wirelessDialog.close()
                usbImportDialog.close()
                bookmarksDialog.close()
                csvOpenPicker.close()
                csvSavePicker.close()
                organizer.closeDialogs()
                settingsView.closeDialogs()
            }
        }
    }

    component TopButton: PaperButton {
        uiScale: window.uiScale
        hoverEnabled: !window.tabletMode
    }

    component InkLabel: Label {
        color: window.ink
        font.pixelSize: 15 * window.uiScale
        textFormat: Text.PlainText
    }

    ColumnLayout {
        anchors.fill: parent
        visible: !window.pinLock.locked
        spacing: 0

        Rectangle {
            visible: !window.fullPage
            Layout.fillWidth: true
            implicitHeight: (window.editing ? 56 : 60) * window.uiScale
            color: "white"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: window.gutter
                anchors.rightMargin: anchors.leftMargin
                spacing: 4 * window.uiScale

                TopButton {
                    text: qsTr("Libreria")
                    visible: window.editing || window.showingSettings
                    iconName: "left"
                    iconOnly: window.width < 530 * window.uiScale
                    quiet: true
                    onClicked: window.showLibrary()
                }

                InkLabel {
                    visible: !window.editing && !window.showingSettings
                    Layout.fillWidth: true
                    text: window.title
                    font.pixelSize: 20 * window.uiScale
                    font.weight: Font.DemiBold
                }

                InkLabel {
                    visible: window.showingSettings
                    Layout.fillWidth: true
                    text: qsTr("Impostazioni")
                    font.pixelSize: 20 * window.uiScale
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                TopButton {
                    visible: window.editing
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: window.notebook.title
                    Accessible.name: qsTr("Rinomina %1").arg(window.notebook.title)
                    iconName: "rename"
                    alignLeft: true
                    font.pixelSize: 18 * window.uiScale
                    quiet: true
                    onClicked: window.nameBook(true)
                }

                TopButton {
                    visible: !window.editing && !window.showingSettings
                    text: qsTr("Impostazioni")
                    iconName: "settings"
                    iconOnly: window.width < 560 * window.uiScale
                    quiet: true
                    onClicked: window.showSettings()
                }

                TopButton {
                    visible: window.editing && usbTransfer.available && !window.tabletMode
                    text: qsTr("Invia al Paper Pro")
                    iconName: "upload"
                    iconOnly: window.width < 760 * window.uiScale
                    quiet: true
                    onClicked: usbTransferDialog.openFor(window.notebook.bookId)
                }

                TopButton {
                    visible: !window.showingSettings
                    text: qsTr("Markdown e Obsidian")
                    iconName: "text"
                    iconOnly: true
                    quiet: true
                    ToolTip.visible: !window.tabletMode && hovered && iconOnly
                    ToolTip.text: text
                    onClicked: markdownDialog.begin(window.editing, organizer.destinationFolderId)
                }

                TopButton {
                    visible: !window.showingSettings && window.wireless.available
                    text: qsTr("Sincronizzazione wireless")
                    iconName: "sync"
                    iconOnly: true
                    quiet: true
                    filled: window.hasResumeOffer || window.wireless.transfer.connected
                    onClicked: wirelessDialog.begin()
                }

                TopButton {
                    visible: window.editing && window.wireless.transfer.connected && window.notebook.pageId.length > 0
                    text: window.tabletMode ? qsTr("Continua sul Mac") : qsTr("Continua sul Paper Pro")
                    iconName: "continue"
                    iconOnly: window.width < 770 * window.uiScale
                    quiet: true
                    enabled: !window.wireless.transfer.busy && !window.wireless.transfer.resumeBusy
                             && !window.wireless.transfer.previewInfo.action
                    ToolTip.visible: !window.tabletMode && hovered && iconOnly
                    ToolTip.text: text
                    onClicked: {
                        wirelessDialog.begin()
                        window.wireless.transfer.sendResume(window.notebook.bookId, window.notebook.pageId)
                    }
                }

                TopButton {
                    visible: window.editing
                    text: window.appleShare.available ? qsTr("Condividi") : qsTr("Salva PDF")
                    iconName: "share"
                    iconOnly: window.width < 680 * window.uiScale
                    quiet: true
                    onClicked: window.shareBook()
                }

                TopButton {
                    visible: window.tabletMode
                    text: qsTr("Esci")
                    iconName: "exit"
                    iconOnly: window.width < 530 * window.uiScale
                    quiet: true
                    onClicked: {
                        inkPage.cancelDrawing()
                        Qt.quit()
                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: window.line
                color: window.border
            }
        }

        RowLayout {
            visible: window.hasResumeOffer && !window.fullPage && !window.showingSettings
            Layout.fillWidth: true
            Layout.leftMargin: window.gutter
            Layout.rightMargin: window.gutter
            spacing: 8 * window.uiScale

            InkLabel {
                Layout.fillWidth: true
                elide: Text.ElideRight
                text: qsTr("Ricevuta una pagina da aprire")
                font.pixelSize: 14 * window.uiScale
            }
            TopButton {
                text: qsTr("Mostra richiesta")
                quiet: true
                enabled: !window.editorPopupOpen
                onClicked: wirelessDialog.begin()
            }
        }

        InkLabel {
            readonly property string state: window.currentSync.state || ""
            visible: window.editing && !window.fullPage && text.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: window.gutter
            Layout.rightMargin: window.gutter
            Layout.topMargin: 4 * window.uiScale
            text: window.syncStateLabel(state)
            elide: Text.ElideRight
            font.pixelSize: 13 * window.uiScale
            color: state === "conflict" || state === "error" ? "#87332c" : window.muted
        }

        SettingsView {
            id: settingsView
            visible: window.showingSettings
            Layout.fillWidth: true
            Layout.fillHeight: true
            uiScale: window.uiScale
            tabletMode: window.tabletMode
            wifi: window.wifi
            pinLock: window.pinLock
            deviceInfo: window.deviceInfo
            usbSsh: window.usbSsh
            wireless: window.wireless
            bluetooth: window.bluetooth
            epaperRefresh: window.epaperRefresh
            showTouchKeyboard: window.showTouchKeyboard
            onOpenBluetooth: wirelessDialog.begin(true)
        }

        GridLayout {
            visible: !window.editing && !window.showingSettings
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: window.gutter
            Layout.rightMargin: window.gutter
            Layout.topMargin: (window.wideLibrary ? 20 : 12) * window.uiScale
            Layout.bottomMargin: 16 * window.uiScale
            columns: window.wideLibrary ? 2 : 1
            columnSpacing: 24 * window.uiScale
            rowSpacing: 12 * window.uiScale

            Rectangle {
                Layout.fillWidth: !window.wideLibrary
                Layout.fillHeight: window.wideLibrary
                Layout.preferredWidth: 210 * window.uiScale
                Layout.preferredHeight: organizer.implicitHeight + 2 * organizer.anchors.margins
                Layout.alignment: Qt.AlignTop
                radius: 16 * window.uiScale
                color: window.wideLibrary ? window.secondarySurface : "transparent"

                LibraryOrganizer {
                    id: organizer
                    showTouchKeyboard: window.showTouchKeyboard
                    anchors.fill: parent
                    anchors.margins: window.wideLibrary ? 10 * window.uiScale : 0
                    notebook: window.notebook
                    uiScale: window.uiScale
                    tabletMode: window.tabletMode
                    sidebar: window.wideLibrary
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                spacing: 12 * window.uiScale

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8 * window.uiScale

                    InkLabel {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: organizer.viewMode === "trash" ? qsTr("Cestino")
                            : organizer.sections.find(section => section.key === organizer.section).label
                        font.pixelSize: (window.wideLibrary ? 34 : 27) * window.uiScale
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }

                    TopButton {
                        visible: window.compactLibraryActions && organizer.viewMode !== "trash"
                        text: qsTr("Importa")
                        iconName: "download"
                        iconOnly: true
                        quiet: true
                        ToolTip.visible: !window.tabletMode && hovered
                        ToolTip.text: text
                        onClicked: importMenu.open()

                        Menu {
                            id: importMenu
                            y: parent.height
                            dim: !window.tabletMode && modal

                            MenuItem {
                                visible: window.appleShare.available
                                height: visible ? implicitHeight : 0
                                text: qsTr("Importa dal Paper Pro")
                                enabled: !window.usbSsh.importBusy && !window.appleShare.busy
                                onTriggered: usbImportDialog.openPreview()
                            }
                            MenuItem {
                                visible: window.appleShare.available
                                height: visible ? implicitHeight : 0
                                text: qsTr("Importa quaderni")
                                enabled: !window.usbSsh.importBusy && !window.appleShare.busy
                                onTriggered: window.appleShare.importNotebookFile(window.notebook)
                            }
                            MenuItem {
                                visible: window.documentExchange.available
                                    && organizer.section === "books"
                                height: visible ? implicitHeight : 0
                                text: qsTr("Importa da Apple Books")
                                onTriggered: appleBooksDialog.open()
                            }
                            MenuItem {
                                text: qsTr("Importa documento")
                                onTriggered: {
                                    librarySearch.active = false
                                    window.openDocumentPicker()
                                }
                            }
                        }
                    }

                    TopButton {
                        id: newBook
                        visible: organizer.viewMode !== "trash" && organizer.section !== "books"
                        text: organizer.section === "tables" ? qsTr("Nuova tabella")
                            : organizer.section === "charts" ? qsTr("Nuovo grafico") : qsTr("Nuovo Markdown")
                        iconName: "plus"
                        filled: true
                        onClicked: window.nameBook(false)
                    }
                }

                Flow {
                    visible: !window.compactLibraryActions && organizer.viewMode !== "trash"
                    Layout.fillWidth: true
                    spacing: 6 * window.uiScale

                    TopButton {
                        visible: window.appleShare.available
                        text: qsTr("Importa dal Paper Pro")
                        iconName: "download"
                        quiet: true
                        enabled: !window.usbSsh.importBusy && !window.appleShare.busy
                        onClicked: usbImportDialog.openPreview()
                    }

                    TopButton {
                        visible: window.appleShare.available
                        text: qsTr("Importa quaderni")
                        iconName: "notebook"
                        quiet: true
                        enabled: !window.usbSsh.importBusy && !window.appleShare.busy
                        onClicked: window.appleShare.importNotebookFile(window.notebook)
                    }

                    TopButton {
                        visible: window.documentExchange.available && organizer.section === "books"
                        text: qsTr("Importa da Apple Books")
                        iconName: "notebook"
                        quiet: true
                        onClicked: appleBooksDialog.open()
                    }

                    TopButton {
                        text: qsTr("Importa documento")
                        iconName: "paper"
                        quiet: true
                        onClicked: {
                            librarySearch.active = false
                            window.openDocumentPicker()
                        }
                    }
                }

                InkLabel {
                    visible: window.appleShare.available && text.length > 0
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: window.muted
                    text: window.appleShare.status
                }

                ColumnLayout {
                    id: librarySearch
                    property bool active: false
                    visible: window.notebook.books.length > 0
                    Layout.fillWidth: true
                    spacing: 6 * window.uiScale

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 46 * window.uiScale
                        color: "white"
                        border.color: search.activeFocus ? window.accent : "transparent"
                        border.width: search.activeFocus ? 2 * window.line : 0
                        radius: 12 * window.uiScale

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12 * window.uiScale
                            anchors.rightMargin: 4 * window.uiScale
                            spacing: 6 * window.uiScale

                            PaperIcon {
                                name: "search"
                                tint: window.muted
                                Layout.preferredWidth: 18 * window.uiScale
                                Layout.preferredHeight: Layout.preferredWidth
                            }

                            TextField {
                                id: search
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                placeholderText: qsTr("Cerca nella sezione")
                                Accessible.name: placeholderText
                                selectByMouse: true
                                leftPadding: 0
                                rightPadding: 0
                                color: window.ink
                                placeholderTextColor: window.muted
                                font.pixelSize: 15 * window.uiScale
                                background: Item {}
                                onPressed: if (window.tabletMode) librarySearch.active = true
                                onAccepted: librarySearch.active = false
                            }

                            TopButton {
                                visible: search.text.length > 0 || (librarySearch.active && window.showTouchKeyboard)
                                text: search.text.length > 0 ? qsTr("Cancella ricerca") : qsTr("Chiudi tastiera")
                                iconName: "close"
                                iconOnly: true
                                quiet: true
                                focusPolicy: Qt.NoFocus
                                onClicked: {
                                    search.clear()
                                    librarySearch.active = false
                                }
                            }
                        }
                    }

                    TouchKeyboard {
                        visible: librarySearch.active && window.showTouchKeyboard
                        Layout.fillWidth: true
                        uiScale: window.uiScale
                        target: search
                    }

                    TopButton {
                        visible: librarySearch.active && window.showTouchKeyboard && search.text.length > 0
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Mostra risultati")
                        onClicked: librarySearch.active = false
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0

                    Rectangle {
                        width: parent.width
                        height: Math.min(parent.height, books.contentHeight)
                        radius: 14 * window.uiScale
                        color: "white"
                        visible: books.count > 0
                    }

                    ListView {
                        id: books
                        anchors.fill: parent
                        clip: true
                        model: window.filteredBooks
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {
                            id: libraryScrollbar
                            policy: ScrollBar.AsNeeded
                        }

                        delegate: ItemDelegate {
                            id: bookRow

                            required property var modelData
                            required property int index
                            readonly property bool isItem: Boolean(modelData.itemId)
                            readonly property string syncLabel: {
                                const version = window.wireless.transfer.bookStatusVersion
                                return modelData.trashed ? "" : window.syncStateLabel(window.wireless.transfer.bookStatus(isItem ? modelData.bookId : modelData.id).state)
                            }

                            width: books.width - (libraryScrollbar.size < 1 ? 8 * window.uiScale : 0)
                            hoverEnabled: !window.tabletMode
                            implicitHeight: 68 * window.uiScale
                            leftPadding: 14 * window.uiScale
                            rightPadding: 14 * window.uiScale
                            text: modelData.title || (modelData.type === "chart" ? qsTr("Grafico senza titolo") : qsTr("Tabella senza titolo"))
                            onClicked: {
                                if (modelData.trashed)
                                    organizer.openBookActions(modelData)
                                else if (isItem)
                                    window.openLibraryItem(modelData)
                                else
                                    window.showBook(modelData.id)
                            }

                            background: Rectangle {
                                radius: 12 * window.uiScale
                                color: bookRow.down || bookRow.hovered ? window.accentLight : "transparent"
                                border.color: window.accent
                                border.width: bookRow.visualFocus ? 2 * window.line : 0

                                Rectangle {
                                    visible: bookRow.index < books.count - 1 && !bookRow.down
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 64 * window.uiScale
                                    anchors.rightMargin: 14 * window.uiScale
                                    anchors.bottom: parent.bottom
                                    height: window.line
                                    color: window.border
                                }
                            }

                            contentItem: RowLayout {
                                spacing: 14 * window.uiScale

                                Rectangle {
                                    Layout.preferredWidth: 38 * window.uiScale
                                    Layout.preferredHeight: Layout.preferredWidth
                                    radius: 8 * window.uiScale
                                    color: window.accentLight

                                    PaperIcon {
                                        name: bookRow.isItem ? bookRow.modelData.type : bookRow.modelData.document ? "paper" : "notebook"
                                        tint: window.accent
                                        width: 20 * window.uiScale
                                        height: width
                                        anchors.centerIn: parent
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3 * window.uiScale

                                    InkLabel {
                                        Layout.fillWidth: true
                                        text: bookRow.text
                                        font.pixelSize: 16 * window.uiScale
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }

                                    InkLabel {
                                        text: [bookRow.isItem ? qsTr("%1 · Pagina %2").arg(bookRow.modelData.bookTitle).arg(bookRow.modelData.pageNumber)
                                              : bookRow.modelData.pageCount === 1 ? qsTr("1 pagina") : qsTr("%1 pagine").arg(bookRow.modelData.pageCount),
                                              ...bookRow.modelData.tags, bookRow.syncLabel].filter(entry => entry.length > 0).join(" · ")
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        color: window.muted
                                        font.pixelSize: 13 * window.uiScale
                                    }
                                }

                                PaperIcon {
                                    visible: bookRow.modelData.favorite && !bookRow.modelData.trashed
                                    name: "star-filled"
                                    tint: window.accent
                                    Layout.preferredWidth: 20 * window.uiScale
                                    Layout.preferredHeight: Layout.preferredWidth
                                    Accessible.ignored: true
                                }

                                TopButton {
                                    visible: !bookRow.isItem
                                    text: qsTr("Azioni del quaderno")
                                    iconName: "more"
                                    iconOnly: true
                                    quiet: true
                                    onClicked: organizer.openBookActions(bookRow.modelData)
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(parent.width, 310 * window.uiScale)
                            visible: books.count === 0 && !librarySearch.active
                            spacing: 8 * window.uiScale

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 64 * window.uiScale
                                Layout.preferredHeight: Layout.preferredWidth
                                radius: 14 * window.uiScale
                                color: window.accentLight

                                PaperIcon {
                                    anchors.centerIn: parent
                                    name: window.libraryBooks.length === 0 ? "notebook" : "search"
                                    tint: window.accent
                                    width: 30 * window.uiScale
                                    height: width
                                }
                            }

                            InkLabel {
                                Layout.fillWidth: true
                                Layout.topMargin: 5 * window.uiScale
                                horizontalAlignment: Text.AlignHCenter
                                font.pixelSize: 20 * window.uiScale
                                font.weight: Font.DemiBold
                                text: organizer.viewMode === "trash" && window.libraryBooks.length === 0 ? qsTr("Cestino vuoto")
                                      : organizer.viewMode === "favorites" && window.libraryBooks.length === 0 ? qsTr("Nessun preferito")
                                      : window.libraryBooks.length > 0 ? qsTr("Nessun risultato")
                                      : organizer.section === "books" ? qsTr("Nessun libro")
                                      : organizer.section === "charts" ? qsTr("Nessun grafico")
                                      : organizer.section === "tables" ? qsTr("Nessuna tabella") : qsTr("Nessun quaderno")
                            }

                            InkLabel {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                color: window.muted
                                visible: text.length > 0
                                text: search.text.trim().length > 0 ? qsTr("Prova con un altro nome o tag.") : ""
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            id: pageView
            visible: window.editing
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                visible: !window.fullPage
                Layout.fillWidth: true
                Layout.leftMargin: 8 * window.uiScale
                Layout.rightMargin: Layout.leftMargin
                Layout.topMargin: 8 * window.uiScale
                implicitHeight: 50 * window.uiScale
                radius: 12 * window.uiScale
                color: "white"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 4 * window.uiScale
                    anchors.rightMargin: anchors.leftMargin
                    spacing: 2 * window.uiScale

                    TopButton {
                        visible: !window.notebook.hasPageBackground
                        text: qsTr("Modello")
                        iconName: "paper"
                        quiet: true
                        iconOnly: window.width < 700 * window.uiScale
                        onClicked: {
                            inkPage.cancelDrawing()
                            pageTemplateDialog.open()
                        }
                    }

                    TopButton {
                        text: window.notebook.hasPageBackground ? qsTr("Aggiungi nota") : qsTr("Testo")
                        iconName: "text"
                        quiet: true
                        iconOnly: window.width < 700 * window.uiScale
                        onClicked: window.editPageItem("text", null)
                    }

                    TopButton {
                        text: qsTr("Tabella")
                        iconName: "table"
                        quiet: true
                        iconOnly: window.width < 700 * window.uiScale
                        onClicked: window.editPageItem("table", null)
                    }

                    TopButton {
                        text: qsTr("Diagramma")
                        iconName: "diagram"
                        quiet: true
                        iconOnly: window.width < 760 * window.uiScale
                        ToolTip.visible: !window.tabletMode && hovered && iconOnly
                        ToolTip.text: text
                        onClicked: window.editPageItem("diagram", null)
                    }

                    Item { Layout.fillWidth: true }

                    TopButton {
                        text: qsTr("Lazo")
                        iconName: "lasso"
                        quiet: true
                        iconOnly: window.width < 700 * window.uiScale
                        filled: window.lassoActive
                        enabled: window.notebook.canWriteLayer && window.notebook.erasableCount > 0
                        onClicked: window.setLasso(!window.lassoActive)
                    }

                    TopButton {
                        text: qsTr("Incolla")
                        iconName: "paste"
                        quiet: true
                        iconOnly: window.width < 760 * window.uiScale
                        visible: window.inkClipboard.length > 0
                        enabled: window.notebook.canWriteLayer
                        onClicked: window.openInkTransform(window.inkClipboard, true, [])
                    }

                    TopButton {
                        visible: window.notebook.pageItems.length > 0
                        text: window.editingItems ? qsTr("Scrivi") : qsTr("Elementi")
                        iconName: "rename"
                        quiet: true
                        iconOnly: window.width < 700 * window.uiScale
                        filled: window.editingItems
                        Accessible.name: window.editingItems ? qsTr("Torna alla scrittura") : qsTr("Modifica e sposta elementi")
                        onClicked: {
                            inkPage.cancelDrawing()
                            window.lassoActive = false
                            window.editingItems = !window.editingItems
                            window.selectedPageItem = ""
                        }
                    }
                }

            }

            RowLayout {
                visible: !window.fullPage
                Layout.fillWidth: true
                Layout.leftMargin: window.gutter
                Layout.rightMargin: window.gutter
                Layout.topMargin: 4 * window.uiScale
                spacing: 4 * window.uiScale

                TopButton {
                    visible: window.notebook.hasPageBackground
                    text: qsTr("Evidenzia")
                    iconName: "pen"
                    filled: window.penTool === "highlighter"
                    enabled: window.notebook.canWriteLayer
                    onClicked: window.selectTool("highlighter")
                }

                TopButton {
                    visible: window.notebook.hasPageBackground
                    text: qsTr("Sottolinea")
                    iconName: "underline"
                    filled: window.penTool === "underline"
                    enabled: window.notebook.canWriteLayer
                    onClicked: window.selectTool("underline")
                }

                TopButton {
                    visible: !window.notebook.hasPageBackground && window.notebook.markdownBook
                    text: qsTr("Modifica Markdown")
                    iconName: "text"
                    iconOnly: window.width < 610 * window.uiScale
                    onClicked: markdownEditor.begin(false)
                }

                TopButton {
                    text: qsTr("Inserisci immagine")
                    iconName: "image"
                    iconOnly: window.width < 610 * window.uiScale || window.notebook.hasPageBackground
                    onClicked: {
                        inkPage.cancelDrawing()
                        imagePicker.targetBook = window.notebook.bookId
                        imagePicker.targetPage = window.notebook.pageId
                        imagePicker.open()
                    }
                }

                TopButton {
                    text: qsTr("Inserisci dalla libreria")
                    iconName: "layers"
                    iconOnly: window.width < 610 * window.uiScale || window.notebook.hasPageBackground
                    onClicked: insertLibraryItemDialog.begin()
                }

                Item { Layout.fillWidth: true }
            }

            Rectangle {
                visible: !window.fullPage
                Layout.fillWidth: true
                Layout.leftMargin: 8 * window.uiScale
                Layout.rightMargin: Layout.leftMargin
                Layout.topMargin: 5 * window.uiScale
                implicitHeight: (window.stackedInkToolbar ? 94 : 50) * window.uiScale
                radius: 12 * window.uiScale
                color: "white"

                GridLayout {
                    anchors.fill: parent
                    anchors.margins: 3 * window.uiScale
                    columns: window.stackedInkToolbar ? 4 : 9
                    columnSpacing: 2 * window.uiScale
                    rowSpacing: 0

                    Repeater {
                        model: [
                            { name: qsTr("Nero"), ink: "#222622" },
                            { name: qsTr("Blu"), ink: "#274f7c" },
                            { name: qsTr("Rosso"), ink: "#9c3c38" }
                        ]

                        TopButton {
                            id: swatch
                            required property var modelData

                            text: modelData.name
                            iconOnly: window.width < 680 * window.uiScale
                            checkable: true
                            checked: window.penColor === modelData.ink
                            ButtonGroup.group: penColors
                            leftPadding: (iconOnly ? 16 : 10) * uiScale
                            rightPadding: leftPadding
                            onClicked: {
                                window.penColor = modelData.ink
                                window.editingItems = false
                                window.selectedPageItem = ""
                            }

                            contentItem: Row {
                                spacing: 6 * swatch.uiScale

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 14 * swatch.uiScale
                                    height: width
                                    radius: width / 2
                                    color: swatch.modelData.ink
                                }

                                Text {
                                    visible: !swatch.iconOnly
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: swatch.text
                                    font: swatch.font
                                    color: window.ink
                                }
                            }

                            background: Rectangle {
                                color: swatch.down || swatch.checked ? window.accentLight
                                    : swatch.hovered ? window.surface : "transparent"
                                border.color: swatch.checked || swatch.activeFocus ? window.accent : window.border
                                border.width: swatch.checked || swatch.activeFocus ? 2 * window.line : 0
                                radius: 10 * swatch.uiScale
                            }
                        }
                    }

                    TopButton {
                        text: window.selectedTool.label
                        iconName: window.penTool === "eraser" ? "eraser" : "pen"
                        iconOnly: window.width < 600 * window.uiScale
                        quiet: true
                        filled: window.penTool === "eraser"
                        Accessible.name: qsTr("Strumenti di scrittura, %1").arg(text)
                        onClicked: {
                            inkPage.cancelDrawing()
                            penTools.open()
                        }
                    }

                    Item { visible: !window.stackedInkToolbar; Layout.fillWidth: true }

                    TopButton {
                        text: qsTr("Pagina intera")
                        iconName: "expand"
                        iconOnly: true
                        quiet: true
                        onClicked: {
                            inkPage.cancelDrawing()
                            window.editingItems = false
                            window.lassoActive = false
                            window.fullPage = true
                        }
                    }

                    TopButton {
                        text: qsTr("Livelli")
                        iconName: "layers"
                        iconOnly: true
                        quiet: true
                        filled: !window.notebook.canWriteLayer && window.editing
                        onClicked: {
                            inkPage.cancelDrawing()
                            pageLayers.open()
                        }
                    }

                    TopButton {
                        text: qsTr("Annulla")
                        iconName: "undo"
                        iconOnly: true
                        quiet: true
                        enabled: !window.editingItems && window.notebook.canUndo
                        onClicked: window.notebook.undo()
                    }

                    TopButton {
                        text: qsTr("Ripeti")
                        iconName: "redo"
                        iconOnly: true
                        quiet: true
                        enabled: !window.editingItems && window.notebook.canRedo
                        onClicked: window.notebook.redo()
                    }
                }
            }

            InkLabel {
                visible: window.editingItems
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4 * window.uiScale
                color: window.accent
                text: qsTr("Tocca per modificare, trascina per spostare.")
                font.pixelSize: 13 * window.uiScale
            }

            InkLabel {
                visible: window.lassoActive
                Layout.fillWidth: true
                Layout.leftMargin: window.gutter
                Layout.rightMargin: window.gutter
                Layout.topMargin: 4 * window.uiScale
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                color: window.accent
                text: window.selectionMessage || qsTr("Disegna un contorno attorno ai tratti del livello attivo.")
                font.pixelSize: 13 * window.uiScale
            }

            InkLabel {
                visible: !window.notebook.canWriteLayer && window.editing && !window.editingItems
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4 * window.uiScale
                color: window.accent
                text: qsTr("Livello nascosto. Mostralo per scrivere.")
                font.pixelSize: 13 * window.uiScale
            }

            ReadingControls {
                visible: !window.fullPage && window.notebook.hasPageBackground
                tabletMode: window.tabletMode
                Layout.fillWidth: true
                Layout.leftMargin: window.gutter
                Layout.rightMargin: window.gutter
                uiScale: window.uiScale
                bookmark: window.notebook.pageBookmarked
                zoom: window.readingZoom
                fitWidth: window.readingFitWidth
                onToggleBookmark: window.notebook.togglePageBookmark()
                onBookmarksRequested: {
                    bookmarksDialog.pages = window.notebook.pageBookmarkEntries()
                    bookmarksDialog.open()
                }
                onZoomRequested: (zoom, fitWidth) => window.setReadingZoom(zoom, fitWidth)
            }

            Flickable {
                id: paperViewport
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 8 * window.uiScale
                Layout.rightMargin: Layout.leftMargin
                Layout.topMargin: 8 * window.uiScale
                Layout.bottomMargin: Layout.topMargin
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: Math.max(width, sheet.width * sheet.scale)
                contentHeight: Math.max(height, sheet.height * sheet.scale)
                interactive: window.notebook.hasPageBackground && (window.readingZoom > 1 || window.readingFitWidth)
                ScrollBar.vertical: ScrollBar { policy: paperViewport.interactive ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }
                ScrollBar.horizontal: ScrollBar { policy: paperViewport.interactive ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

                Rectangle {
                    id: sheet
                    x: (paperViewport.contentWidth - width * scale) / 2
                    y: (paperViewport.contentHeight - height * scale) / 2
                    width: window.notebook.hasPageBackground && window.readingFitWidth ? paperViewport.width
                        : Math.min(paperViewport.width, paperViewport.height * 3 / 4)
                    height: width * 4 / 3
                    scale: window.notebook.hasPageBackground ? window.readingZoom : 1
                    transformOrigin: Item.TopLeft
                    color: "white"
                    border.color: window.border
                    border.width: window.line

                    PageTemplateItem {
                        visible: !window.notebook.hasPageBackground
                        anchors.fill: parent
                        anchors.margins: window.line
                        paper: window.notebook.pageTemplate
                    }

                    PagePreviewItem {
                        visible: window.notebook.hasPageBackground
                        anchors.fill: parent
                        anchors.margins: window.line
                        content: ({ background: window.notebook.pageBackground })
                        thumbnail: false
                    }

                    PageDecorations {
                        anchors.fill: parent
                        anchors.margins: window.line
                        z: window.editingItems ? 2 : 0
                        uiScale: window.uiScale
                        notebook: window.notebook
                        items: window.notebook.pageItems
                        editingItems: window.editingItems && !window.editorPopupOpen
                        selectedId: window.selectedPageItem
                        onEditRequested: item => window.editPageItem(item.type, item)
                        onMoveRequested: (id, x, y) => {
                            if (window.notebook.movePageItem(id, x, y))
                                window.selectedPageItem = id
                        }
                    }

                    TapHandler {
                        enabled: window.editing && !window.showingSettings && !window.editorPopupOpen
                            && !window.pinLock.locked && !window.editingItems && !window.lassoActive
                            && !inkPage.drawing && ["pen", "pencil", "marker"].includes(window.penTool)
                        acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
                        onTapped: eventPoint => {
                            const id = inkPage.textItemAt(inkPage.mapFromItem(null, eventPoint.scenePosition))
                            if (id.length > 0)
                                window.editPageText(id)
                        }
                    }

                    InkItem {
                        id: inkPage
                        anchors.fill: parent
                        anchors.margins: window.line
                        z: 1
                        enabled: window.editing && !window.editorPopupOpen && !window.editingItems
                                 && !window.pinLock.locked && window.notebook.canWriteLayer
                        acceptsMouse: !window.tabletMode
                        inkColor: window.penTool === "highlighter" ? "#f0bf32" : window.penColor
                        inkTool: window.penTool
                        smartShapes: window.smartShapesEnabled
                        lassoMode: window.lassoActive
                        inkWidth: window.selectedTool.width * [0.62, 1, 1.60][window.penThickness]
                        erasableFrom: window.notebook.erasableFrom
                        erasableCount: window.notebook.erasableCount
                        strokes: window.notebook.strokes
                        pageItems: window.notebook.pageItems

                        property string pageKey: window.notebook.bookId + "/" + window.notebook.pageId
                        property string zoomBookId: ""
                        onPageKeyChanged: {
                            cancelDrawing()
                            if (selectionDialog.visible)
                                selectionDialog.close()
                            if (pageItemDialog.visible && pageItemDialog.transcript)
                                pageItemDialog.close()
                            window.editingItems = false
                            window.lassoActive = false
                            window.selectionMessage = ""
                            window.selectedPageItem = ""
                            if (zoomBookId !== window.notebook.bookId) {
                                zoomBookId = window.notebook.bookId
                                window.readingZoom = 1
                                window.readingFitWidth = false
                            }
                            paperViewport.contentX = 0
                            paperViewport.contentY = 0
                        }
                        onStrokeStarted: window.pinLock.recordActivity()
                        onTextItemTapped: id => window.editPageText(id)
                        onStrokeCompleted: stroke => {
                            window.pinLock.recordActivity()
                            window.notebook.appendStroke(stroke)
                        }
                        onCorrectedStrokeCompleted: (shape, original) => {
                            window.pinLock.recordActivity()
                            window.notebook.appendCorrectedStroke(shape, original)
                        }
                        onLassoCompleted: polygon => {
                            window.pinLock.recordActivity()
                            window.useLasso(polygon)
                        }
                        onStrokesErased: indexes => {
                            window.pinLock.recordActivity()
                            window.notebook.eraseStrokes(indexes)
                        }
                    }
                }
            }

            InkLabel {
                visible: window.tabletMode && !inkPage.penAvailable
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 6 * window.uiScale
                text: qsTr("Marker non rilevato")
                color: window.muted
                font.pixelSize: 13 * window.uiScale
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 8 * window.uiScale
                Layout.rightMargin: Layout.leftMargin
                Layout.bottomMargin: 8 * window.uiScale
                implicitHeight: 50 * window.uiScale
                color: "white"
                radius: 12 * window.uiScale

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 5 * window.uiScale
                    anchors.rightMargin: anchors.leftMargin
                    spacing: 3 * window.uiScale

                    TopButton {
                        text: qsTr("Pagina precedente")
                        iconName: "left"
                        iconOnly: true
                        quiet: true
                        enabled: window.notebook.page > 0
                        onClicked: window.notebook.goToPage(window.notebook.page - 1)
                    }

                    TopButton {
                        text: qsTr("%1 / %2").arg(window.notebook.page + 1).arg(window.notebook.pageCount)
                        Accessible.name: qsTr("Panoramica, pagina %1 di %2").arg(window.notebook.page + 1).arg(window.notebook.pageCount)
                        quiet: true
                        onClicked: {
                            inkPage.cancelDrawing()
                            pageOverview.open()
                        }
                    }

                    TopButton {
                        text: qsTr("Pagina successiva")
                        iconName: "right"
                        iconOnly: true
                        quiet: true
                        enabled: window.notebook.page + 1 < window.notebook.pageCount
                        onClicked: window.notebook.goToPage(window.notebook.page + 1)
                    }

                    Item { Layout.fillWidth: true }

                    TopButton {
                        visible: window.epaperRefresh.available
                        enabled: window.canRefreshPage
                        text: qsTr("Aggiorna schermo")
                        iconName: "sync"
                        iconOnly: window.width < 520 * window.uiScale
                        quiet: true
                        ToolTip.visible: !window.tabletMode && hovered && iconOnly
                        ToolTip.text: text
                        onClicked: window.refreshPage()
                    }

                    TopButton {
                        visible: !window.fullPage
                        text: qsTr("Cerca testo")
                        iconName: "search"
                        iconOnly: true
                        quiet: true
                        onClicked: window.openPageSearch()
                    }

                    TopButton {
                        visible: !window.fullPage
                        text: qsTr("Nuova pagina")
                        iconName: "plus"
                        iconOnly: window.epaperRefresh.available && window.width < 520 * window.uiScale
                        ToolTip.visible: !window.tabletMode && hovered && iconOnly
                        ToolTip.text: text
                        quiet: true
                        onClicked: window.notebook.addPage()
                    }

                    TopButton {
                        visible: window.fullPage
                        text: qsTr("Mostra strumenti")
                        iconName: "collapse"
                        onClicked: {
                            inkPage.cancelDrawing()
                            window.fullPage = false
                        }
                    }
                }
            }
        }

        InkLabel {
            visible: window.documentExchange.status.length > 0 && !window.showingSettings
            Layout.fillWidth: true
            Layout.leftMargin: window.gutter
            Layout.rightMargin: window.gutter
            text: window.documentExchange.status
            color: window.muted
            wrapMode: Text.Wrap
        }

        InkLabel {
            visible: text.length > 0 && !nameDialog.visible && !window.showingSettings
            Layout.fillWidth: true
            Layout.margins: 12 * window.uiScale
            text: window.notebook.error
            color: "#8c2d27"
            wrapMode: Text.Wrap
        }
    }

    DocumentDropArea {
        anchors.fill: parent
        z: 1
        exchange: window.documentExchange
        folderId: organizer.destinationFolderId
        enabled: window.documentExchange.available && !window.pinLock.locked && !window.documentExchange.importPaused
        uiScale: window.uiScale
    }

    PinScreen {
        anchors.fill: parent
        z: 1
        showTouchKeyboard: !Boolean(window.externalKeyboard?.connected)
        pinLock: window.pinLock
        uiScale: window.uiScale
        showExit: window.tabletMode
        onExitRequested: Qt.quit()
    }

    AppleShareView {
        id: appleShareView
        notebook: window.notebook
        appleShare: window.appleShare
        documentExchange: window.documentExchange
        uiScale: window.uiScale
        tabletMode: window.tabletMode
    }

    MarkdownDialog {
        id: markdownDialog
        showTouchKeyboard: window.showTouchKeyboard
        notebook: window.notebook
        obsidian: window.obsidian
        web: window.markdownWeb
        uiScale: window.uiScale
        tabletMode: window.tabletMode
        cloudAvailable: window.obsidianSync.available
        onCloudRequested: {
            markdownDialog.close()
            obsidianSyncDialog.begin(organizer.destinationFolderId)
        }
        onExportRequested: {
            markdownSavePicker.bookId = window.notebook.bookId
            markdownSavePicker.selectedFile = window.notebook.suggestedMarkdownName()
            markdownSavePicker.open()
        }
    }

    ObsidianSyncDialog {
        id: obsidianSyncDialog
        dim: !window.tabletMode
        sync: window.obsidianSync
        uiScale: window.uiScale
    }

    Connections {
        target: window.obsidianSync
        function onOpened(bookId) {
            obsidianSyncDialog.close()
            window.showBook(bookId)
        }
    }

    Connections {
        target: window.markdownWeb
        function onImported(bookId) {
            markdownDialog.close()
            window.showBook(bookId)
        }
    }

    PageTemplateDialog {
        id: pageTemplateDialog
        dim: !window.tabletMode
        uiScale: window.uiScale
        currentTemplate: window.notebook.pageTemplate
        onTemplateChosen: paper => {
            if (window.notebook.setPageTemplate(paper))
                pageTemplateDialog.close()
            else
                pageTemplateDialog.errorText = window.notebook.error
        }
    }

    PageOverview {
        id: pageOverview
        tabletMode: window.tabletMode
        notebook: window.notebook
        uiScale: window.uiScale
        onEditPageTags: index => {
            if (window.notebook.goToPage(index)) {
                pageOverview.close()
                pageTags.open()
            }
        }
    }

    PageLayers {
        id: pageLayers
        showTouchKeyboard: window.showTouchKeyboard
        notebook: window.notebook
        uiScale: window.uiScale
        tabletMode: window.tabletMode
    }

    PageTags {
        id: pageTags
        showTouchKeyboard: window.showTouchKeyboard
        notebook: window.notebook
        uiScale: window.uiScale
        tabletMode: window.tabletMode
    }

    InkSelection {
        id: inkTransform
        tabletMode: window.tabletMode
        uiScale: window.uiScale
        onTransformAccepted: (pivotX, pivotY, dx, dy, scale, angle) => {
            const success = pasting ? window.notebook.pasteInk(selectedStrokes, pivotX, pivotY, dx, dy, scale, angle)
                : window.notebook.transformSelectedInk(selectedIndexes, pivotX, pivotY, dx, dy, scale, angle)
            if (success)
                close()
            else
                errorText = window.notebook.error
        }
    }

    UsbTransferDialog {
        id: usbTransferDialog
        dim: !window.tabletMode
        notebook: window.notebook
        transfer: usbTransfer
        uiScale: window.uiScale
    }

    UsbImportDialog {
        id: usbImportDialog
        dim: !window.tabletMode
        notebook: window.notebook
        usbSsh: window.usbSsh
        uiScale: window.uiScale
    }

    TableChartDialog {
        id: tableChartDialog
        showTouchKeyboard: window.showTouchKeyboard
        notebook: window.notebook
        uiScale: window.uiScale
        tabletMode: window.tabletMode
        onDeleteRequested: id => {
            if (window.notebook.removePageItem(id)) {
                window.selectedPageItem = ""
                tableChartDialog.close()
            } else {
                tableChartDialog.errorText = window.notebook.error
            }
        }
    }

    Dialog {
        id: bookmarksDialog
        property var pages: []
        title: qsTr("Segnalibri")
        anchors.centerIn: Overlay.overlay
        width: Math.min(410 * window.uiScale, window.width - 24 * window.uiScale)
        modal: true
        dim: !window.tabletMode
        padding: 16 * window.uiScale

        contentItem: ColumnLayout {
            InkLabel {
                visible: bookmarksDialog.pages.length === 0
                text: qsTr("Nessun segnalibro in questo documento.")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            ListView {
                id: bookmarksList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 240 * window.uiScale)
                model: bookmarksDialog.pages
                clip: true
                delegate: TopButton {
                    required property var modelData
                    width: bookmarksList.width
                    text: qsTr("Pagina %1").arg(modelData.page + 1)
                    onClicked: if (window.notebook.openPage(modelData.pageId)) bookmarksDialog.close()
                }
            }
            TopButton {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Chiudi")
                onClicked: bookmarksDialog.close()
            }
        }
    }

    Dialog {
        id: selectionDialog
        property var selection: null
        readonly property bool hasHandwriting: !!selection && selection.strokes.some(stroke =>
            !stroke.tool || stroke.tool === "pen" || stroke.tool === "pencil" || stroke.tool === "marker")
        readonly property bool actionsEnabled: !!selection && (window.tabletMode || !handwriting.busy)
        readonly property bool hasPreview: window.tabletMode && handwriting.result.length > 0
        title: qsTr("Tratti selezionati")
        anchors.centerIn: Overlay.overlay
        width: Math.min(410 * window.uiScale, window.width - 24 * window.uiScale)
        padding: 16 * window.uiScale
        modal: true
        dim: !window.tabletMode
        onAboutToHide: {
            selection = null
            handwriting.cancel()
        }

        function reviewTranscription() {
            if (!visible || !selection || window.pinLock.locked || handwriting.result.length === 0)
                return
            const transcript = handwriting.result
            const selectedInk = selection
            close()
            pageItemDialog.openFor("text", null, transcript, selectedInk)
        }

        contentItem: ColumnLayout {
            spacing: 8 * window.uiScale

            InkLabel {
                Layout.fillWidth: true
                text: qsTr("Tratti selezionati: %1").arg(selectionDialog.selection ? selectionDialog.selection.strokes.length : 0)
            }

            InkLabel {
                Layout.fillWidth: true
                visible: window.tabletMode && handwriting.busy
                text: qsTr("Trascrizione in corso…")
                color: window.muted
            }

            InkLabel {
                Layout.fillWidth: true
                visible: selectionDialog.hasPreview
                text: handwriting.result
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }

            TopButton {
                Layout.fillWidth: true
                alignLeft: true
                visible: selectionDialog.hasPreview
                text: qsTr("Rivedi la trascrizione")
                iconName: "text"
                filled: true
                onClicked: selectionDialog.reviewTranscription()
            }

            RowLayout {
                Layout.fillWidth: true
                TopButton {
                    text: qsTr("Sposta e trasforma")
                    enabled: selectionDialog.actionsEnabled
                    onClicked: {
                        const selection = selectionDialog.selection
                        selectionDialog.close()
                        window.openInkTransform(selection.strokes, false, selection.indexes)
                    }
                }
                TopButton {
                    text: qsTr("Copia tratti")
                    enabled: selectionDialog.actionsEnabled
                    onClicked: {
                        window.inkClipboard = selectionDialog.selection.strokes
                        selectionDialog.close()
                    }
                }
            }

            TopButton {
                Layout.fillWidth: true
                alignLeft: true
                text: qsTr("Converti in grafico")
                iconName: "chart"
                enabled: selectionDialog.actionsEnabled
                onClicked: {
                    const strokes = selectionDialog.selection.strokes
                    selectionDialog.close()
                    chartDialog.openFor(strokes)
                }
            }

            TopButton {
                Layout.fillWidth: true
                alignLeft: true
                text: handwriting.busy ? qsTr("Trascrizione in corso…")
                      : window.tabletMode ? qsTr("Riprova la trascrizione") : qsTr("Trascrivi in italiano")
                iconName: "text"
                visible: handwriting.available && (!window.tabletMode || handwriting.error.length > 0)
                enabled: !handwriting.busy && !!selectionDialog.selection
                onClicked: handwriting.recognize(selectionDialog.selection.strokes)
            }

            InkLabel {
                Layout.fillWidth: true
                visible: (!window.tabletMode || selectionDialog.hasHandwriting)
                         && (!handwriting.available || handwriting.error.length > 0)
                wrapMode: Text.WordWrap
                color: handwriting.error.length > 0 ? "#8c2d27" : window.muted
                text: handwriting.available ? handwriting.error
                      : window.tabletMode ? qsTr("Il motore di trascrizione non è installato sul Paper Pro.")
                      : qsTr("Trascrizione disponibile su Mac con macOS 27 o successivo.")
                font.pixelSize: 13 * window.uiScale
            }

            TopButton {
                Layout.fillWidth: true
                alignLeft: true
                text: qsTr("Ripristina tratti originali")
                iconName: "undo"
                visible: !!selectionDialog.selection && selectionDialog.selection.reversible > 0
                enabled: selectionDialog.actionsEnabled
                onClicked: {
                    if (window.notebook.restoreSelectedInk(selectionDialog.selection.indexes))
                        selectionDialog.close()
                }
            }

            TopButton {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Chiudi")
                quiet: true
                onClicked: selectionDialog.close()
            }
        }
    }

    ChartDialog {
        id: chartDialog
        showTouchKeyboard: window.showTouchKeyboard
        uiScale: window.uiScale
        tabletMode: window.tabletMode
        onExportCsvRequested: csv => {
            csvSavePicker.csvData = csv
            csvSavePicker.open()
        }
        onSaveRequested: item => {
            if (window.saveEditedItem(item))
                chartDialog.close()
            else
                chartDialog.errorText = window.notebook.error
        }
        onSaveOnNewPageRequested: item => {
            if (window.notebook.savePageItemOnNewPage(item))
                chartDialog.close()
            else
                chartDialog.errorText = window.notebook.error
        }
        onClosed: if (window.newLibraryItemBook && window.newLibraryItemBook.type === "chart")
            window.newLibraryItemBook = null
        onDeleteRequested: id => {
            if (window.notebook.removePageItem(id)) {
                window.selectedPageItem = ""
                if (window.notebook.pageItems.length === 0)
                    window.editingItems = false
                chartDialog.close()
            } else {
                chartDialog.errorText = window.notebook.error
            }
        }
    }

    DiagramExport { id: diagramExport }

    DiagramEditor {
        id: diagramEditor
        showTouchKeyboard: window.showTouchKeyboard
        uiScale: window.uiScale
        tabletMode: window.tabletMode
        onSaved: item => {
            if (window.notebook.savePageItem(item))
                diagramEditor.close()
            else
                diagramEditor.errorText = window.notebook.error
        }
        onSaveOnNewPageRequested: item => {
            if (window.notebook.savePageItemOnNewPage(item))
                diagramEditor.close()
            else
                diagramEditor.errorText = window.notebook.error
        }
        onDeleteRequested: id => {
            if (window.notebook.removePageItem(id)) {
                window.selectedPageItem = ""
                if (window.notebook.pageItems.length === 0)
                    window.editingItems = false
                diagramEditor.close()
            } else {
                diagramEditor.errorText = window.notebook.error
            }
        }
        onExportRequested: item => {
            diagramSavePicker.draft = item
            diagramSavePicker.selectedFile = diagramExport.suggestedFile(window.notebook.title)
            diagramSavePicker.open()
        }
        onImportRequested: diagramOpenPicker.open()
    }

    FileDialog {
        id: diagramOpenPicker
        title: qsTr("Importa diagramma draw.io")
        nameFilters: [qsTr("Diagramma draw.io (*.drawio)"), qsTr("Diagramma XML (*.xml)")]
        onAccepted: {
            if (diagramEditor.visible && !window.pinLock.locked)
                diagramEditor.importDiagram(diagramExport.load(selectedFile))
        }
    }

    FileDialog {
        id: diagramSavePicker
        property var draft: null
        title: qsTr("Esporta diagramma draw.io")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "drawio"
        nameFilters: [qsTr("Diagramma draw.io (*.drawio)")]
        onAccepted: {
            if (draft && !window.pinLock.locked)
                diagramEditor.errorText = diagramExport.save(selectedFile, draft, window.notebook.title)
            draft = null
        }
        onRejected: draft = null
    }

    FileDialog {
        id: documentPicker
        title: qsTr("Importa documento")
        onAccepted: {
            if (!window.pinLock.locked && window.notebook.importDocument(selectedFile, organizer.destinationFolderId)) {
                organizer.section = window.notebook.hasPageBackground ? "books" : "writing"
                window.showCurrentBook()
            }
        }
    }

    Dialog {
        id: appleBooksDialog
        property var pendingFolder: null

        title: qsTr("Importa da Apple Books")
        anchors.centerIn: Overlay.overlay
        width: Math.min(470 * window.uiScale, window.width - 24 * window.uiScale)
        padding: 16 * window.uiScale
        modal: true
        dim: !window.tabletMode
        standardButtons: Dialog.Close
        onClosed: {
            const folder = pendingFolder
            pendingFolder = null
            if (folder !== null && !window.pinLock.locked)
                Qt.callLater(() => window.documentExchange.chooseAppleBooksFiles(folder))
        }

        contentItem: ColumnLayout {
            spacing: 10 * window.uiScale

            InkLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Scegli una copia PDF o EPUB senza protezione DRM. Dopo l’importazione, apri Sincronizzazione wireless per inviare il libro al Paper Pro.")
            }

            InkLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: window.muted
                text: qsTr("Per salvare una copia di un PDF aperto in Anteprima, usa File → Esporta.")
            }

            TopButton {
                Layout.fillWidth: true
                text: qsTr("Apri Apple Books")
                iconName: "notebook"
                onClicked: window.documentExchange.openAppleBooks()
            }

            TopButton {
                Layout.fillWidth: true
                text: qsTr("Scegli PDF o EPUB…")
                iconName: "download"
                filled: true
                onClicked: {
                    appleBooksDialog.pendingFolder = organizer.destinationFolderId
                    appleBooksDialog.close()
                }
            }
        }
    }

    FileDialog {
        id: pdfSavePicker
        title: qsTr("Salva PDF")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pdf"
        nameFilters: [qsTr("PDF (*.pdf)")]
        onAccepted: window.notebook.exportDocument(selectedFile)
    }

    FileDialog {
        id: markdownSavePicker
        property string bookId: ""
        title: qsTr("Salva Markdown")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "md"
        nameFilters: [qsTr("Markdown (*.md *.markdown *.MD *.MARKDOWN)")]
        onAccepted: {
            if (window.notebook.bookId === bookId && !window.pinLock.locked) {
                if (window.notebook.exportMarkdownWithAssets(selectedFile))
                    markdownDialog.close()
                else
                    markdownDialog.exportError = window.notebook.error
            }
            bookId = ""
        }
        onRejected: bookId = ""
    }

    FileDialog {
        id: csvOpenPicker
        title: qsTr("Importa dati CSV")
        nameFilters: [qsTr("CSV e TSV (*.csv *.CSV *.tsv *.TSV)")]
        Component.onCompleted: {
            if (window.epaperGeometry.width > 0)
                currentFolder = "file:///home/root"
        }
        onAccepted: {
            if (pageItemDialog.visible && pageItemDialog.editingTable && !pageItemDialog.existing && !window.pinLock.locked)
                pageItemDialog.importCsv(window.notebook.readTableCsv(selectedFile))
        }
    }

    FileDialog {
        id: imagePicker
        property string targetBook: ""
        property string targetPage: ""
        title: qsTr("Inserisci immagine")
        nameFilters: [qsTr("Immagini (*.png *.PNG *.jpg *.JPG *.jpeg *.JPEG)")]
        Component.onCompleted: if (window.epaperGeometry.width > 0) currentFolder = "file:///home/root"
        onAccepted: if (window.editing && !window.pinLock.locked)
            window.notebook.importPageImage(selectedFile, targetBook, targetPage)
    }

    FileDialog {
        id: tableCsvSavePicker
        property var draft: null
        title: qsTr("Esporta tabella CSV")
        nameFilters: [qsTr("CSV (*.csv)")]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        Component.onCompleted: if (window.epaperGeometry.width > 0) currentFolder = "file:///home/root"
        onAccepted: {
            if (draft && !window.pinLock.locked && !window.notebook.exportTableCsv(selectedFile, draft))
                pageItemDialog.errorText = window.notebook.error
            draft = null
        }
        onRejected: draft = null
    }

    Dialog {
        id: imageProperties
        property var item: null
        title: qsTr("Proprietà immagine")
        anchors.centerIn: Overlay.overlay
        width: Math.min(430 * window.uiScale, window.width - 24 * window.uiScale)
        padding: 16 * window.uiScale
        modal: true
        dim: !window.tabletMode
        standardButtons: Dialog.Close
        onClosed: item = null

        contentItem: ColumnLayout {
            spacing: 8 * window.uiScale

            InkLabel {
                text: qsTr("Larghezza %1% della pagina").arg(imageProperties.item ? Math.round(imageProperties.item.width * 100) : 0)
            }

            RowLayout {
                id: imageActions
                function resize(factor: real) {
                    const item = imageProperties.item
                    if (!item)
                        return
                    const scale = Math.min(factor, 0.96 / item.width, 0.96 / item.height)
                    const width = item.width * scale
                    const height = item.height * scale
                    const updated = Object.assign({}, item, { width, height,
                        x: Math.min(item.x, 1 - width), y: Math.min(item.y, 1 - height) })
                    if (window.notebook.savePageItem(updated))
                        imageProperties.item = updated
                }

                TopButton {
                    text: qsTr("Riduci")
                    enabled: imageProperties.item && imageProperties.item.width > 0.12 && imageProperties.item.height > 0.08
                    onClicked: imageActions.resize(0.85)
                }
                TopButton {
                    text: qsTr("Ingrandisci")
                    enabled: imageProperties.item && imageProperties.item.width < 0.96 && imageProperties.item.height < 0.96
                    onClicked: imageActions.resize(1.18)
                }
                TopButton {
                    text: qsTr("Elimina")
                    enabled: imageProperties.item !== null
                    onClicked: if (window.notebook.removePageItem(imageProperties.item.id)) imageProperties.close()
                }
            }
        }
    }

    Dialog {
        id: insertLibraryItemDialog
        property string kind: "chart"
        property string errorText: ""
        readonly property var entries: window.notebook.libraryItems.filter(item => item.type === kind)
        title: qsTr("Inserisci dalla libreria")
        anchors.centerIn: Overlay.overlay
        width: Math.min(490 * window.uiScale, window.width - 24 * window.uiScale)
        padding: 16 * window.uiScale
        modal: true
        dim: !window.tabletMode
        standardButtons: Dialog.Cancel

        function begin() {
            inkPage.cancelDrawing()
            errorText = ""
            kind = "chart"
            open()
        }

        function insertCopy(entry: var) {
            const item = window.notebook.libraryItemCopy(entry.bookId, entry.pageId, entry.itemId)
            if (!item.type) {
                errorText = qsTr("L’elemento non può essere copiato. Controlla i dati originali.")
                return
            }
            if (window.notebook.savePageItem(item))
                close()
            else
                errorText = window.notebook.error
        }

        contentItem: ColumnLayout {
            spacing: 8 * window.uiScale

            RowLayout {
                TopButton { text: qsTr("Grafici"); filled: insertLibraryItemDialog.kind === "chart"; onClicked: insertLibraryItemDialog.kind = "chart" }
                TopButton { text: qsTr("Tabelle"); filled: insertLibraryItemDialog.kind === "table"; onClicked: insertLibraryItemDialog.kind = "table" }
            }

            InkLabel {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Scegli un elemento per inserirne una copia modificabile nella pagina.")
            }

            ListView {
                id: itemCopies
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(count, 5) * 44 * window.uiScale
                model: insertLibraryItemDialog.entries
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: TopButton {
                    required property var modelData
                    width: itemCopies.width
                    text: modelData.title ? modelData.title + " · " + modelData.bookTitle : modelData.bookTitle
                    alignLeft: true
                    quiet: true
                    onClicked: insertLibraryItemDialog.insertCopy(modelData)
                }
            }

            InkLabel {
                Layout.fillWidth: true
                visible: itemCopies.count === 0 || insertLibraryItemDialog.errorText.length > 0
                text: insertLibraryItemDialog.errorText || qsTr("Nessun elemento disponibile")
                wrapMode: Text.WordWrap
            }
        }
    }

    Dialog {
        id: markdownEditor
        property bool creating: false
        property string sourceBook: ""
        property string folder: ""
        property Item keyboardTarget: null
        readonly property bool keyboardOpen: window.showTouchKeyboard && keyboardTarget !== null

        title: creating ? qsTr("Nuovo Markdown") : qsTr("Modifica Markdown")
        anchors.centerIn: Overlay.overlay
        width: Math.min(630 * window.uiScale, window.width - 24 * window.uiScale)
        padding: 16 * window.uiScale
        modal: true
        dim: !window.tabletMode
        focus: true
        closePolicy: Popup.CloseOnEscape

        function begin(create: bool) {
            inkPage.cancelDrawing()
            creating = create
            sourceBook = create ? "" : window.notebook.bookId
            folder = organizer.destinationFolderId
            markdownName.text = ""
            markdownSource.text = create ? "" : window.notebook.markdownSource()
            open()
        }

        onOpened: creating ? markdownName.forceActiveFocus() : markdownSource.forceActiveFocus()
        onClosed: {
            keyboardTarget = null
            markdownName.clear()
            markdownSource.clear()
            sourceBook = ""
        }

        contentItem: ColumnLayout {
            spacing: 8 * window.uiScale

            TextField {
                id: markdownName
                visible: markdownEditor.creating
                Layout.fillWidth: true
                Layout.preferredHeight: 44 * window.uiScale
                maximumLength: 80
                placeholderText: qsTr("Titolo del documento")
                Accessible.name: placeholderText
                onActiveFocusChanged: if (activeFocus && window.tabletMode) markdownEditor.keyboardTarget = markdownName
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(275 * window.uiScale,
                    Math.max(100 * window.uiScale, window.height - (markdownEditor.keyboardOpen ? 425 : 215) * window.uiScale))
                clip: true

                TextArea {
                    id: markdownSource
                    font.pixelSize: 16 * window.uiScale
                    placeholderText: qsTr("Scrivi in Markdown")
                    Accessible.name: placeholderText
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.Wrap
                    selectByMouse: !window.tabletMode
                    onActiveFocusChanged: if (activeFocus && window.tabletMode) markdownEditor.keyboardTarget = markdownSource
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 5 * window.uiScale

                TopButton {
                    text: qsTr("A capo")
                    visible: markdownEditor.keyboardOpen && markdownEditor.keyboardTarget === markdownSource
                    focusPolicy: Qt.NoFocus
                    onClicked: markdownKeyboard.typeKey("\n")
                }

                Item { Layout.fillWidth: true }

                TopButton {
                    visible: markdownEditor.keyboardOpen
                    text: qsTr("Chiudi tastiera")
                    quiet: true
                    onClicked: markdownEditor.keyboardTarget = null
                }
            }

            TouchKeyboard {
                id: markdownKeyboard
                visible: markdownEditor.keyboardOpen
                Layout.fillWidth: true
                uiScale: Math.max(0.75, window.uiScale * 0.78)
                target: markdownEditor.keyboardTarget
            }

            InkLabel {
                Layout.fillWidth: true
                visible: text.length > 0
                text: window.notebook.error
                color: "#8c2d27"
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                TopButton {
                    text: qsTr("Annulla", "Cancel a dialog")
                    onClicked: markdownEditor.close()
                }

                TopButton {
                    text: qsTr("Salva")
                    filled: true
                    enabled: !markdownEditor.creating || markdownName.text.trim().length > 0
                    onClicked: {
                        if (window.pinLock.locked)
                            return
                        const saved = markdownEditor.creating
                            ? window.notebook.importMarkdownText(markdownSource.text, markdownName.text.trim(), markdownEditor.folder)
                            : markdownEditor.sourceBook === window.notebook.bookId && window.notebook.updateMarkdownText(markdownSource.text)
                        if (saved) {
                            markdownEditor.close()
                            window.showCurrentBook()
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: csvSavePicker
        property string csvData: ""
        title: qsTr("Salva dati del grafico")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: [qsTr("CSV (*.csv)")]
        onAccepted: {
            if (!window.notebook.exportChartCsv(selectedFile, csvData))
                chartDialog.errorText = window.notebook.error
            csvData = ""
        }
        onRejected: csvData = ""
    }

    Dialog {
        id: penTools
        title: qsTr("Strumenti di scrittura")
        anchors.centerIn: Overlay.overlay
        width: Math.min(440 * window.uiScale, window.width - 24 * window.uiScale)
        padding: 16 * window.uiScale
        modal: true
        dim: !window.tabletMode
        contentItem: ColumnLayout {
            spacing: 9 * window.uiScale

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: 5 * window.uiScale
                columnSpacing: 5 * window.uiScale

                Repeater {
                    model: window.penToolsModel
                    TopButton {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        text: modelData.label
                        filled: window.penTool === modelData.id
                        onClicked: window.selectTool(modelData.id)
                    }
                }
            }

            InkLabel {
                text: window.penTool === "eraser" ? qsTr("Ampiezza della gomma") : qsTr("Spessore del tratto")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 5 * window.uiScale

                Repeater {
                    model: [qsTr("Sottile"), qsTr("Medio"), qsTr("Spesso")]
                    TopButton {
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        text: modelData
                        filled: window.penThickness === index
                        onClicked: window.penThickness = index
                    }
                }
            }

            Switch {
                Layout.fillWidth: true
                text: qsTr("Forme intelligenti")
                checked: window.smartShapesEnabled
                onToggled: window.smartShapesEnabled = checked
            }

            InkLabel {
                Layout.fillWidth: true
                visible: window.smartShapesEnabled
                text: qsTr("Disegna con penna, matita o pennarello e tieni ferma la punta alla fine.")
                wrapMode: Text.WordWrap
                color: window.muted
                font.pixelSize: 13 * window.uiScale
            }

            TopButton {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Fine")
                quiet: true
                onClicked: penTools.close()
            }
        }
    }

    Dialog {
        id: pageSearch
        title: qsTr("Cerca nel quaderno")
        anchors.centerIn: Overlay.overlay
        width: Math.min(500 * window.uiScale, window.width - 24 * window.uiScale)
        padding: 16 * window.uiScale
        modal: true
        dim: !window.tabletMode
        onOpened: pageQuery.forceActiveFocus()
        contentItem: ColumnLayout {
            spacing: 8 * window.uiScale

            TextField {
                id: pageQuery
                Layout.fillWidth: true
                Layout.preferredHeight: 44 * window.uiScale
                placeholderText: qsTr("Cerca testo, tabelle, PDF e trascrizioni")
                Accessible.name: placeholderText
                maximumLength: 80
                font.pixelSize: 15 * window.uiScale
            }

            ListView {
                id: pageResults
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(220 * window.uiScale, Math.max(44 * window.uiScale, contentHeight))
                model: pageSearch.visible ? window.notebook.searchPages(pageQuery.text) : []
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                delegate: TopButton {
                    required property var modelData
                    width: pageResults.width
                    text: modelData.source === "handwriting"
                        ? qsTr("Pagina %1 · Scrittura a mano · %2").arg(modelData.page + 1).arg(modelData.excerpt)
                        : modelData.source === "pdf"
                          ? qsTr("Pagina %1 · Testo PDF · %2").arg(modelData.page + 1).arg(modelData.excerpt)
                          : qsTr("Pagina %1 · %2").arg(modelData.page + 1).arg(modelData.excerpt)
                    alignLeft: true
                    quiet: true
                    onClicked: if (window.notebook.openPage(modelData.pageId)) pageSearch.close()
                }

                InkLabel {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    visible: pageResults.count === 0 && pageQuery.text.trim().length > 0
                    text: qsTr("Nessun risultato")
                    color: window.muted
                }
            }

            TouchKeyboard {
                visible: window.showTouchKeyboard
                Layout.fillWidth: true
                target: pageQuery
                uiScale: window.uiScale
            }

            TopButton {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Chiudi")
                quiet: true
                onClicked: pageSearch.close()
            }
        }
    }

    WirelessSyncDialog {
        id: wirelessDialog
        dim: !window.tabletMode
        wireless: window.wireless
        notebook: window.notebook
        uiScale: window.uiScale
        syncLabel: window.syncStateLabel(window.currentSync.state || "")
        onResumeOpened: window.showCurrentBook()
    }

    PageItemDialog {
        id: pageItemDialog
        showTouchKeyboard: window.showTouchKeyboard
        uiScale: window.uiScale
        tabletMode: window.tabletMode
        notebook: window.notebook
        onCsvImportRequested: csvOpenPicker.open()
        onCsvExportRequested: item => {
            tableCsvSavePicker.draft = item
            tableCsvSavePicker.open()
        }
        onClosed: if (window.newLibraryItemBook && window.newLibraryItemBook.type === "table")
            window.newLibraryItemBook = null
        onSaveRequested: item => {
            if (window.saveEditedItem(item))
                pageItemDialog.close()
            else
                pageItemDialog.errorText = window.notebook.error
        }
        onSaveOnNewPageRequested: item => {
            if (window.notebook.saveTranscript(item))
                pageItemDialog.close()
            else
                pageItemDialog.errorText = window.notebook.error
        }
        onReplaceTranscriptRequested: (selection, text) => {
            if (window.notebook.replaceHandwriting(selection, text))
                pageItemDialog.close()
            else
                pageItemDialog.errorText = window.notebook.error
        }
        onTableChartRequested: item => {
            if (window.notebook.savePageItem(item)) {
                pageItemDialog.close()
                tableChartDialog.openFor(item, null)
            } else {
                pageItemDialog.errorText = window.notebook.error
            }
        }
        onDeleteRequested: id => {
            if (window.notebook.removePageItem(id)) {
                window.selectedPageItem = ""
                if (window.notebook.pageItems.length === 0)
                    window.editingItems = false
                pageItemDialog.close()
            } else {
                pageItemDialog.errorText = window.notebook.error
            }
        }
    }

    Dialog {
        id: nameDialog

        property bool renaming: false
        property string createType: ""

        anchors.centerIn: parent
        width: Math.min(window.width - 32 * window.uiScale, 420 * window.uiScale)
        padding: 20 * window.uiScale
        modal: true
        dim: !window.tabletMode
        title: renaming ? qsTr("Rinomina quaderno") : createType === "table" ? qsTr("Nuova tabella") : qsTr("Nuovo grafico")
        font.pixelSize: 16 * window.uiScale
        background: Rectangle {
            color: "#ffffff"
            radius: 16 * window.uiScale
            border.color: window.border
            border.width: window.line
        }
        header: InkLabel {
            text: nameDialog.title
            padding: nameDialog.padding
            bottomPadding: 0
            font.pixelSize: 21 * window.uiScale
            font.weight: Font.Bold
        }
        onOpened: {
            nameField.forceActiveFocus()
            nameField.selectAll()
        }

        function submit() {
            const name = nameField.text.trim()
            if (name.length === 0)
                return
            if (renaming) {
                if (window.notebook.renameBook(name))
                    close()
                return
            }
            window.newLibraryItemBook = { title: name, folderId: organizer.destinationFolderId, type: createType }
            close()
            if (createType === "table")
                pageItemDialog.openFor("table", null, "", null)
            else
                chartDialog.openDraft(name)
        }

        contentItem: ColumnLayout {
            spacing: 10 * window.uiScale

            TextField {
                id: nameField
                Accessible.name: qsTr("Nome del quaderno")
                Layout.fillWidth: true
                Layout.preferredHeight: 46 * window.uiScale
                placeholderText: qsTr("Nome del quaderno")
                color: window.ink
                placeholderTextColor: window.muted
                leftPadding: 11 * window.uiScale
                maximumLength: 80
                selectByMouse: true
                background: Rectangle {
                    color: window.surface
                    border.color: window.accent
                    border.width: nameField.activeFocus ? window.line : 0
                    radius: 10 * window.uiScale
                }
                onAccepted: nameDialog.submit()
            }

            TouchKeyboard {
                visible: window.showTouchKeyboard
                Layout.fillWidth: true
                uiScale: window.uiScale
                target: nameField
            }

            InkLabel {
                Layout.fillWidth: true
                visible: text.length > 0
                text: window.notebook.error
                color: "#8c2d27"
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 5 * window.uiScale

                Item { Layout.fillWidth: true }

                TopButton {
                    text: qsTr("Annulla", "Cancel a dialog")
                    onClicked: nameDialog.close()
                }

                TopButton {
                    text: nameDialog.renaming ? qsTr("Salva") : qsTr("Crea")
                    enabled: nameField.text.trim().length > 0
                    filled: true
                    onClicked: nameDialog.submit()
                }
            }
        }
    }
}
