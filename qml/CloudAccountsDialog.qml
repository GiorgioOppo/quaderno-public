import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: dialog
    objectName: "cloudAccountsDialog"
    required property CloudAccounts cloud
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: false
    property string localError: ""
    property bool canUploadCurrent: true
    property bool keyboardOpen: false
    property string selectedFile: ""
    property var keyboardTarget: null
    readonly property var account: cloud ? cloud.accounts.find(item => item.key === cloud.provider) || ({}) : ({})
    readonly property bool accountConfigured: Boolean(account.configured)
    readonly property bool accountConnected: Boolean(account.connected)
    readonly property var selected: cloud ? cloud.entries.find(item => item.id === selectedFile) || ({}) : ({})
    readonly property var visibleEntries: cloud ? cloud.entries.filter(item => !search.text.trim()
        || item.name.toLocaleLowerCase().includes(search.text.trim().toLocaleLowerCase())) : []
    readonly property string signInLabel: !cloud ? qsTr("Accedi") : cloud.provider === "googleDrive"
        ? qsTr("Accedi con Google") : cloud.provider === "dropbox" ? qsTr("Accedi con Dropbox") : qsTr("Accedi con Microsoft")

    signal uploadRequested()
    signal uploadCurrentRequested()

    title: account.title || qsTr("Account cloud")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(880 * uiScale, (parent ? parent.width : 880 * uiScale) - 24 * uiScale)
    height: Math.min(1080 * uiScale, (parent ? parent.height : 1080 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    modal: true
    enabled: cloud !== null && !cloud.locked
    onClosed: {
        if (cloud) cloud.cancel()
        clearPrivateState()
    }

    function clearPrivateState() {
        code.clear()
        search.clear()
        selectedFile = ""
        keyboardOpen = false
        keyboardTarget = null
        disconnectDialog.close()
    }

    function begin(provider) {
        if (!cloud || cloud.locked || !cloud.selectProvider(provider)) return
        clearPrivateState()
        open()
        if (accountConnected) cloud.refreshFiles()
    }

    component Action: PaperButton {
        uiScale: dialog.uiScale
    }
    component Caption: Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: "#515159"
        font.pixelSize: 14 * dialog.uiScale
    }

    contentItem: ColumnLayout {
        spacing: 10 * dialog.uiScale

        ScrollView {
            id: contentScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: contentScroll.availableWidth
                spacing: 12 * dialog.uiScale

                Caption {
                    objectName: "cloudAccountStatus"
                    text: dialog.account.status || ""
                }
                Caption {
                    visible: !dialog.accountConfigured
                    text: qsTr("Il pulsante SSO sarà disponibile quando questa installazione avrà la registrazione ufficiale del servizio. Le credenziali dell’account si inseriscono soltanto nella pagina del fornitore.")
                }
                Caption {
                    visible: dialog.accountConfigured && !dialog.accountConnected
                    text: dialog.cloud && dialog.cloud.provider === "googleDrive"
                        ? qsTr("Google consente di leggere e scaricare i file esistenti e di creare nuove copie con Quaderno. I file originali non vengono sostituiti.")
                        : qsTr("Autorizza l’accesso ai file nella pagina ufficiale. Quaderno scarica i documenti e carica nuove copie, senza eliminare i file cloud.")
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8 * dialog.uiScale
                    Action {
                        objectName: "cloudSignIn"
                        text: dialog.signInLabel
                        filled: !dialog.accountConnected
                        enabled: dialog.accountConfigured && (dialog.cloud !== null) && !dialog.cloud.busy
                        onClicked: { code.clear(); dialog.cloud.beginAuthorization() }
                    }
                    Action {
                        text: qsTr("Rinnova accesso")
                        visible: dialog.accountConnected
                        enabled: (dialog.cloud !== null) && !dialog.cloud.busy
                        onClicked: dialog.cloud.refreshConnection()
                    }
                    Action {
                        objectName: "cloudDisconnect"
                        text: qsTr("Scollega")
                        visible: dialog.accountConnected
                        enabled: (dialog.cloud !== null) && !dialog.cloud.busy
                        onClicked: { revoke.checked = false; disconnectDialog.open() }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: (dialog.cloud !== null) && dialog.cloud.authPending
                    spacing: 8 * dialog.uiScale
                    Caption {
                        text: dialog.tabletMode && dialog.cloud && dialog.cloud.provider === "googleDrive"
                            ? qsTr("Sul Mac apri l’assistente di accesso al Paper Pro. L’assistente apre Google nel browser e completa il collegamento tramite SSH; i token restano sul tablet.")
                            : qsTr("Completa l’accesso nel browser ufficiale. Non inserire qui la password del tuo account.")
                    }
                    Caption {
                        objectName: "cloudDeviceCode"
                        visible: (dialog.cloud !== null) && dialog.cloud.userCode.length > 0
                        text: dialog.cloud ? qsTr("Codice da inserire nella pagina Microsoft: %1").arg(dialog.cloud.userCode) : ""
                        font.pixelSize: 20 * dialog.uiScale
                        font.weight: Font.DemiBold
                    }
                    TextArea {
                        objectName: "cloudAuthorizationLink"
                        Layout.fillWidth: true
                        visible: (dialog.cloud !== null) && (!dialog.tabletMode || dialog.cloud.provider !== "googleDrive")
                        readOnly: true
                        selectByMouse: true
                        textFormat: Text.PlainText
                        wrapMode: Text.WrapAnywhere
                        text: dialog.cloud ? dialog.cloud.authorizationUrl.toString() : ""
                        font.pixelSize: 12 * dialog.uiScale
                        Accessible.name: qsTr("Collegamento ufficiale di autorizzazione")
                    }
                    Action {
                        visible: !dialog.tabletMode
                        text: qsTr("Apri di nuovo il browser")
                        onClicked: dialog.cloud.openAuthorizationBrowser()
                    }
                    TextField {
                        id: code
                        objectName: "cloudDropboxCode"
                        Layout.fillWidth: true
                        visible: (dialog.cloud !== null) && dialog.cloud.provider === "dropbox"
                        placeholderText: qsTr("Codice di autorizzazione fornito da Dropbox")
                        maximumLength: 4096
                        echoMode: TextInput.Password
                        selectByMouse: true
                        Accessible.name: placeholderText
                        onActiveFocusChanged: if (activeFocus) { dialog.keyboardTarget = code; dialog.keyboardOpen = true }
                        onAccepted: { dialog.cloud.submitAuthorizationCode(text); clear(); dialog.keyboardOpen = false }
                    }
                    Action {
                        visible: code.visible
                        text: qsTr("Conferma codice Dropbox")
                        enabled: code.text.trim().length > 0
                        onClicked: { dialog.cloud.submitAuthorizationCode(code.text); code.clear(); dialog.keyboardOpen = false }
                    }
                }

                Caption {
                    objectName: "cloudTransferStatus"
                    visible: text.length > 0
                    text: dialog.localError || (dialog.cloud ? dialog.cloud.error || dialog.cloud.status : "")
                    color: dialog.localError || (dialog.cloud && dialog.cloud.error) ? "#942922" : "#515159"
                    Accessible.name: text
                }
                ProgressBar {
                    Layout.fillWidth: true
                    visible: (dialog.cloud !== null) && dialog.cloud.busy && !dialog.cloud.authPending
                    indeterminate: (dialog.cloud !== null) && dialog.cloud.progress <= 0
                    value: dialog.cloud ? dialog.cloud.progress : 0
                }

                Caption {
                    visible: dialog.accountConnected
                    text: dialog.cloud ? dialog.cloud.currentPath : ""
                    font.weight: Font.DemiBold
                }
                Flow {
                    visible: dialog.accountConnected
                    Layout.fillWidth: true
                    spacing: 8 * dialog.uiScale
                    Action {
                        objectName: "cloudRefreshFiles"
                        text: qsTr("Aggiorna file")
                        enabled: (dialog.cloud !== null) && !dialog.cloud.busy
                        onClicked: { dialog.selectedFile = ""; dialog.cloud.refreshFiles() }
                    }
                    Action {
                        text: qsTr("Cartella superiore")
                        enabled: (dialog.cloud !== null) && dialog.cloud.canGoUp && !dialog.cloud.busy
                        onClicked: { dialog.selectedFile = ""; dialog.cloud.goUp() }
                    }
                    Action {
                        objectName: "cloudUploadCurrent"
                        text: qsTr("Carica questo quaderno")
                        enabled: (dialog.cloud !== null) && !dialog.cloud.busy && dialog.canUploadCurrent
                        onClicked: dialog.uploadCurrentRequested()
                    }
                    Action {
                        visible: !dialog.tabletMode
                        text: qsTr("Carica un file…")
                        enabled: (dialog.cloud !== null) && !dialog.cloud.busy
                        onClicked: dialog.uploadRequested()
                    }
                }
                TextField {
                    id: search
                    objectName: "cloudFileSearch"
                    Layout.fillWidth: true
                    visible: dialog.accountConnected
                    placeholderText: qsTr("Cerca nei file caricati in elenco")
                    maximumLength: 512
                    selectByMouse: true
                    Accessible.name: placeholderText
                    onActiveFocusChanged: if (activeFocus) { dialog.keyboardTarget = search; dialog.keyboardOpen = true }
                    onAccepted: dialog.keyboardOpen = false
                }
                ListView {
                    id: files
                    objectName: "cloudFileList"
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, 360 * dialog.uiScale)
                    visible: dialog.accountConnected
                    clip: true
                    model: dialog.visibleEntries
                    spacing: 4 * dialog.uiScale
                    delegate: ItemDelegate {
                        id: fileRow
                        required property var modelData
                        width: files.width
                        height: 72 * dialog.uiScale
                        highlighted: dialog.selectedFile === modelData.id
                        enabled: (dialog.cloud !== null) && !dialog.cloud.busy
                        Accessible.name: modelData.name
                        contentItem: ColumnLayout {
                            Label {
                                Layout.fillWidth: true
                                text: fileRow.modelData.name
                                textFormat: Text.PlainText
                                elide: Text.ElideMiddle
                                font.pixelSize: 16 * dialog.uiScale
                            }
                            Label {
                                Layout.fillWidth: true
                                text: fileRow.modelData.folder ? qsTr("Cartella · tocca per aprire")
                                    : !fileRow.modelData.canDownload ? qsTr("File oltre il limite di trasferimento")
                                    : fileRow.modelData.size < 0 ? qsTr("Dimensione non disponibile")
                                    : qsTr("%1 MiB").arg((fileRow.modelData.size / 1048576).toFixed(2))
                                textFormat: Text.PlainText
                                color: "#65656d"
                                font.pixelSize: 12 * dialog.uiScale
                            }
                        }
                        onClicked: {
                            if (modelData.folder) { dialog.selectedFile = ""; dialog.cloud.enterFolder(modelData.id) }
                            else dialog.selectedFile = modelData.id
                        }
                    }
                }
                Action {
                    text: qsTr("Carica altri risultati")
                    visible: (dialog.cloud !== null) && dialog.cloud.hasMore
                    enabled: (dialog.cloud !== null) && !dialog.cloud.busy
                    onClicked: dialog.cloud.loadMore()
                }
                Action {
                    objectName: "cloudDownloadSelected"
                    text: qsTr("Scarica e importa")
                    visible: dialog.accountConnected
                    enabled: (dialog.cloud !== null) && !dialog.cloud.busy && dialog.selected.canDownload === true
                    onClicked: dialog.cloud.downloadFile(dialog.selectedFile)
                }
                Caption {
                    visible: dialog.accountConnected
                    text: qsTr("PDF, EPUB, Markdown, CSV, immagini e .quaderno. Massimo 100 MiB per file e 64 MiB per un quaderno. I trasferimenti sono manuali e creano nuove copie; non sincronizzano automaticamente le modifiche.")
                }
            }
        }

        Action {
            visible: dialog.showTouchKeyboard && dialog.keyboardOpen
            text: qsTr("Nascondi tastiera")
            onClicked: dialog.keyboardOpen = false
        }
        TouchKeyboard {
            Layout.fillWidth: true
            visible: dialog.showTouchKeyboard && dialog.keyboardOpen && dialog.keyboardTarget !== null
            uiScale: dialog.uiScale
            target: dialog.keyboardTarget
        }
        RowLayout {
            Layout.fillWidth: true
            Action {
                objectName: "cloudCancelOperation"
                visible: (dialog.cloud !== null) && dialog.cloud.busy
                text: qsTr("Annulla operazione")
                onClicked: { dialog.cloud.cancel(); code.clear() }
            }
            Item { Layout.fillWidth: true }
            Action { text: qsTr("Chiudi"); onClicked: dialog.close() }
        }
    }

    Dialog {
        id: disconnectDialog
        title: qsTr("Scollega account")
        anchors.centerIn: Overlay.overlay
        width: Math.min(560 * dialog.uiScale, (parent ? parent.width : 560 * dialog.uiScale) - 24 * dialog.uiScale)
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: ColumnLayout {
            Caption { text: qsTr("Rimuove l’accesso salvato su questo dispositivo. I file locali e cloud restano disponibili.") }
            CheckBox {
                id: revoke
                text: qsTr("Revoca anche l’autorizzazione presso il servizio")
                visible: (dialog.cloud !== null) && dialog.cloud.provider !== "oneDrive"
            }
            Caption {
                visible: revoke.visible && revoke.checked && (dialog.cloud !== null) && dialog.cloud.provider === "googleDrive"
                text: qsTr("Google può revocare anche gli altri accessi concessi allo stesso progetto. Per rimuovere solo questo dispositivo, lascia l’opzione disattivata.")
            }
            Caption {
                visible: (dialog.cloud !== null) && dialog.cloud.provider === "oneDrive"
                text: qsTr("Per revocare l’autorizzazione Microsoft, usa la gestione delle app nella pagina del tuo account.")
            }
        }
        onAccepted: dialog.cloud.disconnectAccount(revoke.visible && revoke.checked)
    }
}
