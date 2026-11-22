import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: dialog
    objectName: "obsidianSyncDialog"

    required property ObsidianSync sync
    required property real uiScale
    property string folderId: ""
    property bool showTouchKeyboard: false
    property bool searchKeyboardOpen: false
    property int noteFilter: 0
    readonly property var visibleNotes: {
        const query = searchInput.text.trim().toLocaleLowerCase()
        return sync.notes.filter(note => (!query || (note.title + " " + note.path).toLocaleLowerCase().includes(query))
            && (noteFilter === 1 ? note.conflict : noteFilter === 2 ? note.linked && note.localChanged : true))
    }
    readonly property int conflictCount: sync.notes.filter(note => note.conflict).length
    readonly property int changedCount: sync.notes.filter(note => note.linked && note.localChanged).length

    title: qsTr("Obsidian Sync")
    anchors.centerIn: Overlay.overlay
    width: Math.min((showTouchKeyboard ? 760 : 560) * uiScale, (parent ? parent.width : 560 * uiScale) - 24 * uiScale)
    height: Math.min(740 * uiScale, (parent ? parent.height : 740 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    modal: true
    onClosed: searchKeyboardOpen = false

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

        RowLayout {
            visible: dialog.sync.installed
            Layout.fillWidth: true
            TextField {
                id: searchInput
                objectName: "obsidianSearchInput"
                Layout.fillWidth: true
                placeholderText: qsTr("Cerca per titolo o percorso")
                Accessible.name: placeholderText
                selectByMouse: true
                maximumLength: 512
                onActiveFocusChanged: if (activeFocus) dialog.searchKeyboardOpen = true
                onAccepted: dialog.searchKeyboardOpen = false
            }
            Action {
                visible: searchInput.text.length > 0
                text: qsTr("Cancella ricerca")
                onClicked: searchInput.clear()
            }
        }
        Flow {
            visible: dialog.sync.installed
            Layout.fillWidth: true
            spacing: 5 * dialog.uiScale
            Repeater {
                model: [qsTr("Tutte"), qsTr("Conflitti (%1)").arg(dialog.conflictCount), qsTr("Modificate (%1)").arg(dialog.changedCount)]
                Action {
                    required property string modelData
                    required property int index
                    text: modelData
                    filled: dialog.noteFilter === index
                    checkable: true
                    checked: dialog.noteFilter === index
                    onClicked: dialog.noteFilter = index
                }
            }
        }
        Caption {
            visible: dialog.sync.installed && dialog.sync.notes.length > 0
            text: dialog.visibleNotes.length > 0 ? qsTr("Note mostrate: %1 / %2").arg(dialog.visibleNotes.length).arg(dialog.sync.notes.length)
                                               : qsTr("Nessuna nota corrisponde. Modifica la ricerca o scegli Tutte.")
        }

        Caption {
            visible: dialog.sync.installed && dialog.sync.notes.length === 0 && !dialog.sync.busy
            text: qsTr("Le note del vault appariranno qui dopo la sincronizzazione.")
        }

        ListView {
            id: noteList
            objectName: "obsidianNoteList"
            visible: dialog.sync.installed
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 60 * dialog.uiScale
            clip: true
            spacing: 5 * dialog.uiScale
            model: dialog.visibleNotes
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
        Action {
            visible: dialog.showTouchKeyboard && dialog.searchKeyboardOpen
            text: qsTr("Nascondi tastiera")
            onClicked: dialog.searchKeyboardOpen = false
        }
        TouchKeyboard {
            objectName: "obsidianSearchKeyboard"
            visible: dialog.showTouchKeyboard && dialog.searchKeyboardOpen && dialog.sync.installed
            target: searchInput
            uiScale: dialog.uiScale
            Layout.fillWidth: true
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
