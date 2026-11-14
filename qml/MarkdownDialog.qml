import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Quaderno

Dialog {
    id: dialog

    required property Notebook notebook
    required property ObsidianIntegration obsidian
    required property MarkdownWeb web
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    required property bool cloudAvailable
    property string folderId: ""
    property string sourceBookId: ""
    property url vaultFolder: ""
    readonly property bool editingBook: sourceBookId.length > 0 && notebook.bookId === sourceBookId
    readonly property url publishLink: obsidian.publishBaseUrl.length > 0 ? obsidian.publishUrl(notePath.text) : ""
    property Item activeField: null
    property string exportError: ""

    signal exportRequested()
    signal cloudRequested()

    title: qsTr("Markdown e Obsidian")
    anchors.centerIn: Overlay.overlay
    width: Math.min(480 * uiScale, (parent ? parent.width : 480 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    modal: true
    dim: !tabletMode

    function begin(bookOpen, destinationFolder) {
        obsidian.clearLastSavedNote()
        sourceBookId = bookOpen ? notebook.bookId : ""
        exportError = ""
        folderId = destinationFolder
        noteName.text = editingBook ? obsidian.suggestedFileName(notebook.title) : ""
        siteAddress.text = obsidian.publishBaseUrl
        notePath.clear()
        webAddress.clear()
        open()
        scroll.contentItem.contentY = 0
    }

    onClosed: {
        web.cancel()
        activeField = null
        webAddress.clear()
        notePath.clear()
    }

    function clearPrivateState() {
        vaultPicker.close()
        close()
        vaultFolder = ""
        noteName.clear()
        exportError = ""
    }

    function showActiveField() {
        if (!tabletMode || !activeField || !activeField.activeFocus)
            return
        const area = scroll.contentItem
        const top = activeField.mapToItem(form, 0, 0).y
        const bottom = top + activeField.height
        if (top < area.contentY)
            area.contentY = top
        else if (bottom > area.contentY + scroll.availableHeight)
            area.contentY = bottom - scroll.availableHeight
    }

    component Action: PaperButton {
        uiScale: dialog.uiScale
    }

    component Message: Label {
        Layout.fillWidth: true
        color: "#535862"
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: 14 * dialog.uiScale
    }

    component Field: TextField {
        id: field
        Layout.fillWidth: true
        selectByMouse: !dialog.tabletMode
        onActiveFocusChanged: if (activeFocus) {
            dialog.activeField = field
            Qt.callLater(dialog.showActiveField)
        }
    }

    contentItem: ScrollView {
        id: scroll
        clip: true
        implicitHeight: Math.min(form.implicitHeight, Math.max(100 * dialog.uiScale,
            (dialog.parent ? dialog.parent.height : 500) - 92 * dialog.uiScale - actions.implicitHeight))
        contentWidth: availableWidth
        onHeightChanged: Qt.callLater(dialog.showActiveField)
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: form
            width: scroll.availableWidth
            spacing: 8 * dialog.uiScale

            Action {
                visible: dialog.cloudAvailable
                text: qsTr("Obsidian Sync")
                iconName: "sync"
                onClicked: dialog.cloudRequested()
            }

            Action {
                visible: dialog.editingBook
                text: qsTr("Esporta Markdown")
                iconName: "download"
                onClicked: dialog.exportRequested()
            }
            Message {
                visible: dialog.editingBook
                text: qsTr("L’esportazione Markdown salva immagini, grafici e tratti in una cartella accanto al file. Sposta il file e la cartella insieme.")
            }
            Message {
                visible: dialog.editingBook && text.length > 0
                text: dialog.exportError
                color: "#8c2d27"
            }

            Message {
                visible: dialog.editingBook && dialog.obsidian.available && !dialog.tabletMode
                Layout.topMargin: 7 * dialog.uiScale
                font.weight: Font.DemiBold
                text: qsTr("Vault Obsidian")
            }

            Action {
                visible: dialog.editingBook && dialog.obsidian.available && !dialog.tabletMode
                text: qsTr("Scegli cartella del vault")
                iconName: "folder"
                onClicked: vaultPicker.open()
            }
            Message {
                visible: dialog.editingBook && dialog.obsidian.available && !dialog.tabletMode && dialog.vaultFolder.toString().length > 0
                text: dialog.vaultFolder.toString()
            }
            Field {
                id: noteName
                visible: dialog.editingBook && dialog.obsidian.available && !dialog.tabletMode
                maximumLength: 240
                placeholderText: qsTr("Percorso della nota nel vault")
            }
            RowLayout {
                visible: dialog.editingBook && dialog.obsidian.available && !dialog.tabletMode
                Layout.fillWidth: true

                Action {
                    Layout.fillWidth: true
                    text: qsTr("Salva nel vault")
                    enabled: dialog.vaultFolder.toString().length > 0 && noteName.text.trim().length > 0
                    onClicked: dialog.obsidian.saveMarkdown(dialog.vaultFolder, noteName.text, dialog.notebook.markdownText())
                }
                Action {
                    Layout.fillWidth: true
                    text: qsTr("Apri in Obsidian")
                    enabled: dialog.obsidian.lastSavedFile.toString().length > 0
                    onClicked: dialog.obsidian.openSavedNote()
                }
            }

            Message {
                Layout.topMargin: 7 * dialog.uiScale
                text: qsTr("Obsidian Publish")
                font.weight: Font.DemiBold
            }
            Field {
                id: siteAddress
                maximumLength: 2048
                inputMethodHints: Qt.ImhUrlCharactersOnly
                placeholderText: qsTr("Indirizzo HTTPS del sito")
                onAccepted: dialog.obsidian.setPublishBaseUrl(text)
            }
            Action {
                text: qsTr("Salva indirizzo")
                enabled: siteAddress.text !== dialog.obsidian.publishBaseUrl
                onClicked: if (dialog.obsidian.setPublishBaseUrl(siteAddress.text))
                    siteAddress.text = dialog.obsidian.publishBaseUrl
            }
            Field {
                id: notePath
                visible: dialog.obsidian.publishBaseUrl.length > 0
                maximumLength: 1024
                placeholderText: qsTr("Percorso della nota pubblicata (facoltativo)")
            }
            TextField {
                visible: dialog.publishLink.toString().length > 0
                Layout.fillWidth: true
                text: dialog.publishLink.toString()
                readOnly: true
                selectByMouse: true
                Accessible.name: qsTr("Collegamento web")
            }
            RowLayout {
                visible: dialog.publishLink.toString().length > 0
                Action {
                    visible: dialog.obsidian.available && !dialog.tabletMode
                    text: qsTr("Apri nel browser")
                    onClicked: dialog.obsidian.openPublished(notePath.text)
                }
                Action {
                    text: qsTr("Copia collegamento")
                    onClicked: dialog.obsidian.copyPublished(notePath.text)
                }
            }
            Message {
                visible: text.length > 0
                text: dialog.obsidian.status
            }

            Message {
                Layout.topMargin: 7 * dialog.uiScale
                font.weight: Font.DemiBold
                text: qsTr("Importa Markdown dal web")
            }
            Message {
                text: qsTr("Usa il collegamento diretto al file Markdown.")
            }
            Field {
                id: webAddress
                maximumLength: 4096
                inputMethodHints: Qt.ImhUrlCharactersOnly
                placeholderText: qsTr("Indirizzo HTTPS del file .md")
                enabled: !dialog.web.busy
                onAccepted: if (text.trim().length > 0) dialog.web.importFromUrl(text, dialog.folderId)
            }
            Action {
                text: dialog.web.busy ? qsTr("Annulla download") : qsTr("Importa da URL")
                enabled: dialog.web.busy || webAddress.text.trim().length > 0
                onClicked: {
                    if (dialog.web.busy)
                        dialog.web.cancel()
                    else
                        dialog.web.importFromUrl(webAddress.text, dialog.folderId)
                }
            }
            Message {
                visible: text.length > 0
                text: dialog.web.status
            }
        }
    }

    footer: ColumnLayout {
        id: actions
        TouchKeyboard {
            visible: dialog.showTouchKeyboard && dialog.activeField !== null && dialog.activeField.activeFocus
            Layout.fillWidth: true
            Layout.leftMargin: dialog.padding
            Layout.rightMargin: dialog.padding
            uiScale: dialog.uiScale
            target: dialog.activeField
        }
        Action {
            Layout.alignment: Qt.AlignRight
            Layout.rightMargin: dialog.padding
            Layout.bottomMargin: dialog.padding
            text: qsTr("Chiudi")
            onClicked: dialog.close()
        }
    }

    FolderDialog {
        id: vaultPicker
        title: qsTr("Scegli la cartella del vault Obsidian")
        onAccepted: dialog.vaultFolder = selectedFolder
    }
}
