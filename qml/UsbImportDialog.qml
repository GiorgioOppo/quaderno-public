pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: dialog

    required property Notebook notebook
    required property UsbSsh usbSsh
    required property real uiScale

    readonly property var preview: usbSsh.importPreview
    readonly property bool hasPreview: preview.created !== undefined
    readonly property bool hasChanges: hasPreview && (preview.created + preview.updated + preview.conflicts > 0 || preview.foldersCreated > 0)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(470 * uiScale, parent ? parent.width - 24 * uiScale : 470 * uiScale)
    padding: 16 * uiScale
    spacing: 12 * uiScale
    modal: true
    closePolicy: Popup.CloseOnEscape
    title: qsTr("Importa dal Paper Pro")

    function openPreview() {
        open()
        usbSsh.importFromPaperPro(notebook)
    }

    onClosed: usbSsh.cancelImport()

    component ImportLabel: Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        textFormat: Text.PlainText
        color: "#24262c"
        font.pixelSize: 15 * dialog.uiScale
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

        ImportLabel {
            text: qsTr("Paper Pro → Mac via USB-C")
            color: "#555b67"
            font.weight: Font.DemiBold
        }

        GridLayout {
            columns: dialog.width < 360 * dialog.uiScale ? 1 : 2
            columnSpacing: 24 * dialog.uiScale
            rowSpacing: 6 * dialog.uiScale
            visible: dialog.hasPreview
            Layout.fillWidth: true

            ImportLabel { text: qsTr("Quaderni nuovi: %1").arg(dialog.preview.created || 0) }
            ImportLabel { text: qsTr("Quaderni aggiornati: %1").arg(dialog.preview.updated || 0) }
            ImportLabel { text: qsTr("Quaderni invariati: %1").arg(dialog.preview.unchanged || 0) }
            ImportLabel { text: qsTr("Copie in conflitto: %1").arg(dialog.preview.conflicts || 0) }
        }

        ImportLabel {
            visible: dialog.hasPreview && dialog.preview.foldersCreated > 0
            text: qsTr("Cartelle nuove: %1").arg(dialog.preview.foldersCreated || 0)
        }

        ImportLabel {
            visible: dialog.hasPreview && dialog.preview.conflicts > 0
            text: qsTr("Quaderno conserverà le versioni sul Mac e aggiungerà le copie dal Paper Pro.")
        }

        ImportLabel {
            visible: dialog.usbSsh.importStatus.length > 0
            text: dialog.usbSsh.importStatus
            color: dialog.usbSsh.importBusy || dialog.usbSsh.importCompleted ? "#24262c" : "#87332c"
        }
    }

    footer: RowLayout {
        spacing: 7 * dialog.uiScale
        Item { Layout.fillWidth: true }

        PaperButton {
            uiScale: dialog.uiScale
            Layout.bottomMargin: dialog.padding
            Layout.rightMargin: confirm.visible ? 0 : dialog.padding
            text: dialog.usbSsh.importCompleted ? qsTr("Chiudi") : qsTr("Annulla")
            quiet: true
            onClicked: dialog.close()
        }

        PaperButton {
            id: confirm
            uiScale: dialog.uiScale
            Layout.bottomMargin: dialog.padding
            Layout.rightMargin: dialog.padding
            visible: !dialog.usbSsh.importCompleted && !dialog.usbSsh.importBusy
            text: !dialog.hasPreview ? qsTr("Riprova")
                : dialog.hasChanges ? qsTr("Importa quaderni") : qsTr("Conferma")
            filled: true
            onClicked: {
                if (dialog.hasPreview)
                    dialog.usbSsh.confirmImport()
                else
                    dialog.usbSsh.importFromPaperPro(dialog.notebook)
            }
        }
    }
}
