pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: dialog
    objectName: "cloudImportDialog"
    required property Notebook notebook
    required property PortableDocuments portable
    required property real uiScale
    required property bool tabletMode
    property url localFile
    property string fileName: ""
    property string folderId: ""
    property string destinationBook: ""
    property string destinationPage: ""
    property string errorText: ""
    property var csv: ({})
    property bool importUsesFile: false
    readonly property string extension: fileName.split(".").pop().toLowerCase()
    readonly property bool imageFile: ["png", "jpg", "jpeg", "webp"].includes(extension)
    signal imported()
    signal libraryRequested()
    signal releaseDownload(url file, bool afterImport)
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520 * uiScale, (parent ? parent.width : 560 * uiScale) - 24 * uiScale)
    padding: 18 * uiScale
    modal: true
    dim: !tabletMode
    background: Rectangle {
        color: "white"
        radius: 16 * dialog.uiScale
        border.color: "#a4a4ac"
        border.width: Math.max(1, dialog.uiScale)
    }
    header: Label {
        text: dialog.title
        textFormat: Text.PlainText
        font.pixelSize: 22 * dialog.uiScale
        font.weight: Font.DemiBold
        padding: dialog.padding
        bottomPadding: 10 * dialog.uiScale
        wrapMode: Text.Wrap
    }
    title: qsTr("Importa dal cloud")
    function begin(file, name, folder) {
        localFile = file; fileName = name; folderId = folder
        destinationBook = notebook.bookId; destinationPage = notebook.pageId
        errorText = ""; csv = ({}); importUsesFile = false
        if (extension === "quaderno") {
            if (!portable.prepareImport(file)) errorText = portable.error
        } else if (extension === "csv") {
            csv = notebook.readTableCsv(file)
            errorText = csv.error || ""
        } else if (!["pdf", "epub", "md", "png", "jpg", "jpeg", "webp"].includes(extension))
            errorText = qsTr("Questo formato non è importabile in Quaderno.")
        if (imageFile && (!destinationBook || !destinationPage))
            errorText = qsTr("Apri prima la pagina in cui inserire l’immagine.")
        open()
    }
    function apply() {
        if (portable.locked || notebook.importing || errorText.length) return
        const title = fileName.replace(/\.[^.]+$/, "")
        let success = false
        if (extension === "quaderno") success = portable.applyImport()
        else if (extension === "csv") success = notebook.createBookWithItem(title, folderId,
            {type:"table", title:title, rows:csv.rows, cols:csv.cols, cells:csv.cells})
        else if (imageFile) success = notebook.importPageImage(localFile, destinationBook, destinationPage)
        else success = notebook.startDocumentImport(localFile, folderId)
        if (success) {
            importUsesFile = ["pdf", "epub"].includes(extension) && notebook.importing
            const showLibrary = ["pdf", "epub", "md", "quaderno"].includes(extension)
            close()
            if (showLibrary) libraryRequested()
            else imported()
        } else errorText = portable.error || notebook.error
    }
    onClosed: { releaseDownload(localFile, importUsesFile); portable.clear(); localFile = ""; csv = ({}); fileName = ""; errorText = "" }
    component Detail: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        font.pixelSize: 15 * dialog.uiScale
    }
    contentItem: ColumnLayout {
        spacing: 12 * dialog.uiScale
        Detail { text: dialog.fileName; font.bold: true }
        Detail {
            text: dialog.imageFile ? qsTr("Inserisci l’immagine nella pagina aperta: %1.").arg(dialog.notebook.title)
                : dialog.extension === "quaderno" ? qsTr("Nuovi: %1 · Aggiornati: %2 · Copie in conflitto: %3")
                    .arg(dialog.portable.preview.created || 0).arg(dialog.portable.preview.updated || 0).arg(dialog.portable.preview.conflicts || 0)
                : dialog.extension === "csv" ? qsTr("Crea una tabella modificabile: %1 righe, %2 colonne.").arg(dialog.csv.rows || 0).arg(dialog.csv.cols || 0)
                : qsTr("Aggiungi una nuova copia alla libreria. Le copie già annotate restano disponibili.")
        }
        Detail { visible: dialog.errorText.length > 0; text: dialog.errorText; color: "#942922" }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            PaperButton { uiScale: dialog.uiScale; text: qsTr("Annulla"); onClicked: dialog.close() }
            PaperButton {
                objectName: "confirmCloudImport"
                uiScale: dialog.uiScale; text: qsTr("Importa"); filled: true
                enabled: !dialog.portable.locked && !dialog.notebook.importing && !dialog.errorText.length
                onClicked: dialog.apply()
            }
        }
    }
}
