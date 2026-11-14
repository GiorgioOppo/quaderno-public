pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: dialog

    required property Notebook notebook
    required property UsbTransfer transfer
    required property real uiScale

    property string bookId: ""
    readonly property var preview: transfer.previewInfo
    readonly property string action: preview.action || ""

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(510 * uiScale, parent ? parent.width - 24 * uiScale : 510 * uiScale)
    padding: 16 * uiScale
    spacing: 12 * uiScale
    modal: true
    closePolicy: transfer.applying ? Popup.NoAutoClose : Popup.CloseOnEscape
    title: qsTr("Invia al Paper Pro")

    function openFor(id: string) {
        if (!transfer.available || transfer.busy)
            return
        bookId = id
        open()
        transfer.preview(notebook, bookId)
    }

    onClosed: if (!transfer.applying) transfer.cancel()

    function actionDescription() {
        switch (action) {
        case "create": return qsTr("Sul Paper Pro verrà creato un nuovo quaderno.")
        case "replace": return qsTr("Il contenuto sul Paper Pro verrà aggiornato con la versione del Mac.")
        case "conflict": return qsTr("Le versioni differiscono. Il Paper Pro conserverà il quaderno esistente e aggiungerà una copia separata.")
        case "unchanged": return qsTr("Il contenuto del quaderno coincide sui due dispositivi.")
        default: return ""
        }
    }

    component TransferLabel: Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: "#24262c"
        font.pixelSize: 15 * dialog.uiScale
        textFormat: Text.PlainText
    }

    background: Rectangle {
        color: "white"
        border.color: "#d9dce2"
        radius: 14 * dialog.uiScale
    }

    header: Label {
        text: dialog.title
        color: "#24262c"
        font.weight: Font.DemiBold
        font.pixelSize: 22 * dialog.uiScale
        padding: dialog.padding
        bottomPadding: 0
    }

    contentItem: ColumnLayout {
        spacing: 10 * dialog.uiScale

        TransferLabel {
            text: qsTr("Mac → Paper Pro via USB-C")
            color: "#555b67"
            font.weight: Font.DemiBold
        }

        TransferLabel {
            visible: dialog.action.length > 0 && !dialog.transfer.applying
            text: dialog.actionDescription()
        }

        TransferLabel {
            visible: dialog.action.length > 0 && !dialog.transfer.applying
            text: qsTr("Mac · %1 · %n pagina/e", "", dialog.preview.sourcePages || 0).arg(dialog.preview.sourceTitle || "")
        }

        TransferLabel {
            visible: Boolean(dialog.preview.targetTitle) && !dialog.transfer.applying
            text: qsTr("Paper Pro · %1 · %n pagina/e", "", dialog.preview.targetPages || 0).arg(dialog.preview.targetTitle || "")
        }

        TransferLabel {
            visible: (dialog.action === "replace" || dialog.action === "conflict") && !dialog.transfer.applying
            text: qsTr("Pagine con modifiche: %1").arg(dialog.preview.changedPages || 0)
        }

        TransferLabel {
            visible: dialog.action.length > 0 && !dialog.transfer.busy
            text: qsTr("Chiudi Quaderno sul Paper Pro prima di confermare.")
            color: "#555b67"
            font.pixelSize: 14 * dialog.uiScale
        }

        TransferLabel {
            visible: text.length > 0
            text: dialog.transfer.status
            color: dialog.transfer.busy || dialog.transfer.completed ? "#24262c" : "#87332c"
        }
    }

    footer: RowLayout {
        spacing: 7 * dialog.uiScale
        Item { Layout.fillWidth: true }

        PaperButton {
            uiScale: dialog.uiScale
            Layout.bottomMargin: dialog.padding
            enabled: !dialog.transfer.applying
            text: dialog.transfer.busy ? qsTr("Annulla") : qsTr("Chiudi")
            quiet: true
            onClicked: dialog.close()
        }

        PaperButton {
            uiScale: dialog.uiScale
            Layout.bottomMargin: dialog.padding
            Layout.rightMargin: dialog.padding
            visible: !dialog.transfer.completed && !dialog.transfer.busy
            text: dialog.action.length === 0 ? qsTr("Riprova")
                : dialog.action === "unchanged" ? qsTr("Conferma") : qsTr("Invia quaderno")
            filled: true
            onClicked: {
                if (dialog.action.length === 0)
                    dialog.transfer.preview(dialog.notebook, dialog.bookId)
                else
                    dialog.transfer.apply()
            }
        }
    }
}
