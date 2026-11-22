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
    required property PaperProStandby standby
    required property DeviceInfo deviceInfo
    required property UsbSsh usbSsh
    required property WirelessSync wireless
    property var screenOrientationModel: null
    property var cloudAccountsModel: null
    property var pendingCloudDownloads: []
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
    readonly property var preferences: tabletPreferences
    property int appliedOrientation: 0
    readonly property bool sideways: appliedOrientation === 90 || appliedOrientation === 270
    readonly property real logicalWidth: sideways ? height : width
    readonly property real logicalHeight: sideways ? width : height
    readonly property Item popupOverlay: Overlay.overlay
    readonly property bool orientationBusy: editorPopupOpen || settingsView.dialogOpen || organizer.dialogOpen
        || inkPage.drawing || readingGestures.active || paperViewport.moving || selectionOverlay.active
        || (editingItems && !showingSettings)
    onOrientationBusyChanged: if (!orientationBusy) Qt.callLater(window.applyPreferredOrientation)
    onWidthChanged: Qt.callLater(window.syncOrientationOverlay)
    onHeightChanged: Qt.callLater(window.syncOrientationOverlay)
    onEditingChanged: { if (!editing) placingBookNote = false; Qt.callLater(window.applyPreferredOrientation) }
    onShowingSettingsChanged: { if (showingSettings) placingBookNote = false; Qt.callLater(window.applyPreferredOrientation) }

    readonly property real uiScale: (tabletMode ? Math.max(1, Math.min(width, height) / 790) : 1) * tabletPreferences.interfaceScale / 100
    readonly property real gutter: (logicalWidth < 480 * uiScale ? 12 : 24) * uiScale
    readonly property real line: Math.max(1, Math.round(uiScale))
    readonly property bool wideLibrary: logicalWidth >= 690 * uiScale
    readonly property bool compactLibraryActions: logicalWidth < 890 * uiScale
    readonly property bool stackedInkToolbar: logicalWidth < 440 * uiScale
    readonly property color ink: tabletPreferences.highContrast ? "#000000" : "#1c1c1e"
    readonly property color muted: tabletPreferences.highContrast ? "#202020" : "#515159"
    readonly property color border: tabletPreferences.highContrast ? "#606060" : "#d1d1d6"
    readonly property color surface: "#f2f2f7"
    readonly property color secondarySurface: "#e9e9ef"
    readonly property color accent: "#0059bd"
    readonly property color accentLight: "#e6effb"
    readonly property var libraryBooks: organizer.filteredBooks("")
    readonly property var filteredBooks: organizer.filteredBooks(search.text)
    readonly property bool existingEditorPopupOpen: nameDialog.visible || appleShareView.visible
        || pageTemplateDialog.visible || pageItemDialog.visible || tableHandwritingDialog.visible || diagramHandwritingDialog.visible || pageOverview.visible
        || penTools.visible || documentPicker.visible || pdfSavePicker.visible || pageSearch.visible
        || pageLayers.visible || pageTags.visible || chartDialog.visible || selectionDialog.visible || csvSavePicker.visible
        || inkTransform.visible || usbTransferDialog.visible || usbImportDialog.visible || tableChartDialog.visible
        || bookmarksDialog.visible || wirelessDialog.visible || markdownDialog.visible || markdownSavePicker.visible
        || obsidianSyncDialog.visible || diagramEditor.visible || diagramSavePicker.visible || diagramOpenPicker.visible
        || csvOpenPicker.visible || appleBooksDialog.visible || markdownEditor.visible || imagePicker.visible
        || insertLibraryItemDialog.visible || tableCsvSavePicker.visible || imageProperties.visible || documentDropArea.dialogOpen
        || quickControls.visible || readingPreferencesDialog.visible || cloudImportDialog.visible || cloudUploadPicker.visible
        || Boolean(cloudDialogLoader.item?.visible)
    readonly property bool editorPopupOpen: existingEditorPopupOpen || workspaceTools.dialogOpen || bookNoteDialog.visible
    readonly property bool wirelessEditorBusy: inkPage.drawing || selectionOverlay.active || editingItems
        || placingBookNote || bookNoteDialog.dirty || (editorPopupOpen && !wirelessDialog.visible)
    onWirelessEditorBusyChanged: wireless.transfer.setEditorBusy(wirelessEditorBusy)
    readonly property bool fastPanelRefresh: tabletPreferences.refreshProfile === "fast" || (tabletPreferences.refreshProfile !== "clean"
        && (pinLock.locked || !editing || showingSettings || editorPopupOpen
        || (!notebook.hasPageBackground && !notebook.pageItems.some(item =>
            item.type === "image" || item.type === "chart" || item.type === "diagram"))))
    onFastPanelRefreshChanged: epaperRefresh.setFastRefresh(fastPanelRefresh)
    Component.onCompleted: {
        epaperRefresh.setFastRefresh(fastPanelRefresh)
        Qt.callLater(window.applyPreferredOrientation)
        Qt.callLater(window.syncOrientationOverlay)
    }
    readonly property string displayedPageKey: visible && editing && !showingSettings && !pinLock.locked
        && notebook.bookId.length > 0 && notebook.pageId.length > 0 ? notebook.bookId + "/" + notebook.pageId : ""
    readonly property bool canRefreshPage: epaperRefresh.available && displayedPageKey.length > 0
        && !inkPage.drawing && !paperViewport.moving && !readingGestures.active && !editorPopupOpen && !organizer.dialogOpen && !settingsView.dialogOpen
    onDisplayedPageKeyChanged: epaperRefresh.watchPageChange(displayedPageKey.length > 0)
    readonly property var currentSync: {
        const version = wireless.transfer.bookStatusVersion
        return notebook.bookId.length ? wireless.transfer.bookStatus(notebook.bookId) : ({})
    }
    readonly property string bookSyncStatus: {
        const state = currentSync.state || ""
        if (["sending", "error", "conflict"].includes(state)) return syncStateLabel(state)
        if (!wireless.transfer.connected) return tabletMode ? qsTr("Mac non collegato") : qsTr("Paper Pro non collegato")
        if (!wireless.transfer.pinnedBooks.includes(notebook.bookId)) return qsTr("Libro non selezionato per la sincronizzazione automatica")
        if (!wireless.transfer.autoSync || tabletPreferences.wifiAutoSync === "manual") return qsTr("Sincronizzazione manuale")
        if (state === "localChanges" && tabletPreferences.wifiAutoSync === "onOpen") return qsTr("Modifiche in attesa della prossima apertura")
        return syncStateLabel(state) || (tabletPreferences.wifiAutoSync === "onChange"
            ? qsTr("Sincronizzazione dopo le modifiche") : tabletPreferences.wifiAutoSync === "onOpen"
            ? qsTr("Sincronizzazione all’apertura") : qsTr("Sincronizzazione a intervalli"))
    }
    readonly property bool hasResumeOffer: Boolean(wireless.transfer.resumeOffer.pageId)

    property bool editing: false
    property bool showingSettings: false
    property bool fullPage: false
    property bool editingItems: false
    property bool lassoActive: false
    property string editorWorkspaceBookId: ""
    property string editorWorkspaceOverride: ""
    readonly property var currentBookSummary: notebook.books.find(book => book.id === notebook.bookId) || ({})
    readonly property string editorWorkspace: editorWorkspaceBookId === notebook.bookId && editorWorkspaceOverride.length
        ? editorWorkspaceOverride : currentBookSummary.document ? "books" : notebook.diagramBook ? "diagrams" : "writing"
    readonly property var handwritingEngine: handwriting
    readonly property var pageSheet: sheet
    property string lassoMode: "contained"
    property string lassoOperation: "replace"
    readonly property bool displayingSelection: selectionOverlay.active && !lassoActive
    property var inkClipboard: []
    property real readingZoom: 1
    property bool readingFitWidth: false
    property real canvasZoom: 1
    readonly property real pageZoom: notebook.hasPageBackground ? readingZoom : canvasZoom
    property bool smartShapesEnabled: false
    property string statusClock: tabletPreferences.formatClock()
    readonly property string recognitionLanguage: tabletPreferences.ocrLanguage === "auto" ? notebook.handwritingLanguage : tabletPreferences.ocrLanguage
    readonly property bool backgroundWorkAllowed: !pinLock.locked && !standby.sleeping && !notebook.importing
        && !wirelessEditorBusy && !settingsView.dialogOpen
    property bool loadingReadingPreferences: false
    function readingPreference(key, fallback) {
        const values = notebook.localReadingPreferences
        return values[key] === undefined ? fallback : values[key]
    }
    function restoreReadingPreferences() {
        readingZoomSave.stop()
        loadingReadingPreferences = true
        readingZoom = readingPreference("readingZoom", tabletPreferences.readingZoom) / 100
        readingFitWidth = false
        loadingReadingPreferences = false
        Qt.callLater(window.applyPreferredOrientation)
    }
    onReadingZoomChanged: if (!loadingReadingPreferences && editing && notebook.hasPageBackground) {
        readingZoomSave.bookId = notebook.bookId
        readingZoomSave.restart()
    }
    property string selectionMessage: ""
    property string selectedPageItem: ""
    property bool placingBookNote: false
    property string bookNotePlacementContext: ""
    property var newLibraryItemBook: null
    property string importedBookId: ""
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

    onClosing: close => {
        if (bookNoteDialog.dirty) {
            close.accepted = false
            if (!pinLock.locked && !standby.sleeping) bookNoteDialog.requestClose(true)
            return
        }
        if (markdownEditor.visible && markdownEditor.dirty) {
            close.accepted = false
            markdownEditor.requestDiscard(true)
        }
    }

    title: qsTr("Quaderno")
    // The e-paper platform does not resize the window on a fullscreen request.
    width: epaperGeometry.width > 0 ? epaperGeometry.width : Math.max(380, Math.min(960, Screen.desktopAvailableWidth - 56))
    height: epaperGeometry.height > 0 ? epaperGeometry.height : Math.max(500, Math.min(790, Screen.desktopAvailableHeight - 52))
    minimumWidth: 380
    minimumHeight: 500
    visibility: tabletMode ? Window.FullScreen : Window.Windowed
    font.family: fontFamily
    font.bold: tabletPreferences.boldLabels
    color: surface
    palette.window: surface
    palette.base: "white"
    palette.windowText: ink
    palette.text: ink
    palette.buttonText: ink
    palette.highlight: accent
    palette.highlightedText: "white"

    TabletPreferences {
        id: tabletPreferences
        objectName: "tabletPreferences"
        onOrientationChanged: Qt.callLater(window.applyPreferredOrientation)
        onAutomaticOrientationChanged: Qt.callLater(window.applyPreferredOrientation)
        onAutoDiagramRecognitionChanged: {
            window.diagramRecognitionDismissed = false
            window.scheduleDiagramRecognition()
        }
        onTimeZoneIdChanged: window.statusClock = formatClock()
        onClock24HourChanged: window.statusClock = formatClock()
        onReadingZoomChanged: if (window.notebook.localReadingPreferences.readingZoom === undefined) window.restoreReadingPreferences()
        onReadingOrientationChanged: Qt.callLater(window.applyPreferredOrientation)
        onUsageProfileApplied: Qt.callLater(function() { if (window.editing) window.setFullPage(tabletPreferences.startFullPage) })
    }

    Binding {
        target: window.screenOrientationModel
        property: "enabled"
        value: window.tabletMode && tabletPreferences.automaticOrientation && !window.pinLock.locked && !window.standby.sleeping
        when: window.screenOrientationModel !== null
    }
    Connections {
        target: window.screenOrientationModel
        function onChanged() { Qt.callLater(window.applyPreferredOrientation) }
    }

    function syncOrientationOverlay() {
        const overlay = popupOverlay
        if (!overlay) return
        // Qt sizes the overlay from the native window during resize. Reapply
        // the logical geometry after that layout without altering the window.
        const x = (width - logicalWidth) / 2
        const y = (height - logicalHeight) / 2
        if (overlay.width !== logicalWidth) overlay.width = logicalWidth
        if (overlay.height !== logicalHeight) overlay.height = logicalHeight
        if (overlay.x !== x) overlay.x = x
        if (overlay.y !== y) overlay.y = y
        overlay.transformOrigin = Item.Center
        if (overlay.rotation !== appliedOrientation) overlay.rotation = appliedOrientation
    }

    function applyPreferredOrientation() {
        const bookAngle = editing && !showingSettings && notebook.hasPageBackground
            ? readingPreference("readingOrientation", tabletPreferences.readingOrientation) : -1
        const sensorAngle = tabletPreferences.automaticOrientation && screenOrientationModel
            ? screenOrientationModel.orientation : -1
        const preferred = bookAngle >= 0 ? bookAngle : sensorAngle >= 0 ? sensorAngle : tabletPreferences.orientation
        const angle = tabletMode && [0, 90, 180, 270].includes(preferred) ? preferred : 0
        if (orientationBusy || pinLock.locked || standby.sleeping || appliedOrientation === angle) return
        inkPage.cancelDrawing()
        selectionOverlay.clear()
        lassoActive = false
        selectedPageItem = ""
        const centerX = (paperViewport.contentX + paperViewport.width / 2) / Math.max(1, paperViewport.contentWidth)
        const centerY = (paperViewport.contentY + paperViewport.height / 2) / Math.max(1, paperViewport.contentHeight)
        appliedOrientation = angle
        syncOrientationOverlay()
        Qt.callLater(function() {
            window.syncOrientationOverlay()
            paperViewport.ensurePolished()
            readingGestures.clampPosition(centerX * paperViewport.contentWidth - paperViewport.width / 2,
                                         centerY * paperViewport.contentHeight - paperViewport.height / 2)
        })
    }

    Connections {
        target: window.popupOverlay
        function onWidthChanged() { Qt.callLater(window.syncOrientationOverlay) }
        function onHeightChanged() { Qt.callLater(window.syncOrientationOverlay) }
        function onXChanged() { Qt.callLater(window.syncOrientationOverlay) }
        function onYChanged() { Qt.callLater(window.syncOrientationOverlay) }
    }

    Item {
        id: orientedScene
        objectName: "orientedScene"
        width: window.logicalWidth
        height: window.logicalHeight
        anchors.centerIn: parent
        rotation: window.appliedOrientation
        transformOrigin: Item.Center
    }

    function refreshPage() {
        if (canRefreshPage)
            epaperRefresh.request()
    }

    function showBook(id: string) {
        if (notebook.openBook(id)) {
            selectEditorWorkspace("")
            showCurrentBook()
        }
    }

    function openLibraryItem(entry: var) {
        const item = notebook.openLibraryItem(entry.bookId, entry.pageId, entry.itemId)
        if (!item.id && !item.workspace)
            return
        selectEditorWorkspace(({chart: "charts", table: "tables", diagram: "diagrams"})[item.type] || "")
        showCurrentBook()
    }

    function showCurrentBook() {
        if (editorWorkspaceBookId !== notebook.bookId)
            selectEditorWorkspace("")
        if (!editing)
            fullPage = tabletPreferences.startFullPage
        librarySearch.active = false
        settingsView.closeDialogs()
        showingSettings = false
        editing = true
        restoreReadingPreferences()
        syncAutomation.bookOpened(notebook.bookId)
        pageView.forceActiveFocus()
    }

    function selectEditorWorkspace(workspace: string) {
        editorWorkspaceBookId = notebook.bookId
        editorWorkspaceOverride = workspace
    }

    function canImportOnSheet(key: string): bool {
        return key.length > 0 && key === notebook.bookId + "/" + notebook.pageId
            && editing && !showingSettings && !pinLock.locked && !standby.sleeping && !notebook.importing
    }

    function workspaceAction(action: string, itemId: string) {
        if (!editing || showingSettings || pinLock.locked || standby.sleeping || notebook.importing || inkPage.drawing
            || editorPopupOpen || settingsView.dialogOpen || organizer.dialogOpen) return
        const item = itemId.length ? notebook.pageItems.find(entry => entry.id === itemId) : null
        if (itemId.length && !item) return
        switch (action) {
        case "highlighter": case "underline": selectTool(action); break
        case "bookNote": window.startBookNote(); break
        case "annotatedPages":
            pageOverview.open()
            pageOverview.pageFilter = "annotated"
            break
        case "bookSync": wirelessDialog.begin(); break
        case "markdown": markdownEditor.begin(!notebook.markdownBook); break
        case "text": editPageItem("text", null); break
        case "image":
            imagePicker.targetBook = notebook.bookId
            imagePicker.targetPage = notebook.pageId
            imagePicker.open()
            break
        case "library": insertLibraryItemDialog.begin(); break
        case "chartEdit": if (item && item.type === "chart") editPageItem("chart", item); break
        case "newChart": chartDialog.openDraft(""); break
        case "chartFromTable": if (item && item.type === "table") tableChartDialog.openFor(item, null); break
        case "chartCsv":
            if (item && item.type === "chart") {
                const csv = workspaceChartExporter.csv(item)
                if (csv.length) { csvSavePicker.csvData = csv; csvSavePicker.open() }
            }
            break
        case "tableEdit": if (item && item.type === "table") editPageItem("table", item); break
        case "newTable": editPageItem("table", null); break
        case "importCsv":
            csvOpenPicker.begin(true)
            break
        case "tableCsv":
            if (item && item.type === "table") { tableCsvSavePicker.draft = item; tableCsvSavePicker.open() }
            break
        case "diagramRecognize": reviewDiagramSelection(null); break
        case "diagramNew": editPageItem("diagram", null); break
        case "diagramEdit": if (item && item.type === "diagram") editPageItem("diagram", item); break
        case "diagramImport":
            diagramOpenPicker.begin(true)
            break
        }
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
        value: window.showingSettings || (window.editorPopupOpen && !documentDropArea.dialogOpen) || organizer.dialogOpen || inkPage.drawing
    }

    Connections {
        target: window.documentExchange
        function onImported(bookId) {
            if (!window.editing && !window.showingSettings && !window.editorPopupOpen && !window.pinLock.locked) {
                const book = window.notebook.books.find(entry => entry.id === bookId)
                if (book) organizer.section = book.document ? "books" : "writing"
                window.showBook(bookId)
                window.importedBookId = ""
            } else if (window.notebook.bookId !== bookId) {
                window.importedBookId = bookId
            }
        }
    }

    function selectTool(tool: string) {
        placingBookNote = false
        inkPage.cancelDrawing()
        selectionOverlay.clear()
        penTool = tool
        editingItems = false
        lassoActive = false
        selectedPageItem = ""
    }

    function showLibrary() {
        inkPage.cancelDrawing()
        selectionOverlay.clear()
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
        selectionOverlay.clear()
        librarySearch.active = false
        editing = false
        fullPage = false
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
            : organizer.section === "diagrams" ? "diagram" : "chart"
        nameField.text = renaming ? notebook.title : ""
        nameDialog.open()
    }

    function saveEditedItem(item: var): bool {
        if (!newLibraryItemBook)
            return notebook.savePageItem(item)
        if (!notebook.createBookWithItem(newLibraryItemBook.title, newLibraryItemBook.folderId, item))
            return false
        selectEditorWorkspace(({chart: "charts", table: "tables", diagram: "diagrams"})[newLibraryItemBook.type] || "")
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
        selectionOverlay.clear()
        lassoActive = false
        selectedPageItem = item && item.id ? item.id : ""
        if (type === "note") {
            bookNoteDialog.openFor(item, Qt.point(item.x, item.y))
        } else if (type === "image") {
            imageProperties.item = item
            imageProperties.open()
        } else if (type === "diagram") {
            diagramEditor.openFor(item)
        } else if (type === "chart" && item && item.sourceTable) {
            const source = notebook.pageItems.find(element => element.id === item.sourceTable)
            tableChartDialog.openFor(source, item)
        } else if (type === "chart")
            chartDialog.openItem(item)
        else
            pageItemDialog.openFor(type, item, "", null)
    }

    function startBookNote() {
        if (!editing || showingSettings || pinLock.locked || standby.sleeping) return
        if (bookNoteDialog.hasDraft && bookNoteDialog.dirty) { bookNoteDialog.resume(); return }
        if (!editing || showingSettings || pinLock.locked || standby.sleeping || editorPopupOpen || !notebook.canWriteLayer) return
        inkPage.cancelDrawing()
        selectionOverlay.clear()
        editingItems = false; lassoActive = false
        bookNotePlacementContext = notebook.bookId + "/" + notebook.pageId
        placingBookNote = true
    }
    function placeBookNote(point: point) {
        if (!placingBookNote || bookNotePlacementContext !== notebook.bookId + "/" + notebook.pageId
            || pinLock.locked || standby.sleeping || !notebook.canWriteLayer) return
        placingBookNote = false
        bookNoteDialog.openFor(null, point)
    }
    function editBookNote(id: string) {
        if (pinLock.locked || standby.sleeping || editorPopupOpen || !notebook.canWriteLayer) return
        const item = notebook.pageItems.find(value => value.type === "note" && value.id === id)
        if (item) editPageItem("note", item)
    }
    function resumeBookNote() {
        if (!pinLock.locked && !standby.sleeping && bookNoteDialog.suspended) bookNoteDialog.resume()
    }

    function editPageText(id: string) {
        const item = notebook.pageItems.find(entry => entry.id === id && entry.type === "text")
        if (item && editing && !showingSettings && !pinLock.locked && !editorPopupOpen) {
            pinLock.recordActivity()
            editPageItem("text", item)
        }
    }

    function editTableCell(id: string, row: int, column: int) {
        const item = notebook.pageItems.find(entry => entry.id === id && entry.type === "table")
        if (!item || !editing || showingSettings || pinLock.locked || standby.sleeping || editorPopupOpen
            || row < 0 || row >= item.rows || column < 0 || column >= item.cols)
            return
        pinLock.recordActivity()
        standby.recordActivity()
        editPageItem("table", item)
        pageItemDialog.selectCell(row, column, true)
    }

    function editPageChart(id: string) {
        const item = notebook.pageItems.find(entry => entry.id === id && entry.type === "chart")
        if (!item || !editing || showingSettings || pinLock.locked || standby.sleeping || editorPopupOpen)
            return
        pinLock.recordActivity()
        standby.recordActivity()
        editPageItem("chart", item)
    }

    function openInkTransform(strokes: var, pasting: bool, indexes: var) {
        inkPage.cancelDrawing()
        selectionOverlay.clear()
        lassoActive = false
        inkTransform.openFor(strokes, pasting, notebook.pagePreview(notebook.page), indexes)
    }

    function editPageDiagram(id: string) {
        const item = notebook.pageItems.find(entry => entry.id === id && entry.type === "diagram")
        if (!item || !editing || showingSettings || pinLock.locked || standby.sleeping || editorPopupOpen) return
        pinLock.recordActivity()
        standby.recordActivity()
        editPageItem("diagram", item)
    }

    property var diagramRecognitionOffer: null
    property bool diagramRecognitionDismissed: false
    property string diagramRecognitionContext: ""
    function dismissDiagramRecognition() {
        diagramRecognitionDismissed = true
        diagramRecognitionOffer = null
        diagramRecognitionDelay.stop()
    }
    function updateDiagramRecognitionContext() {
        const key = notebook.bookId + "/" + notebook.pageId + "/" + notebook.activeLayer
        if (key !== diagramRecognitionContext) {
            diagramRecognitionContext = key
            diagramRecognitionDismissed = false
        }
        scheduleDiagramRecognition()
    }
    readonly property bool diagramReviewAllowed: editing && !showingSettings && !pinLock.locked && !standby.sleeping
        && !notebook.importing && notebook.canWriteLayer && !inkPage.drawing && !editorPopupOpen
        && !settingsView.dialogOpen && !organizer.dialogOpen && !readingGestures.active && !paperViewport.moving
    readonly property bool automaticDiagramAllowed: diagramReviewAllowed && editorWorkspace === "diagrams"
        && tabletPreferences.autoDiagramRecognition && !diagramRecognitionDismissed && !selectionOverlay.active && !lassoActive && !editingItems
    onAutomaticDiagramAllowedChanged: scheduleDiagramRecognition()
    function diagramPageSelection() {
        return notebook.selectContent([{x: 0, y: 0}, {x: 1, y: 0}, {x: 1, y: 1}, {x: 0, y: 1}],
            "contained", ({}), "replace")
    }
    function scheduleDiagramRecognition() {
        diagramRecognitionOffer = null
        diagramRecognitionDelay.stop()
        if (automaticDiagramAllowed && notebook.strokes.length > 0) diagramRecognitionDelay.restart()
    }
    function reviewDiagramSelection(selection: var) {
        if (!diagramReviewAllowed) return
        inkPage.cancelDrawing()
        handwriting.cancel()
        const source = selection || diagramPageSelection()
        selectionOverlay.clear()
        lassoActive = false
        diagramRecognitionOffer = null
        diagramHandwritingDialog.begin(source)
    }
    Timer {
        id: diagramRecognitionDelay
        interval: 1200
        onTriggered: {
            if (!window.automaticDiagramAllowed) return
            const plan = window.notebook.diagramHandwritingPlan(window.diagramPageSelection())
            if (!plan.error && (plan.requiresTarget || (plan.hasChanges && plan.diagram && plan.diagram.nodes && plan.diagram.nodes.length)))
                window.diagramRecognitionOffer = {nodes: plan.diagram ? (plan.diagram.nodes || []).length : 0,
                    edges: plan.diagram ? (plan.diagram.edges || []).length : 0, requiresTarget: Boolean(plan.requiresTarget)}
        }
    }

    function setLasso(active: bool) {
        placingBookNote = false
        inkPage.cancelDrawing()
        editingItems = false
        selectedPageItem = ""
        selectionMessage = ""
        lassoOperation = "replace"
        selectionOverlay.clear()
        lassoActive = active
    }

    function useLasso(polygon: var) {
        const selection = notebook.selectContent(polygon, lassoMode, selectionOverlay.selection || ({}), lassoOperation)
        if (!selection.strokes || (selection.strokes.length === 0 && selection.items.length === 0)) {
            selectionMessage = selection.error || qsTr("Nessun elemento selezionato. Circonda tratti o contenuti della pagina.")
            if (lassoOperation === "subtract") selectionOverlay.clear()
            return
        }
        lassoActive = false
        selectionOverlay.begin(selection)
        lassoOperation = "replace"
        if (tabletMode && editorWorkspace !== "diagrams" && handwriting.available && selectionOverlay.hasHandwriting) {
            if (tabletPreferences.lassoInsertTarget === "cells" && notebook.pageItems.some(item => item.type === "table"))
                tableHandwritingDialog.openFor(selection)
            else handwriting.recognize(selection.strokes, recognitionLanguage)
        }
    }

    function setFullPage(enabled: bool) {
        if (inkPage.drawing) return
        const x = (paperViewport.contentX + paperViewport.width / 2) / Math.max(1, paperViewport.contentWidth)
        const y = (paperViewport.contentY + paperViewport.height / 2) / Math.max(1, paperViewport.contentHeight)
        fullPage = enabled
        Qt.callLater(function() {
            paperViewport.ensurePolished()
            paperViewport.contentX = Math.max(0, Math.min(paperViewport.contentWidth - paperViewport.width,
                x * paperViewport.contentWidth - paperViewport.width / 2))
            paperViewport.contentY = Math.max(0, Math.min(paperViewport.contentHeight - paperViewport.height,
                y * paperViewport.contentHeight - paperViewport.height / 2))
        })
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

    function fitCanvasToView() {
        if (notebook.hasPageBackground || inkPage.drawing) return
        canvasZoom = 1
        readingGestures.clampPosition(0, 0)
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
                 && !window.pinLock.locked && !window.standby.sleeping && !inkPage.drawing
                 && !window.editorPopupOpen && !selectionOverlay.previewTranscript && !settingsView.dialogOpen && !organizer.dialogOpen
        onActivated: {
            if (window.placingBookNote) {
                window.placingBookNote = false
            } else if (selectionOverlay.active) {
                selectionOverlay.clear()
            } else if (librarySearch.active) {
                librarySearch.active = false
                newBook.forceActiveFocus()
            } else if (window.fullPage) {
                window.setFullPage(false)
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
        enabled: window.editing && !window.editingItems && !window.pinLock.locked && !window.standby.sleeping && !window.editorPopupOpen && !selectionOverlay.previewTranscript
        onActivated: { selectionOverlay.clear(); window.notebook.undo() }
    }

    Shortcut {
        sequences: [StandardKey.Redo]
        enabled: window.editing && !window.editingItems && !window.pinLock.locked && !window.standby.sleeping && !window.editorPopupOpen && !selectionOverlay.previewTranscript
        onActivated: { selectionOverlay.clear(); window.notebook.redo() }
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: window.editing && !window.pinLock.locked && !window.standby.sleeping && !window.editorPopupOpen && !selectionOverlay.previewTranscript
        onActivated: window.openPageSearch()
    }

    Shortcut {
        sequence: "Ctrl+0"
        enabled: window.editing && !window.showingSettings && !window.notebook.hasPageBackground
            && window.canvasZoom > 1 && !window.editorPopupOpen && !inkPage.drawing
            && !window.pinLock.locked && !window.standby.sleeping
        onActivated: window.fitCanvasToView()
    }

    ButtonGroup { id: penColors }

    UsbTransfer { id: usbTransfer }

    Handwriting {
        id: handwriting
        numericMode: tabletPreferences.ocrNumericMode
        wordList: tabletPreferences.ocrWordList
        corrections: tabletPreferences.ocrCorrections
        onFinished: success => {
            if (success && selectionOverlay.active && !window.pinLock.locked && !window.standby.sleeping)
                selectionOverlay.showTranscription()
        }
    }

    Binding { target: window.standby; property: "drawing"; value: inkPage.drawing }
    Binding { target: window.standby; property: "allowed"; value: !window.existingEditorPopupOpen && !window.notebook.importing && !handwriting.busy }
    Binding { target: window.standby; property: "idleMinutes"; value: tabletPreferences.idleMinutes }
    Binding { target: window.standby; property: "keepAwakeWhileCharging"; value: tabletPreferences.keepAwakeWhileCharging }
    Timer {
        interval: 60000
        repeat: true
        triggeredOnStart: true
        running: tabletPreferences.showStatusBar && tabletPreferences.showClock && !window.fullPage
            && !window.pinLock.locked && !window.standby.sleeping
        onTriggered: window.statusClock = tabletPreferences.formatClock()
    }
    Timer {
        interval: 15000
        repeat: true
        triggeredOnStart: true
        running: tabletPreferences.showStatusBar && !window.fullPage && !window.showingSettings
            && !window.pinLock.locked && !window.standby.sleeping
        onTriggered: window.deviceInfo.refresh()
    }
    Connections {
        target: window.standby
        function onSleepingChanged() {
            Qt.callLater(window.applyPreferredOrientation)
            if (!window.standby.sleeping) Qt.callLater(window.resumeBookNote)
            if (window.standby.sleeping) {
                window.placingBookNote = false
                bookNoteDialog.suspend()
                inkPage.cancelDrawing()
                tableHandwritingDialog.clearPrivateState()
                diagramHandwritingDialog.clearPrivateState()
                quickControls.close()
                readingPreferencesDialog.close()
                cloudImportDialog.close()
                cloudUploadPicker.close()
                if (cloudDialogLoader.item) cloudDialogLoader.item.close()
                window.setLasso(false)
                handwriting.cancel()
            }
        }
    }

    Connections {
        target: window.pinLock
        function onLockedChanged() {
            Qt.callLater(window.applyPreferredOrientation)
            if (!window.pinLock.locked) Qt.callLater(window.resumeBookNote)
            if (window.pinLock.locked) {
                window.placingBookNote = false
                bookNoteDialog.suspend()
                inkPage.cancelDrawing()
                librarySearch.active = false
                nameDialog.close()
                appleShareView.close()
                appleBooksDialog.close()
                pageTemplateDialog.close()
                pageItemDialog.close()
                tableHandwritingDialog.clearPrivateState()
                diagramHandwritingDialog.clearPrivateState()
                quickControls.close()
                readingPreferencesDialog.close()
                cloudImportDialog.close()
                cloudUploadPicker.close()
                if (cloudDialogLoader.item) cloudDialogLoader.item.close()
                pageOverview.close()
                penTools.close()
                documentPicker.close()
                pdfSavePicker.close()
                markdownSavePicker.close()
                markdownEditor.close()
                imagePicker.close()
                documentDropArea.closeDialogs()
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
        font.bold: tabletPreferences.boldLabels
        textFormat: Text.PlainText
    }

    ColumnLayout {
        parent: orientedScene
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
                    iconOnly: window.logicalWidth < 530 * window.uiScale
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
                    iconOnly: window.logicalWidth < 560 * window.uiScale
                    quiet: true
                    onClicked: window.showSettings()
                }

                TopButton {
                    visible: window.editing && usbTransfer.available && !window.tabletMode
                    text: qsTr("Invia al Paper Pro")
                    iconName: "upload"
                    iconOnly: window.logicalWidth < 760 * window.uiScale
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
                    iconOnly: window.logicalWidth < 770 * window.uiScale
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
                    iconOnly: window.logicalWidth < 680 * window.uiScale
                    quiet: true
                    onClicked: window.shareBook()
                }

                TopButton {
                    objectName: "quickControlsButton"
                    visible: tabletPreferences.quickPanelEnabled
                    text: qsTr("Controlli rapidi")
                    iconName: "settings"
                    iconOnly: true
                    quiet: true
                    onClicked: quickControls.open()
                }

                TopButton {
                    visible: window.tabletMode
                    text: qsTr("Esci")
                    iconName: "exit"
                    iconOnly: window.logicalWidth < 530 * window.uiScale
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
            objectName: "deviceStatusRow"
            visible: tabletPreferences.showStatusBar && !window.fullPage
            Layout.fillWidth: true
            Layout.leftMargin: window.gutter
            Layout.rightMargin: window.gutter
            Layout.topMargin: 4 * window.uiScale
            Layout.bottomMargin: 4 * window.uiScale
            spacing: 14 * window.uiScale

            InkLabel {
                objectName: "statusClock"
                visible: tabletPreferences.showClock
                text: window.statusClock
                font.pixelSize: 12 * window.uiScale
            }
            InkLabel {
                visible: window.deviceInfo.batteryPercent >= 0
                text: qsTr("Batteria %1%").arg(window.deviceInfo.batteryPercent)
                    + (window.deviceInfo.charging ? qsTr(" · in carica") : "")
                font.pixelSize: 12 * window.uiScale
            }
            InkLabel {
                objectName: "markerStatusLabel"
                visible: window.tabletMode && tabletPreferences.showPenBattery
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                elide: Text.ElideRight
                text: window.deviceInfo.markerPresent && window.deviceInfo.markerBatteryPercent >= 0
                    ? qsTr("Penna %1%").arg(window.deviceInfo.markerBatteryPercent)
                        + (window.deviceInfo.markerCharging ? qsTr(" · in carica") : "")
                    : window.deviceInfo.markerStatus
                font.pixelSize: 12 * window.uiScale
            }
            Item { Layout.fillWidth: !window.tabletMode || !tabletPreferences.showPenBattery }
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
            preferences: tabletPreferences
            maintenance: libraryMaintenance
            cloudAccounts: window.cloudAccountsModel
            screenOrientation: window.screenOrientationModel
            onOpenCloudAccount: provider => { if (cloudDialogLoader.item) cloudDialogLoader.item.begin(provider) }
            standby: window.standby
            externalKeyboard: window.externalKeyboard
            obsidianSync: window.obsidianSync
            onOpenBluetooth: wirelessDialog.begin(true)
            onOpenWireless: wirelessDialog.begin()
            onOpenObsidianSync: obsidianSyncDialog.begin(organizer.destinationFolderId)
            onOpenMarkdown: markdownDialog.begin(window.editing, organizer.destinationFolderId)
            onOpenApple: appleShareView.open()
            onOpenAppleBooks: appleBooksDialog.open()
            onImportFiles: window.openDocumentPicker()
            onRefreshScreen: window.epaperRefresh.request()
            onSleepNow: window.standby.suspend()
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
                            : organizer.section === "charts" ? qsTr("Nuovo grafico")
                            : organizer.section === "diagrams" ? qsTr("Nuovo diagramma") : qsTr("Nuovo Markdown")
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
                        objectName: "libraryBooks"
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
                            objectName: "libraryRow_" + (modelData.itemId || modelData.id || modelData.bookId)

                            required property var modelData
                            required property int index
                            readonly property bool isItem: Boolean(modelData.itemId) || Boolean(modelData.empty)
                            readonly property string syncLabel: {
                                const version = window.wireless.transfer.bookStatusVersion
                                return modelData.trashed ? "" : window.syncStateLabel(window.wireless.transfer.bookStatus(isItem ? modelData.bookId : modelData.id).state)
                            }

                            width: books.width - (libraryScrollbar.size < 1 ? 8 * window.uiScale : 0)
                            hoverEnabled: !window.tabletMode
                            implicitHeight: 68 * window.uiScale
                            leftPadding: 14 * window.uiScale
                            rightPadding: 14 * window.uiScale
                            text: modelData.title || (modelData.type === "chart" ? qsTr("Grafico senza titolo")
                                : modelData.type === "diagram" ? qsTr("Diagramma senza titolo") : qsTr("Tabella senza titolo"))
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
                                    objectName: "libraryBookActions_" + (bookRow.modelData.bookId || bookRow.modelData.id)
                                    visible: !bookRow.isItem || bookRow.modelData.type === "diagram"
                                    text: qsTr("Azioni del quaderno")
                                    iconName: "more"
                                    iconOnly: true
                                    quiet: true
                                    onClicked: {
                                        const owner = bookRow.isItem ? window.notebook.books.find(book => book.id === bookRow.modelData.bookId) : bookRow.modelData
                                        if (owner) organizer.openBookActions(owner)
                                    }
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
                                      : organizer.section === "tables" ? qsTr("Nessuna tabella")
                                      : organizer.section === "diagrams" ? qsTr("Nessun diagramma") : qsTr("Nessun quaderno")
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
            Layout.minimumWidth: 0
            spacing: 0

            WorkspaceTools {
                id: workspaceTools
                Layout.fillWidth: true
                Layout.leftMargin: 8 * window.uiScale
                Layout.rightMargin: Layout.leftMargin
                Layout.maximumWidth: Math.max(0, window.logicalWidth - Layout.leftMargin - Layout.rightMargin)
                Layout.alignment: Qt.AlignLeft
                Layout.topMargin: 6 * window.uiScale
                visible: !window.fullPage
                uiScale: window.uiScale
                tabletMode: window.tabletMode
                workspace: window.editorWorkspace
                bookId: window.notebook.bookId
                pageId: window.notebook.pageId
                pageItems: window.notebook.pageItems
                active: window.editing && !window.showingSettings && !window.pinLock.locked && !window.standby.sleeping
                actionsEnabled: !window.editorPopupOpen && !inkPage.drawing && !window.notebook.importing
                    && !settingsView.dialogOpen && !organizer.dialogOpen
                canWrite: window.notebook.canWriteLayer
                markdownBook: window.notebook.markdownBook
                hasStrokes: window.notebook.strokes.length > 0
                diagramReviewAllowed: window.diagramReviewAllowed
                autoDiagramRecognition: tabletPreferences.autoDiagramRecognition
                penTool: window.penTool
                syncStatus: window.bookSyncStatus
                onActionRequested: (action, itemId) => window.workspaceAction(action, itemId)
                onAutomaticRecognitionChanged: enabled => tabletPreferences.autoDiagramRecognition = enabled
            }

            Rectangle {
                visible: !window.fullPage
                Layout.fillWidth: true
                Layout.leftMargin: 8 * window.uiScale
                Layout.rightMargin: Layout.leftMargin
                Layout.topMargin: 8 * window.uiScale
                implicitHeight: 50 * window.uiScale
                radius: 12 * window.uiScale
                color: "white"

                Flickable {
                    id: favoriteToolsViewport
                    anchors.fill: parent
                    anchors.margins: 4 * window.uiScale
                    contentWidth: Math.max(width, favoriteTools.implicitWidth)
                    contentHeight: height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.HorizontalFlick
                    RowLayout {
                    id: favoriteTools
                    width: favoriteToolsViewport.contentWidth
                    height: favoriteToolsViewport.height
                    spacing: 2 * window.uiScale
                    layoutDirection: tabletPreferences.handedness === "left" ? Qt.RightToLeft : Qt.LeftToRight

                    TopButton {
                        visible: !window.notebook.hasPageBackground
                        text: qsTr("Modello")
                        iconName: "paper"
                        quiet: true
                        iconOnly: window.logicalWidth < 700 * window.uiScale
                        onClicked: {
                            inkPage.cancelDrawing()
                            pageTemplateDialog.open()
                        }
                    }

                    Repeater {
                        model: tabletPreferences.toolbarActions
                        TopButton {
                            required property string modelData
                            objectName: "toolbarAction_" + modelData
                            text: ({pen: qsTr("Penna"), highlighter: qsTr("Evidenziatore"), eraser: qsTr("Gomma"),
                                lasso: qsTr("Lazo"), shapes: qsTr("Forme"), text: qsTr("Testo"),
                                table: qsTr("Tabella"), chart: qsTr("Grafico")})[modelData] || modelData
                            iconName: ({pen:"pen", highlighter:"pen", eraser:"eraser", lasso:"lasso", shapes:"diagram",
                                text:"text", table:"table", chart:"chart"})[modelData]
                            quiet: true
                            iconOnly: true
                            ToolTip.visible: hovered && !window.tabletMode
                            ToolTip.text: text
                            enabled: modelData === "text" || modelData === "table" || modelData === "chart" || window.notebook.canWriteLayer
                            filled: modelData === "lasso" ? window.lassoActive : modelData === window.penTool
                            onClicked: {
                                if (modelData === "lasso") window.setLasso(!window.lassoActive)
                                else if (modelData === "shapes") penTools.open()
                                else if (["text", "table", "chart"].includes(modelData)) window.editPageItem(modelData, null)
                                else window.selectTool(modelData)
                            }
                        }
                    }

                    TopButton {
                        visible: window.editorWorkspace !== "diagrams"
                        text: qsTr("Diagramma")
                        iconName: "diagram"
                        quiet: true
                        iconOnly: window.logicalWidth < 760 * window.uiScale
                        ToolTip.visible: !window.tabletMode && hovered && iconOnly
                        ToolTip.text: text
                        onClicked: window.editPageItem("diagram", null)
                    }

                    TopButton {
                        visible: window.editorWorkspace !== "writing"
                        text: qsTr("Inserisci immagine")
                        iconName: "image"
                        iconOnly: true
                        quiet: true
                        onClicked: window.workspaceAction("image", "")
                    }

                    TopButton {
                        visible: window.editorWorkspace !== "writing"
                        text: qsTr("Inserisci dalla libreria")
                        iconName: "layers"
                        iconOnly: true
                        quiet: true
                        onClicked: window.workspaceAction("library", "")
                    }

                    Item { Layout.fillWidth: true }

                    TopButton {
                        text: qsTr("Incolla")
                        iconName: "paste"
                        quiet: true
                        iconOnly: window.logicalWidth < 760 * window.uiScale
                        visible: window.inkClipboard.length > 0
                        enabled: window.notebook.canWriteLayer
                        onClicked: window.openInkTransform(window.inkClipboard, true, [])
                    }

                    TopButton {
                        visible: window.notebook.pageItems.length > 0
                        text: window.editingItems ? qsTr("Scrivi") : qsTr("Elementi")
                        iconName: "rename"
                        quiet: true
                        iconOnly: window.logicalWidth < 700 * window.uiScale
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
                            iconOnly: window.logicalWidth < 680 * window.uiScale
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
                        iconOnly: window.logicalWidth < 600 * window.uiScale
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
                            window.setFullPage(true)
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

            RowLayout {
                visible: (window.lassoActive || selectionOverlay.active) && !window.fullPage
                Layout.fillWidth: true
                Layout.leftMargin: window.gutter
                Layout.rightMargin: window.gutter
                spacing: 4 * window.uiScale
                ComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("Lazo: elementi contenuti"), qsTr("Lazo: elementi toccati")]
                    currentIndex: window.lassoMode === "contained" ? 0 : 1
                    onActivated: index => window.lassoMode = index === 0 ? "contained" : "touched"
                    font.pixelSize: 13 * window.uiScale
                }
                ComboBox {
                    model: [qsTr("Italiano"), qsTr("English")]
                    currentIndex: window.notebook.handwritingLanguage === "en" ? 1 : 0
                    onActivated: index => {
                        handwriting.cancel()
                        selectionOverlay.previewTranscript = false
                        window.notebook.setHandwritingLanguage(index === 1 ? "en" : "it")
                    }
                    font.pixelSize: 13 * window.uiScale
                }
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
                onSettingsRequested: readingPreferencesDialog.begin()
            }

            Flickable {
                id: paperViewport
                objectName: "paperViewport"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: window.notebook.hasPageBackground ? window.readingPreference("readingMargins", tabletPreferences.readingMargins) * window.uiScale : window.fullPage ? 0 : 8 * window.uiScale
                Layout.rightMargin: Layout.leftMargin
                Layout.topMargin: Layout.leftMargin
                Layout.bottomMargin: Layout.topMargin
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                contentWidth: Math.max(width, sheet.width * sheet.scale)
                contentHeight: Math.max(height, sheet.height * sheet.scale)
                readonly property bool showScrollBars: !window.tabletMode && !window.displayingSelection && !window.pinLock.locked
                    && !window.standby.sleeping && !window.editorPopupOpen && (interactive || window.pageZoom > 1)
                interactive: !window.tabletMode && !window.displayingSelection && !window.standby.sleeping
                    && window.notebook.hasPageBackground && (window.readingZoom > 1 || window.readingFitWidth)
                ScrollBar.vertical: ScrollBar { policy: paperViewport.showScrollBars ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }
                ScrollBar.horizontal: ScrollBar { policy: paperViewport.showScrollBars ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

                Rectangle {
                    id: readerStatus
                    property bool blocksInk: true
                    parent: paperViewport
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: 8 * window.uiScale
                    z: 7
                    visible: window.notebook.hasPageBackground && !window.pinLock.locked && !window.standby.sleeping
                        && (documentPage.busy || documentPage.error.length > 0)
                    width: Math.min(420 * window.uiScale, paperViewport.width - 16 * window.uiScale)
                    height: readerStatusText.implicitHeight + 12 * window.uiScale
                    color: "white"
                    border.color: documentPage.error.length > 0 ? "#87332c" : window.border
                    border.width: window.line
                    radius: 6 * window.uiScale
                    InkLabel {
                        id: readerStatusText
                        anchors.fill: parent
                        anchors.margins: 6 * window.uiScale
                        text: documentPage.error || qsTr("Caricamento pagina…")
                        color: documentPage.error.length > 0 ? "#87332c" : window.muted
                        font.pixelSize: 13 * window.uiScale
                        wrapMode: Text.WordWrap
                    }
                }

                ReadingGestures {
                    id: readingGestures
                    objectName: "readingGestures"
                    parent: paperViewport
                    anchors.fill: parent
                    z: 5
                    viewport: paperViewport
                    sheet: window.pageSheet
                    zoom: window.pageZoom
                    fitWidth: window.notebook.hasPageBackground && window.readingFitWidth
                    singleFingerGestures: window.notebook.hasPageBackground
                    doubleTapAction: tabletPreferences.doubleTapAction
                    swipeAction: window.readingPreference("swipeAction", tabletPreferences.swipeAction)
                    panSensitivity: tabletPreferences.readingPanSensitivity / 100
                    onFullPageRequested: window.setFullPage(!window.fullPage)
                    onRefreshRequested: window.refreshPage()
                    enabled: tabletPreferences.gestureNavigationEnabled && window.editing && !window.showingSettings && !window.pinLock.locked
                        && !window.standby.sleeping && !window.editorPopupOpen && !window.lassoActive
                        && !settingsView.dialogOpen && !organizer.dialogOpen && !window.notebook.importing
                        && !window.editingItems && !window.displayingSelection && !window.placingBookNote && !inkPage.drawing && !inkPage.touchBlocked
                    onZoomRequested: value => {
                        if (window.notebook.hasPageBackground) window.readingZoom = value
                        else window.canvasZoom = value
                    }
                    onFitPageRequested: {
                        if (window.notebook.hasPageBackground) window.setReadingZoom(1, false)
                        else window.fitCanvasToView()
                    }
                    onPageRequested: delta => window.notebook.goToPage(window.notebook.page + delta)
                    onInteractionStarted: { inkPage.cancelDrawing(); window.pinLock.recordActivity(); window.standby.recordActivity() }
                }

                Rectangle {
                    id: sheet
                    x: (paperViewport.contentWidth - width * scale) / 2
                    y: (paperViewport.contentHeight - height * scale) / 2
                    width: window.notebook.hasPageBackground && window.readingFitWidth ? paperViewport.width
                        : Math.min(paperViewport.width, paperViewport.height * 3 / 4)
                    height: width * 4 / 3
                    scale: window.pageZoom
                    transformOrigin: Item.TopLeft
                    color: "white"
                    border.color: window.border
                    border.width: window.fullPage ? 0 : window.line

                    PageTemplateItem {
                        visible: !window.notebook.hasPageBackground
                        anchors.fill: parent
                        anchors.margins: window.fullPage ? 0 : window.line
                        paper: window.notebook.pageTemplate
                    }

                    PagePreviewItem {
                        id: documentPage
                        visible: window.notebook.hasPageBackground
                        anchors.fill: parent
                        anchors.margins: window.fullPage ? 0 : window.line
                        content: visible ? window.notebook.pageRenderContent : ({})
                        renderScale: window.readingZoom
                        thumbnail: false
                    }

                    PageDecorations {
                        anchors.fill: parent
                        anchors.margins: window.fullPage ? 0 : window.line
                        z: window.editingItems ? 2 : 0
                        uiScale: window.uiScale
                        notebook: window.notebook
                        items: window.displayingSelection ? window.notebook.pageItems.filter(item =>
                            !selectionOverlay.selectedItems.some(selected => selected.id === item.id)) : window.notebook.pageItems
                        editingItems: window.editingItems && !window.editorPopupOpen
                        selectedId: window.selectedPageItem
                        onEditRequested: item => window.editPageItem(item.type, item)
                        onEditCellRequested: (item, row, column) => window.editTableCell(item.id, row, column)
                        onMoveRequested: (id, x, y) => {
                            if (window.notebook.movePageItem(id, x, y))
                                window.selectedPageItem = id
                        }
                    }

                    TapHandler {
                        enabled: window.editing && !window.showingSettings && !window.editorPopupOpen
                            && !window.pinLock.locked && !window.standby.sleeping && !window.displayingSelection && !window.editingItems && !window.lassoActive
                            && !inkPage.drawing && !inkPage.touchBlocked
                        // Pen taps are resolved by InkItem together with stroke input,
                        // so a dot written in an empty cell remains handwriting.
                        acceptedDevices: PointerDevice.TouchScreen
                        onTapped: eventPoint => {
                            const point = inkPage.mapFromItem(null, eventPoint.scenePosition)
                            if (window.placingBookNote) {
                                window.placeBookNote(Qt.point(point.x / inkPage.width, point.y / inkPage.height))
                                return
                            }
                            const noteId = inkPage.noteItemAt(point)
                            if (noteId.length > 0) { window.editBookNote(noteId); return }
                            if (!["pen", "pencil", "marker"].includes(window.penTool)) return
                            const cell = inkPage.tableCellAt(point)
                            if (cell.id) {
                                window.editTableCell(cell.id, cell.row, cell.column)
                                return
                            }
                            const id = inkPage.textItemAt(point)
                            if (id.length > 0) {
                                window.editPageText(id)
                                return
                            }
                            const chartId = inkPage.chartItemAt(point)
                            if (chartId.length > 0) {
                                window.editPageChart(chartId)
                                return
                            }
                            const diagramId = inkPage.diagramItemAt(point)
                            if (diagramId.length > 0)
                                window.editPageDiagram(diagramId)
                        }
                    }

                    InkItem {
                        id: inkPage
                        objectName: "inkPage"
                        inputOverlay: window.contentItem
                        pointPicking: window.placingBookNote
                        markerInkTarget: bookNoteDialog.markerTarget
                        anchors.fill: parent
                        anchors.margins: window.fullPage ? 0 : window.line
                        z: 1
                        enabled: window.editing && !window.editorPopupOpen && !window.editingItems
                                 && !window.pinLock.locked && !window.standby.sleeping && !window.displayingSelection && window.notebook.canWriteLayer
                        acceptsMouse: !window.tabletMode
                        inkColor: window.penTool === "highlighter" ? "#f0bf32" : window.penColor
                        inkTool: window.penTool
                        smartShapes: window.smartShapesEnabled
                        pressureExponent: tabletPreferences.penPressureCurve === "light" ? 0.65 : tabletPreferences.penPressureCurve === "firm" ? 1.5 : 1
                        shapeTolerance: tabletPreferences.shapeTolerance === "strict" ? 0.65 : tabletPreferences.shapeTolerance === "relaxed" ? 1.35 : 1
                        palmRejection: tabletPreferences.palmRejection
                        lassoMode: window.lassoActive
                        inkWidth: window.selectedTool.width * [0.62, 1, 1.60][window.penThickness]
                        erasableFrom: window.notebook.erasableFrom
                        erasableCount: window.notebook.erasableCount
                        strokes: window.displayingSelection ? window.notebook.strokes.filter((stroke, index) =>
                            selectionOverlay.selectedIndexes.indexOf(index) < 0) : window.notebook.strokes
                        pageItems: window.notebook.pageItems

                        property string pageKey: window.notebook.bookId + "/" + window.notebook.pageId
                        property string zoomBookId: ""
                        onPageKeyChanged: {
                            cancelDrawing()
                            selectionOverlay.clear()
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
                                window.canvasZoom = 1
                                window.restoreReadingPreferences()
                            }
                            paperViewport.contentX = 0
                            paperViewport.contentY = 0
                        }
                        onInputActivity: { window.pinLock.recordActivity(); window.standby.recordActivity() }
                        onDrawingChanged: window.standby.drawing = drawing || bookNoteDialog.drawing
                        onStrokeStarted: { window.pinLock.recordActivity(); window.standby.recordActivity() }
                        onTextItemTapped: id => window.editPageText(id)
                        onTableCellTapped: (id, row, column) => window.editTableCell(id, row, column)
                        onChartItemTapped: id => window.editPageChart(id)
                        onDiagramItemTapped: id => window.editPageDiagram(id)
                        onNoteItemTapped: id => window.editBookNote(id)
                        onPagePointPicked: point => window.placeBookNote(point)
                        onStrokeCompleted: stroke => {
                            window.pinLock.recordActivity()
                            window.standby.recordActivity()
                            window.notebook.appendStroke(stroke)
                        }
                        onCorrectedStrokeCompleted: (shape, original) => {
                            window.pinLock.recordActivity()
                            window.standby.recordActivity()
                            window.notebook.appendCorrectedStroke(shape, original)
                        }
                        onLassoCompleted: polygon => {
                            window.pinLock.recordActivity()
                            window.standby.recordActivity()
                            window.useLasso(polygon)
                        }
                        onStrokesErased: indexes => {
                            window.pinLock.recordActivity()
                            window.standby.recordActivity()
                            window.notebook.eraseStrokes(indexes)
                        }
                    }

                    SelectionOverlay {
                        id: selectionOverlay
                        objectName: "selectionOverlay"
                        anchors.fill: parent
                        anchors.margins: window.fullPage ? 0 : window.line
                        z: 3
                        notebook: window.notebook
                        handwriting: window.handwritingEngine
                        keepInk: tabletPreferences.lassoKeepInk
                        uiScale: window.uiScale / sheet.scale
                        toolbarScale: window.uiScale
                        toolbarParent: paperViewport
                        showTouchKeyboard: window.showTouchKeyboard
                        visible: active && !window.lassoActive
                        acceptingInput: !window.lassoActive && !window.editorPopupOpen && !window.standby.sleeping
                        onSelectionClosed: handwriting.cancel()
                        onExtendRequested: operation => {
                            if (changed) return
                            handwriting.cancel()
                            window.lassoOperation = operation
                            window.lassoActive = true
                            window.selectionMessage = operation === "add" ? qsTr("Circonda gli elementi da aggiungere.") : qsTr("Circonda gli elementi da sottrarre.")
                        }
                        onRecognizeRequested: handwriting.recognize(selection.strokes, window.recognitionLanguage)
                        onMoreRequested: { selectionDialog.selection = selection; selectionDialog.open() }
                        onDiagramRecognitionRequested: window.reviewDiagramSelection(selection)
                        onEditTranscriptRequested: text => pageItemDialog.openFor("text", null, text, selection)
                        onTableTranscriptionRequested: {
                            handwriting.cancel()
                            selectionOverlay.previewTranscript = false
                            tableHandwritingDialog.openFor(selection)
                        }
                    }

                    TopButton {
                        parent: paperViewport
                        property bool blocksInk: true
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.margins: 8 * window.uiScale
                        z: 6
                        visible: window.fullPage && tabletPreferences.quickPanelEnabled && !selectionOverlay.active && !window.editorPopupOpen
                        text: qsTr("Controlli rapidi")
                        iconName: "settings"
                        iconOnly: true
                        onClicked: quickControls.open()
                    }

                    Rectangle {
                        objectName: "diagramRecognitionStatus"
                        parent: paperViewport
                        property bool blocksInk: true
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.margins: 8 * window.uiScale
                        z: 6
                        visible: window.diagramRecognitionOffer !== null && window.automaticDiagramAllowed
                        width: Math.min(parent.width - 16 * window.uiScale, offerControls.implicitWidth + 16 * window.uiScale)
                        height: offerControls.implicitHeight + 12 * window.uiScale
                        color: "white"
                        radius: 10 * window.uiScale
                        border.color: window.border
                        RowLayout {
                            id: offerControls
                            anchors.fill: parent
                            anchors.margins: 6 * window.uiScale
                            InkLabel {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                wrapMode: Text.WordWrap
                                text: !window.diagramRecognitionOffer ? "" : window.diagramRecognitionOffer.requiresTarget
                                    ? qsTr("Scegli il diagramma da aggiornare") : qsTr("Forme: %1 · Collegamenti: %2")
                                        .arg(window.diagramRecognitionOffer.nodes).arg(window.diagramRecognitionOffer.edges)
                            }
                            TopButton {
                                objectName: "reviewHanddrawnDiagram"
                                text: qsTr("Rivedi")
                                filled: true
                                onClicked: window.reviewDiagramSelection(null)
                            }
                            TopButton {
                                objectName: "dismissDiagramRecognition"
                                text: qsTr("Chiudi")
                                iconName: "close"
                                iconOnly: true
                                quiet: true
                                onClicked: window.dismissDiagramRecognition()
                            }
                        }
                    }

                    Rectangle {
                        objectName: "bookNotePlacementHint"
                        property bool blocksInk: true
                        parent: paperViewport
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 8 * window.uiScale
                        z: 7
                        visible: window.placingBookNote
                        height: notePlacementControls.implicitHeight + 12 * window.uiScale
                        color: "#fff2a6"; border.color: "#6a6041"; radius: 10 * window.uiScale
                        RowLayout {
                            id: notePlacementControls
                            anchors.fill: parent; anchors.margins: 6 * window.uiScale
                            InkLabel { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("Tocca il punto della pagina in cui aggiungere il post-it.") }
                            TopButton { text: qsTr("Annulla"); onClicked: window.placingBookNote = false }
                        }
                    }

                    TopButton {
                        objectName: "fitCanvasButton"
                        property bool blocksInk: true
                        parent: paperViewport
                        anchors.top: parent.top
                        anchors.right: window.fullPage ? collapsedTools.left : parent.right
                        anchors.margins: 8 * window.uiScale
                        z: 6
                        visible: !window.notebook.hasPageBackground && window.canvasZoom > 1.01
                            && !selectionOverlay.active && !window.editorPopupOpen
                        enabled: !inkPage.drawing && !readingGestures.active && !window.placingBookNote
                        text: qsTr("%1%").arg(Math.round(window.canvasZoom * 100))
                        Accessible.name: qsTr("Adatta il foglio alla vista")
                        ToolTip.visible: !window.tabletMode && hovered
                        ToolTip.text: qsTr("Adatta il foglio alla vista")
                        onClicked: window.fitCanvasToView()
                    }

                    TopButton {
                        id: collapsedTools
                        objectName: "collapsedTools"
                        parent: paperViewport
                        property bool blocksInk: true
                        property bool refreshed: false
                        anchors.top: parent.top
                        anchors.right: parent.right
                        anchors.margins: 8 * window.uiScale
                        z: 6
                        visible: window.fullPage && !selectionOverlay.active && !window.editorPopupOpen
                        text: qsTr("Mostra strumenti")
                        iconName: "collapse"
                        iconOnly: true
                        uiScale: window.uiScale
                        onPressed: refreshed = false
                        onPressAndHold: { if (window.canRefreshPage) { window.refreshPage(); refreshed = true } }
                        onClicked: if (!refreshed) window.setFullPage(false)
                    }
                }
            }

            InkLabel {
                visible: window.tabletMode && !window.fullPage && !inkPage.penAvailable
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 6 * window.uiScale
                text: qsTr("Marker non rilevato")
                color: window.muted
                font.pixelSize: 13 * window.uiScale
            }

            Rectangle {
                visible: !window.fullPage
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
                        iconOnly: window.logicalWidth < 520 * window.uiScale
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
                        iconOnly: window.logicalWidth < 520 * window.uiScale
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
                            window.setFullPage(false)
                        }
                    }
                }
            }
        }

        InkLabel {
            visible: window.documentExchange.status.length > 0 && !window.showingSettings && !window.fullPage
            Layout.fillWidth: true
            Layout.leftMargin: window.gutter
            Layout.rightMargin: window.gutter
            text: window.documentExchange.status
            color: window.muted
            wrapMode: Text.Wrap
        }

        InkLabel {
            visible: text.length > 0 && !nameDialog.visible && !window.showingSettings && !window.fullPage
            Layout.fillWidth: true
            Layout.margins: 12 * window.uiScale
            text: window.notebook.error
            color: "#8c2d27"
            wrapMode: Text.Wrap
        }
    }

    DocumentDropArea {
        id: documentDropArea
        parent: orientedScene
        folderLabel: organizer.folderPath(organizer.destinationFolderId)
        anchors.fill: parent
        z: 1
        exchange: window.documentExchange
        folderId: organizer.destinationFolderId
        enabled: window.documentExchange.available && !window.pinLock.locked && !window.documentExchange.importPaused
        uiScale: window.uiScale
    }

    Rectangle {
        objectName: "standbyOverlay"
        parent: orientedScene
        anchors.fill: parent
        z: 20
        visible: window.standby.sleeping
        color: "white"
        MouseArea { anchors.fill: parent; onClicked: window.standby.wake() }
        ColumnLayout {
            anchors.centerIn: parent
            spacing: 20 * window.uiScale
            InkLabel { text: qsTr("Quaderno in pausa"); font.pixelSize: 24 * window.uiScale }
            TopButton { text: qsTr("Riprendi"); Layout.alignment: Qt.AlignHCenter; onClicked: window.standby.wake() }
        }
    }

    PinScreen {
        objectName: "pinScreen"
        parent: orientedScene
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
        preferredDestination: window.notebook.hasPageBackground ? tabletPreferences.appleBookDestination
            : organizer.section === "tables" ? tabletPreferences.appleTableDestination
            : organizer.section === "charts" ? tabletPreferences.appleChartDestination : tabletPreferences.appleWritingDestination
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
        showTouchKeyboard: window.showTouchKeyboard
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
        showTouchKeyboard: window.showTouchKeyboard
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
        objectName: "tableChartDialog"
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
        width: Math.min(410 * window.uiScale, window.logicalWidth - 24 * window.uiScale)
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
        readonly property bool hasPreview: handwriting.result.length > 0
        title: qsTr("Selezione")
        anchors.centerIn: Overlay.overlay
        width: Math.min(410 * window.uiScale, window.logicalWidth - 24 * window.uiScale)
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
                text: qsTr("Tratti: %1 · Elementi: %2").arg(selectionDialog.selection ? selectionDialog.selection.strokes.length : 0)
                    .arg(selectionDialog.selection ? (selectionDialog.selection.items || []).length : 0)
            }
            ComboBox {
                Layout.fillWidth: true
                model: [qsTr("Riconoscimento: italiano"), qsTr("Riconoscimento: inglese")]
                currentIndex: window.notebook.handwritingLanguage === "en" ? 1 : 0
                onActivated: index => {
                    handwriting.cancel()
                    selectionOverlay.previewTranscript = false
                    window.notebook.setHandwritingLanguage(index === 1 ? "en" : "it")
                }
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
                        selectionOverlay.begin(selection)
                    }
                }
                TopButton {
                    text: qsTr("Copia tratti")
                    enabled: selectionDialog.actionsEnabled && selectionDialog.selection.strokes.length > 0
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
                enabled: selectionDialog.actionsEnabled && selectionDialog.selection.strokes.length > 0
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
                      : qsTr("Trascrivi nella lingua del documento")
                iconName: "text"
                visible: handwriting.available && (!window.tabletMode || handwriting.error.length > 0)
                enabled: !handwriting.busy && !!selectionDialog.selection
                onClicked: handwriting.recognize(selectionDialog.selection.strokes, window.notebook.handwritingLanguage)
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
                Layout.fillWidth: true
                text: qsTr("Elimina selezione")
                enabled: selectionDialog.actionsEnabled
                onClicked: {
                    if (window.notebook.removeSelection(selectionDialog.selection)) {
                        selectionDialog.close()
                        selectionOverlay.clear()
                    }
                }
            }
            TopButton {
                Layout.fillWidth: true
                text: qsTr("Ripristina scrittura originale")
                visible: selectionDialog.selection && (selectionDialog.selection.items || []).length === 1
                    && !!selectionDialog.selection.items[0].handwritingSource
                onClicked: {
                    if (window.notebook.restoreOriginalHandwriting(selectionDialog.selection.items[0].id)) {
                        selectionDialog.close()
                        selectionOverlay.clear()
                    }
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
    ChartRecognizer { id: workspaceChartExporter }

    DiagramEditor {
        id: diagramEditor
        objectName: "diagramEditor"
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
        onImportRequested: diagramOpenPicker.begin(false)
    }

    FileDialog {
        id: diagramOpenPicker
        objectName: "diagramOpenPicker"
        property string sourcePage: ""
        property bool createDraft: false
        title: qsTr("Importa diagramma draw.io")
        nameFilters: [qsTr("Diagramma draw.io (*.drawio)"), qsTr("Diagramma XML (*.xml)")]
        function begin(direct: bool) {
            sourcePage = window.notebook.bookId + "/" + window.notebook.pageId
            createDraft = direct
            open()
        }
        onAccepted: {
            const key = sourcePage
            sourcePage = ""
            if (!window.canImportOnSheet(key)) return
            const result = diagramExport.load(selectedFile)
            if (createDraft) window.editPageItem("diagram", null)
            if (diagramEditor.visible) diagramEditor.importDiagram(result)
        }
        onRejected: sourcePage = ""
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

    Connections {
        target: window.notebook
        function onImportChanged() {
            if (!window.notebook.importing && window.cloudAccountsModel) {
                for (const file of window.pendingCloudDownloads) window.cloudAccountsModel.discardDownload(file)
                window.pendingCloudDownloads = []
            }
        }
        function onStrokesChanged() { window.diagramRecognitionDismissed = false; window.scheduleDiagramRecognition() }
        function onDocumentChanged() {
            if (window.placingBookNote && window.bookNotePlacementContext !== window.notebook.bookId + "/" + window.notebook.pageId)
                window.placingBookNote = false
            if (window.editorWorkspaceBookId !== window.notebook.bookId) window.selectEditorWorkspace("")
            window.updateDiagramRecognitionContext()
        }
        function onPageContentChanged() { window.scheduleDiagramRecognition() }
        function onLayersChanged() {
            if (!window.notebook.canWriteLayer) window.placingBookNote = false
            window.updateDiagramRecognitionContext()
            selectionOverlay.clear()
        }
        function onDocumentImported(bookId) {
            window.importedBookId = bookId
            if (!window.editing && !window.showingSettings && !window.editorPopupOpen
                && !window.pinLock.locked && !window.standby.sleeping) {
                window.showBook(bookId)
                organizer.section = window.notebook.hasPageBackground ? "books" : "writing"
                window.importedBookId = ""
            }
        }
    }

    Rectangle {
        id: importProgressDialog
        objectName: "importProgressDialog"
        parent: orientedScene
        property bool blocksInk: true
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 8 * window.uiScale
        z: 8
        visible: (window.notebook.importing || window.importedBookId.length > 0)
            && !window.pinLock.locked && !window.standby.sleeping
        width: Math.min(420 * window.uiScale, window.logicalWidth - 24 * window.uiScale)
        height: importControls.implicitHeight + 20 * window.uiScale
        color: "white"
        border.color: window.border
        border.width: window.line
        radius: 8 * window.uiScale
        ColumnLayout {
            id: importControls
            anchors.fill: parent
            anchors.margins: 10 * window.uiScale
            InkLabel {
                text: window.notebook.importing ? window.notebook.importStatus : qsTr("Documento importato")
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            ProgressBar { visible: window.notebook.importing; Layout.fillWidth: true; from: 0; to: 1; value: window.notebook.importProgress }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                TopButton { text: qsTr("Annulla"); visible: window.notebook.importing; onClicked: window.notebook.cancelDocumentImport() }
                TopButton { text: qsTr("Chiudi"); visible: !window.notebook.importing; onClicked: window.importedBookId = "" }
                TopButton {
                    text: qsTr("Apri documento")
                    visible: !window.notebook.importing
                    enabled: !inkPage.drawing && !window.editorPopupOpen
                    onClicked: { window.showBook(window.importedBookId); window.importedBookId = "" }
                }
            }
        }
    }

    FileDialog {
        id: documentPicker
        title: qsTr("Importa documento")
        onAccepted: {
            if (!window.pinLock.locked)
                window.notebook.startDocumentImport(selectedFile, organizer.destinationFolderId)
        }
    }

    Dialog {
        id: appleBooksDialog
        property var pendingFolder: null

        title: qsTr("Importa da Apple Books")
        anchors.centerIn: Overlay.overlay
        width: Math.min(470 * window.uiScale, window.logicalWidth - 24 * window.uiScale)
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
        objectName: "csvOpenPicker"
        property string sourcePage: ""
        property bool createDraft: false
        title: qsTr("Importa dati CSV")
        nameFilters: [qsTr("CSV e TSV (*.csv *.CSV *.tsv *.TSV)")]
        function begin(direct: bool) {
            sourcePage = window.notebook.bookId + "/" + window.notebook.pageId
            createDraft = direct
            open()
        }
        Component.onCompleted: {
            if (window.epaperGeometry.width > 0)
                currentFolder = "file:///home/root"
        }
        onAccepted: {
            const key = sourcePage
            sourcePage = ""
            if (!window.canImportOnSheet(key)) return
            const result = window.notebook.readTableCsv(selectedFile)
            if (createDraft) window.editPageItem("table", null)
            if (pageItemDialog.visible && pageItemDialog.editingTable && !pageItemDialog.existing)
                pageItemDialog.importCsv(result)
        }
        onRejected: sourcePage = ""
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
        width: Math.min(430 * window.uiScale, window.logicalWidth - 24 * window.uiScale)
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
        readonly property var entries: window.notebook.libraryItems.filter(item => item.type === kind && !item.empty)
        title: qsTr("Inserisci dalla libreria")
        anchors.centerIn: Overlay.overlay
        width: Math.min(490 * window.uiScale, window.logicalWidth - 24 * window.uiScale)
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
                TopButton { text: qsTr("Diagrammi"); filled: insertLibraryItemDialog.kind === "diagram"; onClicked: insertLibraryItemDialog.kind = "diagram" }
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

    MarkdownEditor {
        id: markdownEditor
        notebook: window.notebook
        uiScale: window.uiScale
        tabletMode: window.tabletMode
        showTouchKeyboard: window.showTouchKeyboard
        folderId: organizer.destinationFolderId
        onEditingStarted: inkPage.cancelDrawing()
        onSaved: window.showCurrentBook()
        onApplicationCloseApproved: window.close()
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
        width: Math.min(440 * window.uiScale, window.logicalWidth - 24 * window.uiScale)
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
        width: Math.min(500 * window.uiScale, window.logicalWidth - 24 * window.uiScale)
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

    Timer {
        id: readingZoomSave
        interval: 700
        property string bookId: ""
        onTriggered: {
            if (bookId === window.notebook.bookId && window.editing && window.notebook.hasPageBackground
                && !window.pinLock.locked && !window.standby.sleeping)
                window.notebook.setReadingPreferences({readingZoom: Math.round(window.readingZoom * 100)})
        }
    }
    Connections {
        target: window.notebook
        function onLocalReadingPreferencesChanged() {
            readingZoomSave.stop()
            const zoom = window.readingPreference("readingZoom", tabletPreferences.readingZoom) / 100
            if (Math.abs(window.readingZoom - zoom) > 0.011) window.restoreReadingPreferences()
            Qt.callLater(window.applyPreferredOrientation)
        }
    }
    LibraryMaintenance {
        id: libraryMaintenance
        notebook: window.notebook
        backupSchedule: tabletPreferences.backupSchedule
        backupRetention: tabletPreferences.backupRetention
        cacheLimitMiB: tabletPreferences.cacheLimitMiB
        allowed: !window.pinLock.locked && !window.standby.sleeping && !window.notebook.importing
            && !inkPage.drawing && !selectionOverlay.active && !window.editorPopupOpen
    }
    SyncAutomation {
        id: syncAutomation
        preferences: tabletPreferences
        wireless: window.wireless
        wifi: window.wifi
        deviceInfo: window.deviceInfo
        notebook: window.notebook
        allowed: window.backgroundWorkAllowed
    }
    QuickControlsPopup {
        id: quickControls
        preferences: tabletPreferences
        deviceInfo: window.deviceInfo
        epaper: window.epaperRefresh
        currentOrientation: window.appliedOrientation
        onRotationRequested: angle => {
            tabletPreferences.automaticOrientation = false
            if (window.editing && !window.showingSettings && window.notebook.hasPageBackground
                && window.readingPreference("readingOrientation", tabletPreferences.readingOrientation) >= 0)
                window.notebook.setReadingPreferences({readingOrientation: angle})
            else tabletPreferences.orientation = angle
        }
        tabletMode: window.tabletMode
        uiScale: window.uiScale
        onSettingsRequested: category => { window.showSettings(); settingsView.selectCategory(category) }
        onSleepRequested: window.standby.suspend()
        onRefreshRequested: Qt.callLater(function() { window.epaperRefresh.request() })
    }
    ReadingPreferencesDialog {
        id: readingPreferencesDialog
        tabletMode: window.tabletMode
        notebook: window.notebook
        preferences: tabletPreferences
        uiScale: window.uiScale
    }
    PortableDocuments {
        id: portableDocuments
        notebook: window.notebook
        locked: window.pinLock.locked || window.standby.sleeping
    }
    Loader {
        id: cloudDialogLoader
        active: window.cloudAccountsModel !== null
        sourceComponent: Component {
            CloudAccountsDialog {
                cloud: window.cloudAccountsModel
                localError: portableDocuments.error
                canUploadCurrent: window.notebook.bookId.length > 0 && window.notebook.pageCount > 0 && !window.notebook.importing
                uiScale: window.uiScale
                tabletMode: window.tabletMode
                showTouchKeyboard: window.showTouchKeyboard
                onUploadRequested: cloudUploadPicker.open()
                onUploadCurrentRequested: {
                    const url = portableDocuments.exportCurrent()
                    if (url.toString().length) cloud.uploadFile(url, window.notebook.title + ".quaderno")
                }
            }
        }
    }
    Connections {
        target: window.cloudAccountsModel
        function onDownloaded(file, name) {
            if (!window.pinLock.locked && !window.standby.sleeping) {
                if (cloudDialogLoader.item) cloudDialogLoader.item.close()
                cloudImportDialog.begin(file, name, organizer.destinationFolderId)
            }
        }
    }
    CloudImportDialog {
        id: cloudImportDialog
        notebook: window.notebook
        portable: portableDocuments
        onReleaseDownload: (file, afterImport) => {
            if (!window.cloudAccountsModel || !file.toString().length) return
            if (afterImport && window.notebook.importing)
                window.pendingCloudDownloads = window.pendingCloudDownloads.concat([file])
            else window.cloudAccountsModel.discardDownload(file)
        }
        onLibraryRequested: window.showLibrary()
        uiScale: window.uiScale
        tabletMode: window.tabletMode
        onImported: window.showCurrentBook()
    }
    FileDialog {
        id: cloudUploadPicker
        title: qsTr("Carica una copia nel cloud")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("Documenti (*.pdf *.epub *.md *.csv *.quaderno *.png *.jpg *.jpeg *.webp)")]
        onAccepted: if (window.cloudAccountsModel && !window.pinLock.locked && !window.standby.sleeping)
            window.cloudAccountsModel.uploadFile(selectedFile)
    }

    WirelessSyncDialog {
        id: wirelessDialog
        dim: !window.tabletMode
        wireless: window.wireless
        notebook: window.notebook
        uiScale: window.uiScale
        preferences: tabletPreferences
        automationStatus: syncAutomation.status
        onAutomaticSyncRequested: syncAutomation.syncNow()
        syncLabel: window.syncStateLabel(window.currentSync.state || "")
        onResumeOpened: window.showCurrentBook()
    }

    BookNoteDialog {
        id: bookNoteDialog
        notebook: window.notebook
        uiScale: window.uiScale
        tabletMode: window.tabletMode
        showTouchKeyboard: window.showTouchKeyboard
        editingAllowed: window.editing && !window.showingSettings && !window.pinLock.locked && !window.standby.sleeping
        onActivity: { window.pinLock.recordActivity(); window.standby.recordActivity() }
        onDrawingChanged: window.standby.drawing = inkPage.drawing || drawing
        onApplicationCloseRequested: window.close()
    }

    DiagramHandwritingDialog {
        id: diagramHandwritingDialog
        notebook: window.notebook
        uiScale: window.uiScale
        tabletMode: window.tabletMode
        showTouchKeyboard: window.showTouchKeyboard
        defaultKeepInk: tabletPreferences.lassoKeepInk
        recognitionLanguage: window.recognitionLanguage
        preferredWords: tabletPreferences.ocrWordList
        preferredCorrections: tabletPreferences.ocrCorrections
        onDiagramApplied: {
            selectionOverlay.clear()
            window.dismissDiagramRecognition()
        }
    }

    TableHandwritingDialog {
        id: tableHandwritingDialog
        defaultKeepInk: tabletPreferences.lassoKeepInk
        recognitionLanguage: window.recognitionLanguage
        numericRecognition: tabletPreferences.ocrNumericMode
        preferredWords: tabletPreferences.ocrWordList
        preferredCorrections: tabletPreferences.ocrCorrections
        notebook: window.notebook
        uiScale: window.uiScale
        tabletMode: window.tabletMode
        showTouchKeyboard: window.showTouchKeyboard
        onCellsApplied: selectionOverlay.clear()
    }

    PageItemDialog {
        id: pageItemDialog
        objectName: "pageItemDialog"
        showTouchKeyboard: window.showTouchKeyboard
        uiScale: window.uiScale
        tabletMode: window.tabletMode
        notebook: window.notebook
        onCsvImportRequested: csvOpenPicker.begin(false)
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
        onRestoreHandwritingRequested: id => {
            if (window.notebook.restoreOriginalHandwriting(id)) {
                pageItemDialog.close()
                selectionOverlay.clear()
            } else pageItemDialog.errorText = window.notebook.error
        }
        onReplaceTranscriptRequested: (selection, text) => {
            if (window.notebook.replaceHandwriting(selection, text, tabletPreferences.lassoKeepInk)) {
                selectionOverlay.clear()
                pageItemDialog.close()
            }
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

        anchors.centerIn: Overlay.overlay
        width: Math.min(window.logicalWidth - 32 * window.uiScale, 420 * window.uiScale)
        padding: 20 * window.uiScale
        modal: true
        dim: !window.tabletMode
        title: renaming ? qsTr("Rinomina quaderno") : createType === "table" ? qsTr("Nuova tabella")
            : createType === "diagram" ? qsTr("Nuovo diagramma") : qsTr("Nuovo grafico")
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
            if (createType === "diagram") {
                if (window.notebook.createDiagramBook(name, organizer.destinationFolderId)) {
                    close()
                    window.showCurrentBook()
                }
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
