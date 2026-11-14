import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: dialog
    required property WirelessSync wireless
    required property Notebook notebook
    required property real uiScale
    property string syncLabel: ""
    property bool bluetoothOnly: false
    signal resumeOpened()
    readonly property BleSync transfer: wireless.transfer
    readonly property var summary: transfer.previewInfo
    readonly property string action: summary.action || ""
    readonly property var offer: transfer.resumeOffer
    readonly property var bookState: {
        const version = transfer.bookStatusVersion
        return notebook.bookId.length ? transfer.bookStatus(notebook.bookId) : ({})
    }
    readonly property bool codeVisible: !wireless.paired && (!wireless.tablet || wireless.code.length > 0)

    parent: Overlay.overlay
    anchors.centerIn: parent
    title: bluetoothOnly ? qsTr("Sincronizzazione Bluetooth") : qsTr("Sincronizzazione wireless")
    width: Math.min(480 * uiScale, parent.width - 24 * uiScale)
    modal: true
    padding: 16 * uiScale

    onClosed: {
        codeInput.clear()
        forgetConfirmation.visible = false
    }

    function begin(bluetooth = false) {
        bluetoothOnly = bluetooth && (!wireless.active || wireless.bluetoothActive)
        open()
        scroll.contentItem.contentY = 0
        if (!bluetooth && wireless.tablet && !wireless.paired && !wireless.active && !wireless.code)
            wireless.preparePairing()
    }

    function describeAction() {
        switch (action) {
        case "create": return qsTr("Il quaderno verrà aggiunto al dispositivo collegato.")
        case "replace": return qsTr("Il quaderno sul dispositivo collegato verrà aggiornato.")
        case "conflict": return qsTr("Le versioni differiscono. Verrà aggiunta una copia separata.")
        case "unchanged": return qsTr("Le due copie coincidono.")
        default: return ""
        }
    }

    component Note: Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: "#24262c"
        font.pixelSize: 15 * dialog.uiScale
    }

    component SyncButton: PaperButton {
        uiScale: dialog.uiScale
    }

    header: Note {
        text: dialog.title
        font.pixelSize: 22 * dialog.uiScale
        font.weight: Font.DemiBold
        padding: dialog.padding
        bottomPadding: 0
    }

    contentItem: ScrollView {
        id: scroll
        clip: true
        implicitHeight: Math.min(body.implicitHeight, Math.max(100, dialog.parent.height - 140 * dialog.uiScale))
        contentWidth: availableWidth
        contentHeight: body.implicitHeight
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: body
            width: scroll.availableWidth
            spacing: 9 * dialog.uiScale

            Note {
                visible: Boolean(dialog.offer.pageId)
                text: qsTr("Apri «%1» a pagina %2?").arg(dialog.offer.title || "").arg(dialog.offer.page || 0)
                font.weight: Font.DemiBold
            }
            RowLayout {
                visible: Boolean(dialog.offer.pageId)
                SyncButton {
                    text: qsTr("Ignora")
                    onClicked: dialog.transfer.dismissResume()
                }
                SyncButton {
                    text: qsTr("Apri pagina")
                    filled: true
                    enabled: dialog.transfer.canAcceptResume
                    onClicked: {
                        if (dialog.transfer.acceptResume()) {
                            dialog.close()
                            dialog.resumeOpened()
                        }
                    }
                }
            }

            Note {
                visible: dialog.wireless.tablet && !dialog.wireless.paired
                text: dialog.bluetoothOnly
                      ? (dialog.codeVisible ? qsTr("Sul Mac, apri Quaderno e inserisci questo codice nella sincronizzazione Bluetooth.")
                                            : qsTr("Avvia il Bluetooth per ottenere il codice da inserire in Quaderno sul Mac."))
                      : qsTr("Inserisci questo codice sul Mac e scegli lo stesso tipo di collegamento.")
            }
            Note {
                visible: dialog.wireless.tablet && dialog.codeVisible
                text: dialog.wireless.code
                font.family: "monospace"
                font.weight: Font.DemiBold
                font.pixelSize: (dialog.width < 410 * dialog.uiScale ? 12 : 15) * dialog.uiScale
            }
            SyncButton {
                visible: !dialog.bluetoothOnly && dialog.wireless.tablet && !dialog.wireless.paired && !dialog.codeVisible
                text: qsTr("Genera codice")
                onClicked: dialog.wireless.preparePairing()
            }
            TextField {
                id: codeInput
                visible: !dialog.wireless.tablet && dialog.codeVisible
                Layout.fillWidth: true
                placeholderText: qsTr("Codice mostrato sul Paper Pro")
                maximumLength: 42
                echoMode: TextInput.PasswordEchoOnEdit
                selectByMouse: true
                enabled: !dialog.wireless.active
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
            }
            TextField {
                id: addressInput
                visible: !dialog.bluetoothOnly && !dialog.wireless.tablet && !dialog.wireless.active
                Layout.fillWidth: true
                placeholderText: qsTr("Indirizzo Wi-Fi del Paper Pro")
                maximumLength: 15
                selectByMouse: true
            }
            RowLayout {
                visible: !dialog.wireless.active
                Layout.fillWidth: true

                SyncButton {
                    Layout.fillWidth: true
                    text: dialog.bluetoothOnly
                          ? (dialog.wireless.tablet ? qsTr("Avvia Bluetooth") : qsTr("Collega il Paper Pro"))
                          : qsTr("Bluetooth")
                    enabled: !dialog.wireless.activatingBluetooth
                             && (dialog.wireless.paired || dialog.wireless.tablet || codeInput.length > 0)
                    onClicked: dialog.wireless.startBluetooth(codeInput.text)
                }
                SyncButton {
                    Layout.fillWidth: true
                    visible: !dialog.bluetoothOnly
                    text: qsTr("Wi-Fi locale")
                    enabled: !dialog.wireless.activatingBluetooth
                             && (dialog.wireless.paired || (dialog.wireless.tablet ? dialog.codeVisible : codeInput.length > 0))
                             && (dialog.wireless.tablet || addressInput.length > 0)
                    onClicked: dialog.wireless.startWifi(addressInput.text, codeInput.text)
                }
            }
            Note {
                visible: !dialog.bluetoothOnly && dialog.wireless.tablet && dialog.wireless.address.length > 0
                text: qsTr("Indirizzo da inserire sul Mac: %1").arg(dialog.wireless.address)
            }
            Note {
                visible: text.length > 0
                text: dialog.wireless.status
                color: "#555b67"
            }
            SyncButton {
                visible: dialog.wireless.active
                enabled: !dialog.transfer.applying
                text: qsTr("Disconnetti")
                onClicked: dialog.wireless.stop()
            }

            CheckBox {
                visible: dialog.wireless.paired
                text: qsTr("Sincronizza automaticamente")
                checked: dialog.transfer.autoSync
                onClicked: dialog.transfer.autoSync = checked
            }
            CheckBox {
                visible: dialog.wireless.paired && dialog.notebook.bookId.length > 0
                text: qsTr("Sincronizza questo quaderno")
                checked: dialog.transfer.pinnedBooks.indexOf(dialog.notebook.bookId) !== -1
                onClicked: dialog.transfer.setPinned(dialog.notebook.bookId, checked)
            }
            Note {
                visible: dialog.syncLabel.length > 0
                text: dialog.syncLabel
                color: "#555b67"
                font.weight: Font.DemiBold
            }
            Note {
                visible: Boolean(dialog.bookState.lastSuccess) && dialog.syncLabel.length > 0
                text: visible ? qsTr("Ultima sincronizzazione riuscita · %1")
                    .arg(Qt.formatDateTime(dialog.bookState.lastSuccess, Qt.locale().dateTimeFormat(Locale.ShortFormat))) : ""
                color: "#555b67"
            }
            Note {
                visible: text.length > 0 && text !== dialog.transfer.status
                text: dialog.bookState.detail || ""
                color: "#87332c"
            }
            SyncButton {
                visible: dialog.transfer.connected && dialog.notebook.bookId.length > 0
                enabled: !dialog.transfer.busy && dialog.action.length === 0
                text: qsTr("Anteprima invio quaderno")
                onClicked: dialog.transfer.preview(dialog.notebook.bookId)
            }
            SyncButton {
                visible: dialog.transfer.connected && dialog.notebook.pageId.length > 0
                enabled: !dialog.transfer.busy && !dialog.transfer.resumeBusy && dialog.action.length === 0
                text: dialog.wireless.tablet ? qsTr("Continua sul Mac") : qsTr("Continua sul Paper Pro")
                onClicked: dialog.transfer.sendResume(dialog.notebook.bookId, dialog.notebook.pageId)
            }
            Note {
                visible: text.length > 0
                text: dialog.transfer.resumeStatus
                color: "#555b67"
            }
            SyncButton {
                visible: dialog.transfer.resumeBusy && dialog.action.length === 0
                enabled: !dialog.transfer.applying
                quiet: true
                text: qsTr("Annulla richiesta di apertura")
                onClicked: dialog.transfer.cancelResume()
            }
            Note {
                visible: text.length > 0
                text: dialog.transfer.status
                color: "#555b67"
            }
            Note {
                visible: dialog.action.length > 0
                text: dialog.describeAction()
            }
            Note {
                visible: dialog.action.length > 0
                text: qsTr("%1 · Pagine con modifiche: %2").arg(dialog.summary.sourceTitle || "")
                    .arg(dialog.summary.changedPages || 0)
            }
            RowLayout {
                visible: dialog.action.length > 0
                SyncButton {
                    text: qsTr("Annulla")
                    enabled: !dialog.transfer.applying
                    onClicked: dialog.transfer.cancel()
                }
                SyncButton {
                    text: qsTr("Conferma invio")
                    filled: true
                    enabled: !dialog.transfer.busy
                    onClicked: dialog.transfer.apply()
                }
            }
            SyncButton {
                visible: dialog.wireless.paired && !forgetConfirmation.visible
                enabled: !dialog.transfer.applying && !dialog.wireless.activatingBluetooth
                quiet: true
                text: qsTr("Rimuovi abbinamento")
                onClicked: forgetConfirmation.visible = true
            }
            RowLayout {
                id: forgetConfirmation
                visible: false
                SyncButton {
                    text: qsTr("Annulla")
                    onClicked: forgetConfirmation.visible = false
                }
                SyncButton {
                    text: qsTr("Conferma rimozione")
                    onClicked: {
                        dialog.wireless.forget()
                        forgetConfirmation.visible = false
                    }
                }
            }
        }
    }

    footer: RowLayout {
        Item { Layout.fillWidth: true }
        SyncButton {
            Layout.rightMargin: dialog.padding
            Layout.bottomMargin: dialog.padding
            text: qsTr("Chiudi")
            onClicked: dialog.close()
        }
    }
}
