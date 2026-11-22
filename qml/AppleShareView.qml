import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Quaderno

Dialog {
    id: share
    objectName: "appleShareView"

    required property Notebook notebook
    required property AppleShare appleShare
    required property MacDocumentExchange documentExchange
    required property WirelessSync wireless
    property var smtpMail: null
    required property real uiScale
    required property bool tabletMode
    property bool locked: false
    property bool canTransfer: false
    property bool showApple: false
    property string sourceBook: ""
    property string sourcePage: ""
    property var formats: []
    property var pendingOption: null
    property string saveStatus: ""
    property string requestedTransport: ""
    property bool showLinkControls: false
    property int deliveryFilter: 0
    property string preferredDestination: "notes"
    readonly property var selection: formats[formatPicker.currentIndex] || null
    readonly property bool selectedPdf: selection !== null && (selection.format === "pdf" || selection.format === "pdf-page")
    readonly property bool validContext: !locked && sourceBook.length > 0 && sourcePage.length > 0
        && notebook.bookId === sourceBook && notebook.pageId === sourcePage
    readonly property bool tabletPdf: tabletMode && selectedPdf
    readonly property bool busy: appleShare.busy || wireless.pdfSending || Boolean(smtpMail?.busy)
    signal transferRequested(string transport)
    signal mailRequested(var option)
    readonly property var visibleDeliveries: appleShare.deliveries.filter(entry => {
        const finished = entry.state === "completed" || entry.state === "cancelled"
        return deliveryFilter === 1 ? !finished : deliveryFilter === 2 ? finished : true
    })

    function destinationName(destination) {
        const names = {notes: qsTr("Note"), books: qsTr("Libri"), numbers: "Numbers", keynote: "Keynote",
                       journal: qsTr("Diario"), files: qsTr("File"), share: qsTr("Condivisione macOS")}
        return names[destination] || destination
    }

    function deliveryDate(created) {
        const date = new Date(created || "")
        return isNaN(date.getTime()) ? "" : Qt.formatDateTime(date, "yyyy-MM-dd hh:mm")
    }

    function begin(apple: bool, itemId: string) {
        if (locked) return
        showApple = apple && appleShare.available
        open()
        if (itemId.length) {
            const index = formats.findIndex(option => option.itemId === itemId)
            if (index >= 0) formatPicker.currentIndex = index
        }
    }

    function formatLabel(option: var): string {
        switch (option.format) {
        case "pdf": return qsTr("PDF · Intero quaderno")
        case "pdf-page": return qsTr("PDF · Pagina corrente")
        case "markdown": return qsTr("Markdown (.md)")
        case "markdown-assets": return qsTr("Markdown con allegati")
        case "drawio": return qsTr("draw.io · %1").arg(option.title)
        case "csv": return qsTr("CSV · %1").arg(option.title)
        case "quaderno": return qsTr("Quaderno modificabile (.quaderno)")
        case "png": return qsTr("PNG · Pagina corrente")
        case "source": return qsTr("Originale · %1").arg(option.extension.toUpperCase())
        default: return option.extension.toUpperCase()
        }
    }

    function saveFile(option: var) {
        if (!validContext || busy || !option) return
        if (tabletMode && (option.format === "pdf" || option.format === "pdf-page")) return
        pendingOption = option
        saveStatus = ""
        savePicker.selectedFile = notebook.suggestedShareFile(option)
        if (!savePicker.selectedFile.toString().length) {
            saveStatus = qsTr("Il formato non è più disponibile per questa pagina.")
            pendingOption = null
            return
        }
        savePicker.open()
    }

    function sendFile() {
        if (!validContext || !selection) return
        saveStatus = ""
        appleShare.shareFile(notebook, selection)
    }

    function printFile() {
        if (!validContext || !selectedPdf) return
        saveStatus = ""
        appleShare.printPdf(notebook, selection)
    }

    function openConnection(transport: string) {
        close()
        transferRequested(transport)
    }

    function sendPdf(transport: string) {
        if (!validContext || !tabletPdf || busy) return
        if (!wireless.paired) {
            openConnection(transport)
            return
        }
        requestedTransport = transport
        saveStatus = ""
        showLinkControls = !wireless.sendPdf(notebook, selection, transport)
    }

    onAboutToShow: {
        sourceBook = notebook.bookId
        sourcePage = notebook.pageId
        saveStatus = ""
        requestedTransport = ""
        showLinkControls = false
        formats = notebook.shareOptions().map(option => Object.assign({}, option, {label: formatLabel(option)}))
        formatPicker.currentIndex = formats.length ? 0 : -1
    }
    onAboutToHide: {
        pendingOption = null
        savePicker.close()
    }
    onLockedChanged: if (locked) {
        appleShare.cancelPendingFileShare()
        if (wireless.pdfSending) wireless.cancelPdf()
        close()
    }
    onShowAppleChanged: if (shareScroll.contentItem) shareScroll.contentItem.contentY = 0
    onClosed: {
        showApple = false
        sourceBook = ""
        sourcePage = ""
        formats = []
        shareScroll.contentItem.contentY = 0
    }

    Connections {
        target: share.notebook
        function onDocumentChanged() {
            if (share.visible && (share.notebook.bookId !== share.sourceBook || share.notebook.pageId !== share.sourcePage)) {
                share.appleShare.cancelPendingFileShare()
                share.close()
            }
        }
    }

    FileDialog {
        id: savePicker
        objectName: "shareSavePicker"
        title: qsTr("Salva una copia")
        fileMode: FileDialog.SaveFile
        defaultSuffix: share.pendingOption ? share.pendingOption.extension : ""
        nameFilters: share.pendingOption ? [qsTr("%1 (*.%2)").arg(share.pendingOption.extension.toUpperCase())
            .arg(share.pendingOption.extension)] : []
        onAccepted: {
            const option = share.pendingOption
            const destination = selectedFile
            share.pendingOption = null
            if (!option || !share.validContext) return
            share.saveStatus = share.notebook.exportShareFile(option, destination)
                ? qsTr("Copia salvata.") : share.notebook.error
        }
        onRejected: share.pendingOption = null
    }

    readonly property color ink: "#1e2024"
    readonly property color muted: "#535862"
    readonly property color blue: "#155cc2"
    readonly property color border: "#dedfe4"
    readonly property color surface: "#f6f7f9"

    title: qsTr("Condividi ed esporta")
    anchors.centerIn: Overlay.overlay
    width: Math.min(460 * uiScale, (parent ? parent.width : 460 * uiScale) - 32 * uiScale)
    padding: 18 * uiScale
    spacing: 14 * uiScale
    modal: true
    dim: !tabletMode
    focus: true

    background: Rectangle {
        color: "white"
        border.color: share.border
        radius: 16 * share.uiScale
    }

    header: Label {
        text: share.title
        textFormat: Text.PlainText
        color: share.ink
        font.pixelSize: 22 * share.uiScale
        font.weight: Font.DemiBold
        leftPadding: share.leftPadding
        rightPadding: share.rightPadding
        topPadding: share.topPadding
    }

    component Detail: Label {
        Layout.fillWidth: true
        color: share.muted
        textFormat: Text.PlainText
        font.pixelSize: 14 * share.uiScale
        wrapMode: Text.Wrap
    }

    component Destination: ItemDelegate {
        id: destination

        required property string description

        Layout.fillWidth: true
        implicitHeight: implicitContentHeight + topPadding + bottomPadding
        leftPadding: 12 * share.uiScale
        rightPadding: leftPadding
        topPadding: 10 * share.uiScale
        bottomPadding: topPadding
        enabled: share.appleShare.available && !share.appleShare.busy && share.notebook.pageCount > 0
        Accessible.name: qsTr("%1. %2").arg(text).arg(description)

        background: Rectangle {
            color: destination.down ? "#e6eefb" : destination.hovered && destination.enabled ? "#eef3fc" : "transparent"
            border.color: share.blue
            border.width: destination.visualFocus ? 2 : 0
            radius: 8 * share.uiScale

        }

        contentItem: RowLayout {
            spacing: 8 * share.uiScale

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3 * share.uiScale

                Detail {
                    text: destination.text
                    color: destination.enabled ? share.blue : share.muted
                    font.pixelSize: 16 * share.uiScale
                    font.weight: Font.DemiBold
                }

                Detail { text: destination.description }
            }

            PaperIcon {
                name: "right"
                tint: destination.enabled ? share.blue : share.muted
                Layout.preferredWidth: 18 * share.uiScale
                Layout.preferredHeight: Layout.preferredWidth
                Accessible.ignored: true
            }
        }
    }

    contentItem: ScrollView {
        id: shareScroll
        clip: true
        implicitHeight: Math.min(shareBody.implicitHeight, Math.max(120, (share.parent ? share.parent.height : 800) - 120 * share.uiScale))
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: shareBody
            width: shareScroll.availableWidth
            spacing: 12 * share.uiScale

            RowLayout {
                Layout.fillWidth: true
                spacing: 8 * share.uiScale

                Detail {
                    text: share.notebook.title
                    color: share.ink
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                }

                Detail {
                    Layout.fillWidth: false
                    text: share.notebook.pageCount === 1 ? qsTr("1 pagina") : qsTr("%1 pagine").arg(share.notebook.pageCount)
                }
            }

            RowLayout {
                Layout.fillWidth: true
                visible: share.appleShare.available
                PaperButton {
                    Layout.fillWidth: true
                    uiScale: share.uiScale
                    text: qsTr("File")
                    filled: !share.showApple
                    onClicked: share.showApple = false
                }
                PaperButton {
                    Layout.fillWidth: true
                    uiScale: share.uiScale
                    text: qsTr("Apple")
                    filled: share.showApple
                    onClicked: share.showApple = true
                }
            }

            Detail {
                visible: !share.showApple && !share.validContext
                text: qsTr("Apri un quaderno per esportarlo.")
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: !share.showApple && share.validContext && share.formats.length > 0
                spacing: 8 * share.uiScale
                ComboBox {
                    id: formatPicker
                    objectName: "fileShareFormat"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.minimumHeight: 44 * share.uiScale
                    model: share.formats
                    textRole: "label"
                    font.pixelSize: 15 * share.uiScale
                    enabled: !share.busy
                    Accessible.name: qsTr("Formato della copia")
                }
                Detail {
                    visible: share.selection?.format === "markdown-assets"
                    text: share.appleShare.available
                          ? qsTr("Il salvataggio conserva gli allegati accanto al Markdown. Condividi li raccoglie in uno ZIP.")
                          : qsTr("Conserva la cartella degli allegati accanto al file Markdown.")
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 6 * share.uiScale
                    PaperButton {
                        objectName: "saveShareFile"
                        uiScale: share.uiScale
                        text: qsTr("Salva file…")
                        filled: true
                        iconName: "download"
                        visible: !share.tabletPdf
                        enabled: share.selection !== null && !share.busy
                        onClicked: share.saveFile(share.selection)
                    }
                    PaperButton {
                        objectName: "sendShareFile"
                        uiScale: share.uiScale
                        text: qsTr("Condividi…")
                        iconName: "share"
                        visible: share.appleShare.available
                        enabled: share.selection !== null && !share.appleShare.busy
                        onClicked: share.sendFile()
                    }
                    PaperButton {
                        objectName: "printShareFile"
                        uiScale: share.uiScale
                        text: qsTr("Stampa…")
                        iconName: "printer"
                        visible: share.appleShare.available && share.selectedPdf
                        enabled: !share.appleShare.busy
                        onClicked: share.printFile()
                    }
                    PaperButton {
                        objectName: "sendNotebookToMac"
                        uiScale: share.uiScale
                        text: qsTr("Trasferisci quaderno")
                        iconName: "sync"
                        visible: share.tabletMode && share.canTransfer && share.selection?.format === "quaderno"
                        onClicked: share.openConnection("")
                    }
                }
                ColumnLayout {
                    visible: share.tabletPdf
                    Layout.fillWidth: true
                    spacing: 6 * share.uiScale
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6 * share.uiScale
                        PaperButton {
                            objectName: "pdfShareWifi"
                            uiScale: share.uiScale
                            text: qsTr("Invia via Wi-Fi")
                            filled: true
                            iconName: "wifi"
                            visible: share.canTransfer
                            enabled: !share.busy
                            onClicked: share.sendPdf("wifi")
                        }
                        PaperButton {
                            objectName: "pdfShareBluetooth"
                            uiScale: share.uiScale
                            text: qsTr("Invia via Bluetooth")
                            iconName: "bluetooth"
                            visible: share.canTransfer
                            enabled: !share.busy
                            onClicked: share.sendPdf("bluetooth")
                        }
                        PaperButton {
                            objectName: "pdfShareEmail"
                            uiScale: share.uiScale
                            text: qsTr("Invia via email")
                            iconName: "share"
                            visible: share.smtpMail !== null
                            enabled: !share.busy
                            onClicked: share.mailRequested(share.selection)
                        }
                    }
                    Detail {
                        visible: share.canTransfer && !share.wireless.paired
                        text: qsTr("Abbina Quaderno sul Mac per ricevere i PDF via Wi-Fi o Bluetooth.")
                    }
                    Detail {
                        visible: share.canTransfer && share.wireless.pdfTransferSupported === "unsupported"
                        text: qsTr("Aggiorna Quaderno sul Mac alla 0.2.24 o successiva per ricevere il PDF.")
                    }
                    Detail {
                        visible: text.length > 0
                        text: share.requestedTransport.length > 0 || share.wireless.pdfSending ? share.wireless.pdfStatus : ""
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        visible: share.wireless.pdfSending
                        indeterminate: share.wireless.pdfProgress <= 0
                        value: share.wireless.pdfProgress
                    }
                    PaperButton {
                        uiScale: share.uiScale
                        text: qsTr("Annulla invio")
                        visible: share.wireless.pdfSending
                        onClicked: share.wireless.cancelPdf()
                    }
                    PaperButton {
                        uiScale: share.uiScale
                        text: qsTr("Connessione al Mac…")
                        iconName: "sync"
                        visible: share.showLinkControls && !share.wireless.pdfSending
                        onClicked: share.openConnection(share.requestedTransport)
                    }
                }
            }

            Control {
                Layout.fillWidth: true
                visible: share.showApple && share.appleShare.available && share.validContext
                padding: 2 * share.uiScale

                background: Rectangle {
                    color: share.surface
                    border.color: share.border
                    radius: 10 * share.uiScale
                }

                contentItem: ColumnLayout {
                    spacing: 0

                    Destination {
                        text: qsTr("Destinazione preferita: %1").arg(share.destinationName(share.preferredDestination))
                        description: qsTr("Prepara una copia da controllare prima dell’invio.")
                        onClicked: {
                            if (share.preferredDestination === "files")
                                share.saveFile(share.formats.find(option => option.format === "pdf"))
                            else share.appleShare.prepareDelivery(share.notebook, share.preferredDestination)
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 5 * share.uiScale
                        Repeater {
                            model: [ {name: qsTr("Note"), id: "notes"}, {name: qsTr("Libri"), id: "books"},
                                {name: "Numbers", id: "numbers"}, {name: "Keynote", id: "keynote"},
                                {name: qsTr("Diario"), id: "journal"} ]
                            Button {
                                required property var modelData
                                text: modelData.name
                                enabled: !share.appleShare.busy && share.notebook.pageCount > 0
                                onClicked: share.appleShare.prepareDelivery(share.notebook, modelData.id)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: !share.showApple && share.documentExchange.available && share.validContext
                spacing: 6 * share.uiScale

                Detail { text: qsTr("Trascina un PDF nel Finder") }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8 * share.uiScale

                    PdfDragHandle {
                        Layout.fillWidth: true
                        exchange: share.documentExchange
                        currentPage: true
                        uiScale: share.uiScale
                        text: qsTr("Pagina corrente")
                    }

                    PdfDragHandle {
                        Layout.fillWidth: true
                        exchange: share.documentExchange
                        currentPage: false
                        uiScale: share.uiScale
                        text: qsTr("Intero quaderno")
                    }
                }
            }

            Detail {
                visible: text.length > 0
                text: share.saveStatus || share.appleShare.status
            }

            Flow {
                visible: share.showApple && share.appleShare.deliveries.length > 0
                Layout.fillWidth: true
                spacing: 5 * share.uiScale
                Repeater {
                    model: [qsTr("Tutte"), qsTr("Da completare"), qsTr("Concluse")]
                    PaperButton {
                        required property string modelData
                        required property int index
                        uiScale: share.uiScale
                        text: modelData
                        filled: share.deliveryFilter === index
                        checked: share.deliveryFilter === index
                        checkable: true
                        onClicked: share.deliveryFilter = index
                    }
                }
            }
            Detail {
                visible: share.showApple && share.appleShare.deliveries.length > 0
                text: share.visibleDeliveries.length ? qsTr("Esportazioni mostrate: %1 / %2").arg(share.visibleDeliveries.length).arg(share.appleShare.deliveries.length)
                                                    : qsTr("Nessuna esportazione in questa vista.")
            }
            ScrollView {
                id: deliveriesScroll
                Layout.fillWidth: true
                Layout.preferredHeight: 160 * share.uiScale
                visible: share.showApple && share.appleShare.deliveries.length > 0
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: deliveriesScroll.availableWidth
                    Repeater {
                        model: share.visibleDeliveries
                        ColumnLayout {
                            id: deliveryRow
                            required property var modelData
                            Layout.fillWidth: true
                            Detail { text: modelData.title + " · " + share.destinationName(modelData.destination) }
                            Detail {
                                text: qsTr("File: %1 · %2").arg(modelData.file || "").arg((modelData.file || "").split(".").pop().toUpperCase())
                            }
                            Detail {
                                visible: text.length > 0
                                text: share.deliveryDate(modelData.created).length > 0
                                      ? qsTr("Creato il %1").arg(share.deliveryDate(modelData.created)) : ""
                            }
                            Detail {
                                text: modelData.state === "completed" ? qsTr("Importazione confermata")
                                    : modelData.state === "cancelled" ? qsTr("Annullata; file conservato")
                                    : modelData.state === "failed" ? modelData.error
                                    : qsTr("Da completare nell’app Apple")
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 5 * share.uiScale
                                Button {
                                    text: qsTr("Mostra nel Finder")
                                    enabled: !share.appleShare.busy
                                    onClicked: share.appleShare.revealDelivery(deliveryRow.modelData.id)
                                }
                                Button {
                                    visible: deliveryRow.modelData.state !== "completed" && deliveryRow.modelData.state !== "cancelled"
                                    text: qsTr("Ho importato il file")
                                    enabled: modelData.state === "awaitingConfirmation" && !share.appleShare.busy
                                    onClicked: share.appleShare.confirmDelivery(modelData.id)
                                }
                                Button {
                                    visible: deliveryRow.modelData.state !== "completed" && deliveryRow.modelData.state !== "cancelled"
                                    text: qsTr("Riapri")
                                    enabled: !share.appleShare.busy
                                    onClicked: share.appleShare.retryDelivery(modelData.id)
                                }
                                Button {
                                    visible: deliveryRow.modelData.state !== "completed" && deliveryRow.modelData.state !== "cancelled"
                                    text: qsTr("Annulla")
                                    enabled: !share.appleShare.busy
                                    onClicked: share.appleShare.cancelDelivery(modelData.id)
                                }
                            }
                        }
                    }
                }
            }

            Detail {
                visible: share.documentExchange.available && text.length > 0
                text: share.documentExchange.status
            }

            Button {
                visible: share.showApple && share.appleShare.deliveries.some(entry =>
                    entry.state === "completed" || entry.state === "cancelled")
                text: qsTr("Archivia completate")
                enabled: !share.appleShare.busy
                onClicked: share.appleShare.archiveCompletedDeliveries()
            }

            Button {
                id: done

                Layout.alignment: Qt.AlignRight
                text: qsTr("Chiudi")
                implicitHeight: 42 * share.uiScale
                leftPadding: 13 * share.uiScale
                rightPadding: leftPadding
                font.pixelSize: 15 * share.uiScale
                font.weight: Font.DemiBold
                onClicked: share.close()

                contentItem: Text {
                    text: done.text
                    color: share.blue
                    font: done.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: done.down ? "#dfeaff" : "#edf3ff"
                    border.color: share.blue
                    border.width: done.visualFocus ? 2 : 0
                    radius: 7 * share.uiScale
                }
            }
        }
    }
}
