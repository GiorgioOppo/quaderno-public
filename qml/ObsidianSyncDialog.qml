import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: dialog

    required property ObsidianSync sync
    required property real uiScale
    property string folderId: ""

    title: qsTr("Obsidian Sync")
    anchors.centerIn: Overlay.overlay
    width: Math.min(560 * uiScale, (parent ? parent.width : 560 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    modal: true

    function begin(destinationFolder) {
        folderId = destinationFolder
        sync.reload()
        open()
    }

    component Action: PaperButton {
        uiScale: dialog.uiScale
    }

    component Caption: Label {
        Layout.fillWidth: true
        color: "#535862"
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: 14 * dialog.uiScale
    }

    contentItem: ColumnLayout {
        spacing: 8 * dialog.uiScale

        Caption {
            visible: !dialog.sync.installed
            text: qsTr("Per collegare il vault, usa lo script di configurazione Obsidian Sync del pacchetto Paper Pro. Serve un abbonamento Obsidian Sync attivo.")
        }

        RowLayout {
            visible: dialog.sync.installed
            Layout.fillWidth: true
            Action {
                text: qsTr("Sincronizza ora")
                iconName: "sync"
                enabled: !dialog.sync.busy
                onClicked: dialog.sync.syncNow()
            }
            Item { Layout.fillWidth: true }
        }

        Caption {
            visible: text.length > 0
            text: dialog.sync.status
        }

        Caption {
            visible: dialog.sync.installed && dialog.sync.notes.length === 0 && !dialog.sync.busy
            text: qsTr("Le note del vault appariranno qui dopo la sincronizzazione.")
        }

        ListView {
            id: noteList
            visible: dialog.sync.installed && count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, Math.max(100 * dialog.uiScale,
                (dialog.parent ? dialog.parent.height : 560) - 255 * dialog.uiScale))
            clip: true
            spacing: 5 * dialog.uiScale
            model: dialog.sync.notes
            ScrollBar.vertical: ScrollBar { }

            delegate: ColumnLayout {
                id: noteRow
                required property var modelData
                width: Math.max(0, noteList.width - 14 * dialog.uiScale)
                spacing: 4 * dialog.uiScale

                Action {
                    text: noteRow.modelData.title
                    Accessible.name: qsTr("Apri %1").arg(text)
                    iconName: "text"
                    alignLeft: true
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    enabled: !dialog.sync.busy
                    onClicked: dialog.sync.openNote(noteRow.modelData.path, dialog.folderId)
                }
                Caption {
                    text: noteRow.modelData.path
                    Layout.leftMargin: 5 * dialog.uiScale
                }
                Caption {
                    visible: noteRow.modelData.conflict
                    text: qsTr("La nota cloud differisce dalla copia locale. Puoi importarla separatamente.")
                }
                Action {
                    visible: noteRow.modelData.conflict
                    text: qsTr("Importa copia dal cloud")
                    enabled: !dialog.sync.busy
                    onClicked: dialog.sync.openCloudCopy(noteRow.modelData.path, dialog.folderId)
                }
                Action {
                    visible: noteRow.modelData.linked && noteRow.modelData.localChanged && !noteRow.modelData.conflict
                    text: qsTr("Invia modifiche")
                    enabled: !dialog.sync.busy
                    onClicked: dialog.sync.sendNote(noteRow.modelData.path)
                }
            }
        }
    }

    footer: RowLayout {
        Item { Layout.fillWidth: true }
        Action {
            Layout.rightMargin: dialog.padding
            Layout.bottomMargin: dialog.padding
            text: qsTr("Chiudi")
            onClicked: dialog.close()
        }
    }
}
