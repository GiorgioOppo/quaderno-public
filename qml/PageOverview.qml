pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: overview
    objectName: "pageOverview"

    required property Notebook notebook
    required property real uiScale
    property bool tabletMode: false
    property bool showTouchKeyboard: tabletMode
    readonly property real hairline: Math.max(1, Math.round(uiScale))
    readonly property color ink: "#24262c"
    readonly property color border: "#d9dce2"
    readonly property color blue: "#155cc2"
    readonly property var destinations: notebook.books
        .filter(book => !book.trashed && book.id !== notebook.bookId)
        .sort((a, b) => a.title.localeCompare(b.title))
    property int selectedPage: 0
    property int previewRevision: 0
    property string pageFilter: "all"
    readonly property var summaries: previewRevision > 0 ? notebook.pageSummaries() : []
    readonly property var visiblePages: summaries.filter(page => pageFilter === "all"
        || (pageFilter === "bookmarks" ? page.bookmarked : page.annotated))
    readonly property int bookmarkCount: summaries.filter(page => page.bookmarked).length
    readonly property int annotatedCount: summaries.filter(page => page.annotated).length
    readonly property bool selectionAvailable: visiblePages.some(page => page.index === selectedPage)
    property bool confirmingDelete: false
    property bool choosingDestination: false
    property string errorText: ""
    property string transferStatus: ""
    signal editPageTags(int index)

    anchors.centerIn: Overlay.overlay
    width: Math.min(680 * uiScale, (parent ? parent.width : 680 * uiScale) - 24 * uiScale)
    height: Math.min(630 * uiScale, (parent ? parent.height : 630 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    spacing: 10 * uiScale
    modal: true
    dim: !tabletMode
    focus: true
    title: qsTr("Pagine")
    closePolicy: Popup.CloseOnEscape

    onAboutToShow: {
        selectedPage = notebook.page
        pageFilter = "all"
        pageNumber.text = String(selectedPage + 1)
        previewRevision++
        confirmingDelete = false
        choosingDestination = false
        errorText = ""
        transferStatus = ""
    }

    onOpened: {
        positionSelected()
        grid.forceActiveFocus()
    }

    function selectPage(index: int) {
        selectedPage = Math.min(notebook.pageCount - 1, Math.max(0, index))
        pageNumber.text = String(selectedPage + 1)
        errorText = ""
        transferStatus = ""
        confirmingDelete = false
        positionSelected()
    }

    function positionSelected() {
        const index = visiblePages.findIndex(page => page.index === selectedPage)
        if (index >= 0)
            grid.positionViewAtIndex(index, GridView.Contain)
    }

    function selectVisible(offset: int) {
        if (visiblePages.length === 0)
            return
        const current = visiblePages.findIndex(page => page.index === selectedPage)
        const next = Math.max(0, Math.min(visiblePages.length - 1, current < 0 ? 0 : current + offset))
        selectPage(visiblePages[next].index)
    }

    function jumpToNumber() {
        if (!pageNumber.acceptableInput)
            return
        pageFilter = "all"
        selectPage(Number(pageNumber.text) - 1)
        grid.forceActiveFocus()
    }

    function setPageFilter(value: string) {
        pageFilter = value
        if (!visiblePages.some(page => page.index === selectedPage) && visiblePages.length > 0)
            selectPage(visiblePages[0].index)
        Qt.callLater(positionSelected)
    }

    function openPage() {
        if (!selectionAvailable)
            return
        if (notebook.goToPage(selectedPage))
            overview.close()
        else
            errorText = notebook.error
    }

    function duplicateSelected() {
        const index = selectedPage
        if (notebook.duplicatePage(index))
            selectPage(index + 1)
        else
            errorText = notebook.error
    }

    function moveSelected(offset: int) {
        const target = selectedPage + offset
        if (notebook.movePage(selectedPage, target))
            selectPage(target)
        else
            errorText = notebook.error
    }

    function deleteSelected() {
        const index = selectedPage
        if (notebook.deletePage(index))
            selectPage(index)
        else
            errorText = notebook.error
    }

    function chooseDestination() {
        transferStatus = ""
        errorText = ""
        choosingDestination = true
        destinationList.currentIndex = -1
        destinationList.forceActiveFocus()
    }

    function cancelDestination() {
        choosingDestination = false
        errorText = ""
        grid.forceActiveFocus()
    }

    function transferSelected() {
        const book = destinations[destinationList.currentIndex]
        if (!book)
            return
        const index = selectedPage
        if (notebook.transferPage(index, book.id)) {
            cancelDestination()
            selectPage(index)
            transferStatus = qsTr("Pagina spostata in «%1».").arg(book.title)
        } else {
            errorText = notebook.error
        }
    }

    function destinationFolder(folderId: string): string {
        const folders = notebook.folders
        const names = []
        let id = folderId
        while (id.length > 0) {
            const folder = folders.find(entry => entry.id === id)
            if (!folder)
                break
            names.unshift(folder.title)
            id = folder.parentId
        }
        return names.length > 0 ? names.join(" / ") : qsTr("Libreria")
    }

    function refreshPreviews() {
        if (visible)
            previewRevision++
    }

    Connections {
        target: overview.notebook
        enabled: overview.visible

        function onDocumentChanged() { Qt.callLater(overview.refreshPreviews) }
        function onPageContentChanged() { Qt.callLater(overview.refreshPreviews) }
        function onStrokesChanged() { Qt.callLater(overview.refreshPreviews) }
    }

    background: Rectangle {
        color: "white"
        border.color: overview.border
        border.width: overview.hairline
        radius: 16 * overview.uiScale
    }

    header: RowLayout {
        spacing: 6 * overview.uiScale

        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.leftMargin: overview.leftPadding
            Layout.topMargin: overview.topPadding
            text: overview.choosingDestination ? qsTr("Sposta pagina %1").arg(overview.selectedPage + 1) : overview.title
            elide: Text.ElideRight
            font.pixelSize: 22 * overview.uiScale
            font.weight: Font.DemiBold
            color: overview.ink
        }

        PaperButton {
            visible: !overview.choosingDestination && !overview.confirmingDelete
            enabled: overview.selectionAvailable && overview.destinations.length > 0
            Layout.topMargin: 8 * overview.uiScale
            uiScale: overview.uiScale
            leftPadding: 9 * uiScale
            rightPadding: leftPadding
            text: qsTr("Sposta in…")
            onClicked: overview.chooseDestination()
        }

        PaperButton {
            visible: !overview.choosingDestination && !overview.confirmingDelete
            Layout.topMargin: 8 * overview.uiScale
            uiScale: overview.uiScale
            leftPadding: 9 * uiScale
            rightPadding: leftPadding
            text: qsTr("Tag pagina")
            enabled: overview.selectionAvailable
            onClicked: overview.editPageTags(overview.selectedPage)
        }

        PaperButton {
            Layout.rightMargin: overview.rightPadding
            Layout.topMargin: 8 * overview.uiScale
            uiScale: overview.uiScale
            text: qsTr("Chiudi")
            iconName: "close"
            iconOnly: true
            quiet: true
            onClicked: overview.close()
        }
    }

    contentItem: ColumnLayout {
        spacing: 8 * overview.uiScale

        RowLayout {
            visible: !overview.choosingDestination
            Layout.fillWidth: true
            spacing: 4 * overview.uiScale
            Repeater {
                model: [
                    {key: "all", label: qsTr("Tutte (%1)").arg(overview.summaries.length)},
                    {key: "bookmarks", label: qsTr("Segnalibri (%1)").arg(overview.bookmarkCount)},
                    {key: "annotated", label: qsTr("Annotate (%1)").arg(overview.annotatedCount)}
                ]
                PaperButton {
                    required property var modelData
                    uiScale: overview.uiScale
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    leftPadding: 5 * uiScale
                    rightPadding: leftPadding
                    text: modelData.label
                    filled: overview.pageFilter === modelData.key
                    onClicked: overview.setPageFilter(modelData.key)
                }
            }
        }

        RowLayout {
            visible: !overview.choosingDestination
            Layout.fillWidth: true
            spacing: 5 * overview.uiScale
            PaperButton {
                uiScale: overview.uiScale
                text: qsTr("Prima")
                enabled: overview.notebook.pageCount > 0
                onClicked: { overview.pageFilter = "all"; overview.selectPage(0) }
            }
            TextField {
                id: pageNumber
                objectName: "overviewPageNumber"
                Layout.fillWidth: true
                Layout.minimumWidth: 40 * overview.uiScale
                font.pixelSize: 15 * overview.uiScale
                horizontalAlignment: TextInput.AlignHCenter
                inputMethodHints: Qt.ImhDigitsOnly
                validator: IntValidator { bottom: 1; top: Math.max(1, overview.notebook.pageCount) }
                Accessible.name: qsTr("Numero di pagina")
                selectByMouse: true
                onActiveFocusChanged: if (activeFocus) selectAll()
                onAccepted: overview.jumpToNumber()
            }
            Label {
                text: qsTr("di %1").arg(overview.notebook.pageCount)
                font.pixelSize: 14 * overview.uiScale
                color: overview.ink
            }
            PaperButton {
                uiScale: overview.uiScale
                text: qsTr("Vai")
                enabled: pageNumber.acceptableInput
                onClicked: overview.jumpToNumber()
            }
            PaperButton {
                uiScale: overview.uiScale
                text: qsTr("Ultima")
                enabled: overview.notebook.pageCount > 0
                onClicked: { overview.pageFilter = "all"; overview.selectPage(overview.notebook.pageCount - 1) }
            }
        }

        GridLayout {
            visible: !overview.choosingDestination && overview.showTouchKeyboard && pageNumber.activeFocus
            Layout.fillWidth: true
            columns: 6
            Repeater {
                model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "⌫", "✓"]
                PaperButton {
                    required property string modelData
                    uiScale: overview.uiScale
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    focusPolicy: Qt.NoFocus
                    text: modelData
                    Accessible.name: modelData === "⌫" ? qsTr("Cancella cifra") : modelData === "✓" ? qsTr("Vai alla pagina") : modelData
                    onClicked: {
                        if (modelData === "✓") { overview.jumpToNumber(); return }
                        const start = pageNumber.selectionStart
                        const end = pageNumber.selectionEnd
                        if (end > start) pageNumber.remove(start, end)
                        else if (modelData === "⌫" && start > 0) pageNumber.remove(start - 1, start)
                        if (modelData !== "⌫") pageNumber.insert(pageNumber.cursorPosition, modelData)
                    }
                }
            }
        }

        Label {
            visible: !overview.choosingDestination && overview.visiblePages.length === 0
            Layout.fillWidth: true
            text: qsTr("Nessuna pagina corrisponde al filtro.")
            wrapMode: Text.WordWrap
            color: overview.ink
        }

        GridView {
            id: grid

            readonly property int columns: Math.max(2, Math.floor(width / (132 * overview.uiScale)))
            visible: !overview.choosingDestination
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            activeFocusOnTab: true
            cacheBuffer: 0
            boundsBehavior: Flickable.StopAtBounds
            cellWidth: Math.floor(width / columns)
            cellHeight: Math.round(cellWidth * 1.28)
            keyNavigationEnabled: false
            model: overview.visible ? overview.visiblePages : []
            ScrollBar.vertical: ScrollBar { }

            Keys.onLeftPressed: overview.selectVisible(-1)
            Keys.onRightPressed: overview.selectVisible(1)
            Keys.onUpPressed: overview.selectVisible(-columns)
            Keys.onDownPressed: overview.selectVisible(columns)
            Keys.onReturnPressed: if (!overview.confirmingDelete) overview.openPage()
            Keys.onEnterPressed: if (!overview.confirmingDelete) overview.openPage()

            delegate: Item {
                id: tile

                required property int index
                required property var modelData
                readonly property int pageIndex: modelData.index
                width: grid.cellWidth
                height: grid.cellHeight

                ItemDelegate {
                    id: card

                    readonly property bool selected: overview.selectedPage === tile.pageIndex
                    anchors.fill: parent
                    anchors.margins: 4 * overview.uiScale
                    padding: 8 * overview.uiScale
                    text: qsTr("Pagina %1").arg(tile.pageIndex + 1)
                    focusPolicy: Qt.NoFocus
                    onClicked: overview.selectPage(tile.pageIndex)
                    onDoubleClicked: {
                        overview.selectPage(tile.pageIndex)
                        overview.openPage()
                    }

                    background: Rectangle {
                        color: card.selected || card.down ? "#edf3ff" : "#f4f5f7"
                        border.color: card.selected ? overview.blue : overview.border
                        border.width: (card.selected ? 2 : 1) * overview.hairline
                        radius: 10 * overview.uiScale
                    }

                    contentItem: ColumnLayout {
                        spacing: 6 * overview.uiScale

                        Item {
                            id: previewSpace
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Rectangle {
                                anchors.centerIn: parent
                                height: Math.min(previewSpace.height, previewSpace.width * 4 / 3)
                                width: height * 0.75
                                color: "white"
                                border.color: overview.border
                                border.width: overview.hairline

                                PagePreviewItem {
                                    anchors.fill: parent
                                    anchors.margins: overview.hairline
                                    content: overview.previewRevision > 0
                                             ? overview.notebook.pagePreview(tile.pageIndex) : ({})
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: card.text
                            horizontalAlignment: Text.AlignHCenter
                            font.pixelSize: 13 * overview.uiScale
                            font.weight: Font.DemiBold
                            color: card.selected ? overview.blue : overview.ink
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        ListView {
            id: destinationList

            visible: overview.choosingDestination
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            activeFocusOnTab: true
            spacing: 5 * overview.uiScale
            boundsBehavior: Flickable.StopAtBounds
            model: overview.choosingDestination ? overview.destinations : []
            ScrollBar.vertical: ScrollBar { }
            Keys.onReturnPressed: overview.transferSelected()
            Keys.onEnterPressed: overview.transferSelected()

            delegate: ItemDelegate {
                id: destination

                required property int index
                required property var modelData
                readonly property bool selected: destinationList.currentIndex === index
                width: destinationList.width - 6 * overview.uiScale
                topPadding: 8 * overview.uiScale
                bottomPadding: topPadding
                leftPadding: 12 * overview.uiScale
                rightPadding: leftPadding
                text: modelData.title
                focusPolicy: Qt.NoFocus
                onClicked: destinationList.currentIndex = index

                background: Rectangle {
                    color: destination.selected || destination.down ? "#edf3ff" : "#f4f5f7"
                    border.color: destination.selected ? overview.blue : overview.border
                    border.width: (destination.selected ? 2 : 1) * overview.hairline
                    radius: 8 * overview.uiScale
                }

                contentItem: ColumnLayout {
                    spacing: 2 * overview.uiScale

                    Label {
                        Layout.fillWidth: true
                        text: destination.text
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        font.pixelSize: 16 * overview.uiScale
                        font.weight: Font.DemiBold
                        color: overview.ink
                    }

                    Label {
                        Layout.fillWidth: true
                        text: overview.destinationFolder(destination.modelData.folderId)
                        textFormat: Text.PlainText
                        elide: Text.ElideLeft
                        font.pixelSize: 13 * overview.uiScale
                        color: "#555b67"
                    }
                }
            }
        }

        Label {
            visible: overview.errorText.length > 0
            Layout.fillWidth: true
            text: overview.errorText
            color: "#87332c"
            wrapMode: Text.WordWrap
        }

        Label {
            visible: overview.transferStatus.length > 0 && !overview.confirmingDelete
            Layout.fillWidth: true
            text: overview.transferStatus
            textFormat: Text.PlainText
            color: overview.blue
            wrapMode: Text.WordWrap
        }

        Label {
            visible: overview.confirmingDelete
            Layout.fillWidth: true
            text: qsTr("Eliminare la pagina %1?").arg(overview.selectedPage + 1)
            font.pixelSize: 14 * overview.uiScale
            font.weight: Font.DemiBold
            color: "#87332c"
            wrapMode: Text.WordWrap
        }

        RowLayout {
            visible: !overview.choosingDestination
            Layout.fillWidth: true
            spacing: 5 * overview.uiScale

            Label {
                visible: !overview.confirmingDelete && overview.width >= 470 * overview.uiScale
                text: qsTr("Sposta")
                font.pixelSize: 14 * overview.uiScale
                color: overview.ink
            }

            PaperButton {
                visible: !overview.confirmingDelete
                uiScale: overview.uiScale
                text: qsTr("Sposta indietro")
                iconName: "left"
                iconOnly: true
                enabled: overview.selectionAvailable && overview.selectedPage > 0
                ToolTip.visible: !overview.tabletMode && hovered
                ToolTip.text: text
                onClicked: overview.moveSelected(-1)
            }

            PaperButton {
                visible: !overview.confirmingDelete
                uiScale: overview.uiScale
                text: qsTr("Sposta avanti")
                iconName: "right"
                iconOnly: true
                enabled: overview.selectionAvailable && overview.selectedPage + 1 < overview.notebook.pageCount
                ToolTip.visible: !overview.tabletMode && hovered
                ToolTip.text: text
                onClicked: overview.moveSelected(1)
            }

            PaperButton {
                visible: !overview.confirmingDelete
                uiScale: overview.uiScale
                text: qsTr("Duplica")
                enabled: overview.selectionAvailable
                onClicked: overview.duplicateSelected()
            }

            PaperButton {
                visible: !overview.confirmingDelete
                uiScale: overview.uiScale
                text: qsTr("Elimina")
                enabled: overview.selectionAvailable && overview.notebook.pageCount > 1
                onClicked: overview.confirmingDelete = true
            }

            Item { Layout.fillWidth: true }

            PaperButton {
                visible: !overview.confirmingDelete
                uiScale: overview.uiScale
                text: qsTr("Apri")
                enabled: overview.selectionAvailable
                filled: true
                onClicked: overview.openPage()
            }

            PaperButton {
                visible: overview.confirmingDelete
                uiScale: overview.uiScale
                text: qsTr("Annulla")
                onClicked: overview.confirmingDelete = false
            }

            PaperButton {
                visible: overview.confirmingDelete
                uiScale: overview.uiScale
                text: qsTr("Elimina pagina")
                filled: true
                onClicked: overview.deleteSelected()
            }
        }

        RowLayout {
            visible: overview.choosingDestination
            Layout.fillWidth: true
            spacing: 5 * overview.uiScale

            Item { Layout.fillWidth: true }

            PaperButton {
                uiScale: overview.uiScale
                text: qsTr("Annulla")
                onClicked: overview.cancelDestination()
            }

            PaperButton {
                uiScale: overview.uiScale
                text: qsTr("Sposta pagina")
                filled: true
                enabled: destinationList.currentIndex >= 0 && destinationList.currentIndex < overview.destinations.length
                onClicked: overview.transferSelected()
            }
        }
    }
}
