pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import Quaderno

Item {
    id: strip
    objectName: "notebookPageStrip"

    required property Notebook notebook
    required property real uiScale
    property bool tabletMode: false
    readonly property bool available: Boolean(roleCounts.writing)
        && Boolean(roleCounts.charts || roleCounts.tables || roleCounts.diagrams)
    signal pageRequested(int index)

    property string summarizedBook: ""
    property var summaries: []
    property var roleCounts: ({})

    visible: available
    implicitHeight: visible ? 44 * uiScale : 0

    function refreshPages() {
        summarizedBook = notebook.bookId
        const pages = notebook.pageSummaries()
        const counts = {}
        for (const page of pages)
            counts[page.workspace] = (counts[page.workspace] || 0) + 1
        roleCounts = counts
        summaries = pages
        Qt.callLater(positionCurrentPage)
    }

    function positionCurrentPage() {
        if (available && notebook.page >= 0 && notebook.page < tabs.count)
            tabs.positionViewAtIndex(notebook.page, ListView.Contain)
    }

    function pageLabel(page: var): string {
        const labels = {
            writing: qsTr("Scrittura"), charts: qsTr("Grafico"),
            tables: qsTr("Tabella"), diagrams: qsTr("Diagramma")
        }
        const label = labels[page.workspace]
        if (!label) return qsTr("Pagina %1").arg(page.index + 1)
        return roleCounts[page.workspace] > 1 ? qsTr("%1 · %2").arg(label).arg(page.index + 1) : label
    }

    Component.onCompleted: refreshPages()

    Connections {
        target: strip.notebook
        function onBooksChanged() { Qt.callLater(strip.refreshPages) }
        function onLibraryItemsChanged() { Qt.callLater(strip.refreshPages) }
        function onDocumentChanged() {
            if (strip.summarizedBook !== strip.notebook.bookId)
                Qt.callLater(strip.refreshPages)
            else
                Qt.callLater(strip.positionCurrentPage)
        }
        function onPageContentChanged() {
            const cached = strip.summaries[strip.notebook.page]
            if (cached && cached.workspace !== strip.notebook.pageWorkspace)
                Qt.callLater(strip.refreshPages)
        }
    }

    ListView {
        id: tabs
        anchors.fill: parent
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        spacing: 6 * strip.uiScale
        reuseItems: true
        cacheBuffer: 0
        model: strip.available ? strip.summaries : []
        ScrollBar.horizontal: ScrollBar { policy: strip.tabletMode ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded }

        delegate: PaperButton {
            required property var modelData
            objectName: "notebookPageTab_" + modelData.index
            property bool remoteClickable: true
            uiScale: strip.uiScale
            text: strip.pageLabel(modelData)
            filled: modelData.index === strip.notebook.page
            quiet: true
            iconName: ({writing: "pen", charts: "chart", tables: "table", diagrams: "diagram"})[modelData.workspace] || "paper"
            Accessible.role: Accessible.PageTab
            Accessible.name: modelData.title ? text + " · " + modelData.title : text
            onClicked: strip.pageRequested(modelData.index)
        }
    }
}
