import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

ColumnLayout {
    id: organizer

    required property Notebook notebook
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode

    property bool sidebar: false
    property string section: "writing"
    property string viewMode: "files"
    property string folderId: ""
    property string selectedTag: ""
    property bool sortByTitle: false

    readonly property string destinationFolderId: viewMode === "files" ? folderId : ""
    readonly property bool dialogOpen: actions.visible
    readonly property color ink: "#1d1d1f"
    readonly property color muted: "#525258"
    readonly property color blue: "#0757ad"
    readonly property color border: "#c9c9ce"
    readonly property color surface: "#f1f1f4"
    readonly property color selectedSurface: "#e2ebf7"
    readonly property var activeFolder: notebook.folders.find(folder => folder.id === folderId) || null
    readonly property var childFolders: viewMode === "files"
        ? notebook.folders.filter(folder => folder.parentId === folderId) : []
    readonly property var tags: notebook.knownTags
    readonly property bool showsItems: section === "charts" || section === "tables"
    readonly property var sections: [
        { key: "books", label: qsTr("Libri"), icon: "notebook" },
        { key: "writing", label: qsTr("Scrittura"), icon: "pen" },
        { key: "charts", label: qsTr("Grafici"), icon: "chart" },
        { key: "tables", label: qsTr("Tabelle"), icon: "table" }
    ]

    spacing: (sidebar ? 12 : 7) * uiScale

    function filteredBooks(query: string): var {
        const text = query.trim().toLocaleLowerCase()
        const tag = selectedTag.toLocaleLowerCase()
        const parents = new Map()
        for (const folder of notebook.folders)
            parents.set(folder.id, folder.parentId)
        const entries = showsItems && viewMode !== "trash" ? notebook.libraryItems : notebook.books
        const results = entries.filter(book => {
            if (Boolean(book.trashed) !== (viewMode === "trash"))
                return false
            if (viewMode !== "trash") {
                if (showsItems && book.type !== (section === "charts" ? "chart" : "table"))
                    return false
                if (!showsItems && Boolean(book.document) !== (section === "books"))
                    return false
            }
            if (viewMode === "favorites" && !book.favorite)
                return false
            if (viewMode === "tags" && !book.tags.some(value => value.toLocaleLowerCase() === tag)
                                    && !book.pageTags.some(value => value.toLocaleLowerCase() === tag))
                return false
            if (viewMode === "files") {
                let id = book.folderId
                if (text.length > 0) {
                    for (let depth = 0; id !== folderId && parents.has(id) && depth < parents.size; ++depth)
                        id = parents.get(id)
                }
                if (id !== folderId)
                    return false
            }
            return text.length === 0 || book.title.toLocaleLowerCase().includes(text)
                || (book.bookTitle || "").toLocaleLowerCase().includes(text)
                || book.tags.some(value => value.toLocaleLowerCase().includes(text))
                || book.pageTags.some(value => value.toLocaleLowerCase().includes(text))
        })
        return sortByTitle ? results.sort((a, b) => a.title.localeCompare(b.title)) : results
    }

    function folderPath(id: string): string {
        const names = []
        const folders = notebook.folders
        for (let depth = 0; id.length > 0 && depth < folders.length; ++depth) {
            const folder = folders.find(entry => entry.id === id)
            if (!folder)
                break
            names.unshift(folder.title)
            id = folder.parentId
        }
        return names.length === 0 ? qsTr("Libreria") : names.join(" / ")
    }

    function closeDialogs() {
        actions.close()
    }

    function openBookActions(book: var) {
        actions.bookId = book.id
        actions.folder = null
        actions.openStep("book")
    }

    function openFolderActions(folder: var) {
        actions.folder = folder
        actions.bookId = ""
        actions.openStep("folder")
    }

    function openLibraryActions(step: string) {
        actions.bookId = ""
        actions.folder = null
        actions.openStep(step)
    }

    component ModeButton: PaperButton {
        uiScale: organizer.uiScale
        hoverEnabled: !organizer.tabletMode
        Layout.minimumWidth: 0
        leftPadding: 8 * uiScale
        rightPadding: leftPadding
    }

    component OrganizerLabel: Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        color: organizer.ink
        font.pixelSize: 14 * organizer.uiScale
    }

    component ModeFilter: ModeButton {
        id: filter
        required property string mode
        readonly property bool chosen: organizer.viewMode === mode

        alignLeft: organizer.sidebar
        quiet: true
        Layout.fillWidth: organizer.sidebar
        leftPadding: (organizer.sidebar ? 11 : 7) * organizer.uiScale
        rightPadding: leftPadding
        background: Rectangle {
            color: filter.chosen ? organizer.selectedSurface : filter.down ? "#dddde2"
                   : filter.hovered ? "#ebebef" : "transparent"
            border.color: organizer.blue
            border.width: filter.visualFocus ? Math.max(2, organizer.uiScale) : 0
            radius: (organizer.sidebar ? 8 : 6) * organizer.uiScale
        }

        onClicked: {
            if (mode === "tags") {
                organizer.openLibraryActions("pickTag")
                return
            }
            if (mode === "files" && organizer.viewMode === "files")
                organizer.folderId = ""
            organizer.viewMode = mode
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: organizer.sidebar ? 1 : (organizer.width < 480 * organizer.uiScale ? 2 : 4)
        columnSpacing: 4 * organizer.uiScale
        rowSpacing: 3 * organizer.uiScale

        Repeater {
            model: organizer.sections

            ModeButton {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                text: modelData.label
                iconName: modelData.icon
                alignLeft: organizer.sidebar
                quiet: organizer.section !== modelData.key || organizer.viewMode === "trash"
                filled: organizer.section === modelData.key && organizer.viewMode !== "trash"
                onClicked: {
                    organizer.section = modelData.key
                    if (organizer.viewMode === "trash")
                        organizer.viewMode = "files"
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 7 * organizer.uiScale

        Control {
            Layout.fillWidth: organizer.sidebar
            Layout.minimumWidth: 0
            padding: organizer.sidebar ? 0 : 3 * organizer.uiScale
            background: Rectangle {
                color: organizer.sidebar ? "transparent" : organizer.surface
                radius: 9 * organizer.uiScale
            }
            contentItem: GridLayout {
                columns: organizer.sidebar ? 1 : 4
                rowSpacing: 3 * organizer.uiScale
                columnSpacing: 1 * organizer.uiScale

                ModeFilter { mode: "files"; text: qsTr("File") }
                ModeFilter { mode: "favorites"; text: qsTr("Preferiti") }
                ModeFilter { mode: "tags"; text: qsTr("Tag") }
                ModeFilter { mode: "trash"; text: qsTr("Cestino") }
            }
        }

        Item { visible: !organizer.sidebar; Layout.fillWidth: true }

        RowLayout {
            visible: !organizer.sidebar
            spacing: 4 * organizer.uiScale

            ModeButton {
                visible: organizer.viewMode === "files"
                text: qsTr("Nuova cartella")
                iconName: "folder"
                iconOnly: organizer.width < 500 * organizer.uiScale
                onClicked: organizer.openLibraryActions("newFolder")
            }

            ModeButton {
                text: qsTr("Ordina")
                iconName: "more"
                iconOnly: true
                onClicked: organizer.openLibraryActions("sort")
            }
        }
    }

    RowLayout {
        visible: organizer.viewMode === "tags" || (organizer.viewMode === "files" && organizer.activeFolder !== null)
        Layout.fillWidth: true
        spacing: 6 * organizer.uiScale

        ModeButton {
            visible: organizer.viewMode === "files"
            text: qsTr("Cartella superiore")
            iconName: "left"
            iconOnly: true
            quiet: true
            onClicked: organizer.folderId = organizer.activeFolder ? organizer.activeFolder.parentId : ""
        }

        OrganizerLabel {
            text: organizer.viewMode === "tags" ? organizer.selectedTag : organizer.folderPath(organizer.folderId)
            elide: Text.ElideLeft
            font.weight: Font.DemiBold
        }
    }

    ListView {
        id: folders

        visible: count > 0
        Layout.fillWidth: true
        Layout.fillHeight: organizer.sidebar
        Layout.minimumHeight: organizer.sidebar ? 46 * organizer.uiScale : 0
        Layout.preferredHeight: (organizer.sidebar ? Math.min(count, 5) * 47 + 4 : 48) * organizer.uiScale
        orientation: organizer.sidebar ? ListView.Vertical : ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        spacing: 3 * organizer.uiScale
        clip: true
        model: organizer.childFolders
        ScrollBar.horizontal: ScrollBar { policy: organizer.sidebar ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded }
        ScrollBar.vertical: ScrollBar { policy: organizer.sidebar ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

        delegate: Control {
            id: folderRow

            required property var modelData
            width: organizer.sidebar ? folders.width : implicitWidth
            rightPadding: organizer.sidebar && folders.contentHeight > folders.height ? 5 * organizer.uiScale : 0
            background: Rectangle {
                color: organizer.sidebar ? "transparent" : "#ffffff"
                border.color: "#dbdbe0"
                border.width: organizer.sidebar ? 0 : Math.max(1, Math.round(organizer.uiScale))
                radius: 8 * organizer.uiScale
            }

            contentItem: RowLayout {
                spacing: 0
                ModeButton {
                    text: folderRow.modelData.title
                    iconName: "folder"
                    alignLeft: true
                    quiet: true
                    Layout.fillWidth: organizer.sidebar
                    Layout.preferredWidth: Math.min(implicitWidth, 176 * organizer.uiScale)
                    onClicked: organizer.folderId = folderRow.modelData.id
                }

                ModeButton {
                    text: qsTr("Azioni cartella %1").arg(folderRow.modelData.title)
                    iconName: "more"
                    iconOnly: true
                    quiet: true
                    Layout.minimumWidth: 42 * organizer.uiScale
                    onClicked: organizer.openFolderActions(folderRow.modelData)
                }
            }
        }
    }

    Item {
        visible: organizer.sidebar && folders.count === 0
        Layout.fillHeight: true
    }

    RowLayout {
        visible: organizer.sidebar
        Layout.fillWidth: true
        spacing: 4 * organizer.uiScale

        ModeButton {
            visible: organizer.viewMode === "files"
            Layout.fillWidth: true
            text: qsTr("Nuova cartella")
            iconName: "folder"
            alignLeft: true
            onClicked: organizer.openLibraryActions("newFolder")
        }
        Item { visible: organizer.viewMode !== "files"; Layout.fillWidth: true }
        ModeButton {
            text: qsTr("Ordina")
            iconName: "more"
            iconOnly: true
            Layout.minimumWidth: 42 * organizer.uiScale
            onClicked: organizer.openLibraryActions("sort")
        }
    }

    Dialog {
        id: actions

        property string step: ""
        property string bookId: ""
        property var folder: null
        property var tagDraft: []
        property string errorText: ""

        readonly property var book: organizer.notebook.books.find(entry => entry.id === bookId) || null
        readonly property bool folderNameStep: step === "newFolder" || step === "renameFolder"
        readonly property bool inputStep: folderNameStep || step === "bookTags"
        readonly property bool confirmationStep: step === "purgeBook" || step === "removeFolder"
        readonly property var destinations: {
            const sorted = organizer.notebook.folders.map(folder => ({
                id: folder.id,
                title: organizer.folderPath(folder.id)
            })).sort((a, b) => a.title.localeCompare(b.title))
            return [{ id: "", title: qsTr("Libreria") }, ...sorted]
        }

        title: ({
            book: book ? book.title : qsTr("Azioni del quaderno"),
            folder: folder ? folder.title : qsTr("Azioni cartella"),
            newFolder: qsTr("Nuova cartella"),
            renameFolder: qsTr("Rinomina cartella"),
            moveBook: qsTr("Sposta quaderno"),
            bookTags: qsTr("Tag del quaderno"),
            pickTag: qsTr("Filtra per tag"),
            sort: qsTr("Ordina quaderni"),
            purgeBook: qsTr("Elimina definitivamente"),
            removeFolder: qsTr("Elimina cartella")
        })[step] || ""
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(460 * organizer.uiScale, (parent ? parent.width : 460 * organizer.uiScale) - 24 * organizer.uiScale)
        padding: 16 * organizer.uiScale
        spacing: 10 * organizer.uiScale
        modal: true
        dim: !organizer.tabletMode
        focus: true
        closePolicy: Popup.CloseOnEscape

        function focusInput() {
            if (!visible || !inputStep)
                return
            input.forceActiveFocus()
            input.selectAll()
        }

        function openStep(next: string) {
            step = next
            errorText = ""
            input.text = next === "renameFolder" && folder ? folder.title : ""
            if (next === "bookTags")
                tagDraft = book ? Array.from(book.tags) : []
            scroll.contentItem.contentY = 0
            if (!visible)
                open()
            else if (inputStep)
                Qt.callLater(focusInput)
        }

        function addTag() {
            const name = input.text.trim()
            if (name.length === 0)
                return true
            if (tagDraft.some(tag => tag.toLocaleLowerCase() === name.toLocaleLowerCase())) {
                input.clear()
                return true
            }
            if (tagDraft.length >= 20) {
                errorText = qsTr("Puoi assegnare fino a 20 tag a un quaderno.")
                return false
            }
            tagDraft = [...tagDraft, name]
            input.clear()
            errorText = ""
            return true
        }

        function saveInput() {
            const name = input.text.trim()
            if (step === "newFolder" && name.length > 0)
                finish(organizer.notebook.createFolder(name, organizer.folderId))
            else if (step === "renameFolder" && folder && name.length > 0)
                finish(organizer.notebook.renameFolder(folder.id, name))
            else if (step === "bookTags" && book && addTag())
                finish(organizer.notebook.setBookTags(book.id, tagDraft))
        }

        function finish(saved: bool) {
            if (saved)
                close()
            else
                errorText = organizer.notebook.error
        }

        onOpened: Qt.callLater(focusInput)

        onClosed: {
            step = ""
            bookId = ""
            folder = null
            tagDraft = []
            input.clear()
        }

        background: Rectangle {
            color: "white"
            border.color: "#dedee4"
            radius: 15 * organizer.uiScale
        }

        Overlay.modal: Rectangle { color: "#590d1726" }

        header: OrganizerLabel {
            text: actions.title
            font.pixelSize: 21 * organizer.uiScale
            font.weight: Font.DemiBold
            padding: actions.padding
            bottomPadding: 0
            elide: Text.ElideRight
        }

        component Choice: PaperButton {
            uiScale: organizer.uiScale
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            alignLeft: true
        }

        contentItem: ScrollView {
            id: scroll

            clip: true
            contentWidth: availableWidth
            contentHeight: fields.implicitHeight
            implicitHeight: Math.min(fields.implicitHeight, Math.max(110 * organizer.uiScale,
                                (actions.parent ? actions.parent.height : 480) - 146 * organizer.uiScale))
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: fields

                width: scroll.availableWidth
                spacing: 6 * organizer.uiScale

                Choice {
                    visible: actions.step === "book" && actions.book && !actions.book.trashed
                    text: actions.book && actions.book.favorite ? qsTr("Rimuovi dai preferiti") : qsTr("Aggiungi ai preferiti")
                    iconName: actions.book && actions.book.favorite ? "star-filled" : "star"
                    onClicked: actions.finish(organizer.notebook.setFavorite(actions.bookId, !actions.book.favorite))
                }

                Choice {
                    visible: actions.step === "book" && actions.book && !actions.book.trashed
                    text: qsTr("Sposta in una cartella")
                    iconName: "folder"
                    onClicked: actions.openStep("moveBook")
                }

                Choice {
                    visible: actions.step === "book" && actions.book && !actions.book.trashed
                    text: qsTr("Gestisci tag")
                    onClicked: actions.openStep("bookTags")
                }

                Choice {
                    visible: actions.step === "book" && actions.book && !actions.book.trashed
                    text: qsTr("Duplica quaderno")
                    onClicked: actions.finish(organizer.notebook.duplicateBook(actions.bookId))
                }

                Choice {
                    visible: actions.step === "book" && actions.book && !actions.book.trashed
                    text: qsTr("Sposta nel cestino")
                    onClicked: actions.finish(organizer.notebook.trashBook(actions.bookId))
                }

                Choice {
                    visible: actions.step === "book" && actions.book && actions.book.trashed
                    text: qsTr("Ripristina quaderno")
                    onClicked: actions.finish(organizer.notebook.restoreBook(actions.bookId))
                }

                Choice {
                    visible: actions.step === "book" && actions.book && actions.book.trashed
                    text: qsTr("Elimina definitivamente")
                    onClicked: actions.openStep("purgeBook")
                }

                Choice {
                    visible: actions.step === "folder" && actions.folder
                    text: qsTr("Rinomina cartella")
                    iconName: "rename"
                    onClicked: actions.openStep("renameFolder")
                }

                Choice {
                    visible: actions.step === "folder" && actions.folder
                    text: qsTr("Elimina cartella")
                    onClicked: actions.openStep("removeFolder")
                }

                Choice {
                    visible: actions.step === "sort"
                    text: qsTr("Ordine della libreria")
                    filled: !organizer.sortByTitle
                    onClicked: {
                        organizer.sortByTitle = false
                        actions.close()
                    }
                }

                Choice {
                    visible: actions.step === "sort"
                    text: qsTr("Nome da A a Z")
                    filled: organizer.sortByTitle
                    onClicked: {
                        organizer.sortByTitle = true
                        actions.close()
                    }
                }

                OrganizerLabel {
                    visible: actions.confirmationStep
                    wrapMode: Text.WordWrap
                    text: actions.step === "purgeBook" && actions.book
                          ? qsTr("Vuoi eliminare definitivamente «%1»?").arg(actions.book.title)
                          : actions.folder ? qsTr("Vuoi eliminare la cartella «%1»?").arg(actions.folder.title) : ""
                }

                Repeater {
                    model: actions.step === "moveBook" ? actions.destinations : []

                    Choice {
                        required property var modelData

                        text: modelData.title
                        iconName: "folder"
                        enabled: actions.book !== null && actions.book.folderId !== modelData.id
                        onClicked: actions.finish(organizer.notebook.moveBook(actions.bookId, modelData.id))
                    }
                }

                OrganizerLabel {
                    visible: actions.step === "pickTag" && organizer.tags.length === 0
                    text: qsTr("Assegna un tag a un quaderno o a una pagina.")
                    color: organizer.muted
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: actions.step === "pickTag" ? organizer.tags : []

                    Choice {
                        required property string modelData

                        text: modelData
                        filled: organizer.viewMode === "tags"
                                && organizer.selectedTag.toLocaleLowerCase() === modelData.toLocaleLowerCase()
                        onClicked: {
                            organizer.selectedTag = modelData
                            organizer.viewMode = "tags"
                            actions.close()
                        }
                    }
                }

                Flow {
                    Layout.fillWidth: true
                    visible: actions.step === "bookTags" && actions.tagDraft.length > 0
                    spacing: 4 * organizer.uiScale

                    Repeater {
                        model: actions.step === "bookTags" ? actions.tagDraft : []

                        PaperButton {
                            required property int index
                            required property string modelData

                            uiScale: organizer.uiScale
                            width: Math.min(implicitWidth, fields.width)
                            text: qsTr("%1 ×").arg(modelData)
                            Accessible.name: qsTr("Rimuovi tag %1").arg(modelData)
                            onClicked: {
                                actions.tagDraft = actions.tagDraft.filter((entry, position) => position !== index)
                                actions.errorText = ""
                            }
                        }
                    }
                }

                RowLayout {
                    visible: actions.inputStep
                    Layout.fillWidth: true
                    spacing: 6 * organizer.uiScale

                    TextField {
                        id: input

                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredHeight: 44 * organizer.uiScale
                        placeholderText: actions.folderNameStep ? qsTr("Nome cartella") : qsTr("Nuovo tag")
                        Accessible.name: placeholderText
                        maximumLength: actions.folderNameStep ? 80 : 40
                        selectByMouse: true
                        color: organizer.ink
                        placeholderTextColor: organizer.muted
                        font.pixelSize: 15 * organizer.uiScale
                        leftPadding: 10 * organizer.uiScale
                        rightPadding: leftPadding
                        selectionColor: organizer.blue
                        selectedTextColor: "#ffffff"
                        background: Rectangle {
                            color: organizer.surface
                            border.color: input.activeFocus ? organizer.blue : organizer.border
                            border.width: input.activeFocus ? Math.max(2, organizer.uiScale) : 1
                            radius: 8 * organizer.uiScale
                        }
                        onAccepted: {
                            if (actions.folderNameStep)
                                actions.saveInput()
                            else
                                actions.addTag()
                        }
                    }

                    PaperButton {
                        visible: actions.step === "bookTags"
                        uiScale: organizer.uiScale
                        text: qsTr("Aggiungi")
                        enabled: input.text.trim().length > 0
                        onClicked: actions.addTag()
                    }
                }

                TouchKeyboard {
                    Layout.fillWidth: true
                    visible: actions.inputStep && organizer.showTouchKeyboard
                    target: input
                    uiScale: organizer.uiScale
                }

                OrganizerLabel {
                    visible: actions.errorText.length > 0
                    text: actions.errorText
                    color: "#87332c"
                    wrapMode: Text.WordWrap
                }
            }
        }

        footer: RowLayout {
            spacing: 6 * organizer.uiScale

            Item { Layout.fillWidth: true }

            PaperButton {
                uiScale: organizer.uiScale
                Layout.rightMargin: submit.visible ? 0 : actions.rightPadding
                Layout.bottomMargin: actions.bottomPadding
                text: actions.inputStep || actions.confirmationStep ? qsTr("Annulla") : qsTr("Chiudi")
                quiet: true
                onClicked: actions.close()
            }

            PaperButton {
                id: submit

                visible: actions.inputStep || actions.confirmationStep
                uiScale: organizer.uiScale
                Layout.rightMargin: actions.rightPadding
                Layout.bottomMargin: actions.bottomPadding
                text: actions.confirmationStep ? qsTr("Elimina") : actions.step === "newFolder" ? qsTr("Crea") : qsTr("Salva")
                filled: true
                enabled: !actions.folderNameStep || input.text.trim().length > 0
                onClicked: {
                    if (actions.step === "purgeBook")
                        actions.finish(organizer.notebook.purgeBook(actions.bookId))
                    else if (actions.step === "removeFolder" && actions.folder)
                        actions.finish(organizer.notebook.removeFolder(actions.folder.id))
                    else
                        actions.saveInput()
                }
            }
        }
    }
}
