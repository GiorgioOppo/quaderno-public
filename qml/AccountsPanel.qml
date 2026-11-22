pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    objectName: "accountsPanel"

    required property real uiScale
    required property bool tabletMode
    property bool pinLocked: false
    property var obsidianSync: null
    property var wireless: null
    property var cloudAccounts: null
    readonly property bool browserAvailable: !tabletMode && Qt.platform.os === "osx"
    readonly property bool syncAvailable: Boolean(obsidianSync?.available)
    readonly property bool wirelessAvailable: Boolean(wireless?.available)
    property string browserStatus: ""
    property bool browserError: false

    signal openObsidianSync()
    signal openMarkdown()
    signal openApple()
    signal openAppleBooks()
    signal importFiles()
    signal openWireless()
    signal openCloudAccount(string provider)

    spacing: 12 * uiScale
    enabled: !pinLocked
    onPinLockedChanged: if (pinLocked) clearFeedback()

    function clearFeedback() {
        browserStatus = ""
        browserError = false
    }

    // Accept service identifiers only: account links never come from notebook content.
    function serviceUrl(service: string): string {
        const urls = {
            googleDrive: "https://drive.google.com/",
            dropbox: "https://www.dropbox.com/",
            oneDrive: "https://onedrive.live.com/",
            icloud: "https://www.icloud.com/",
            obsidian: "https://obsidian.md/"
        }
        return Object.prototype.hasOwnProperty.call(urls, service) ? urls[service] : ""
    }

    function launchService(service: string): bool {
        if (pinLocked) return false
        clearFeedback()
        const url = serviceUrl(service)
        if (!browserAvailable || !url) {
            browserError = true
            browserStatus = browserAvailable ? qsTr("Servizio web non disponibile.")
                                             : qsTr("Apri il sito dal browser del Mac.")
            return false
        }
        let launched = false
        try { launched = Qt.openUrlExternally(url) } catch (error) { launched = false }
        browserError = !launched
        browserStatus = launched
            ? qsTr("Richiesta inviata al browser del Mac. Completa lì l’accesso; Quaderno non rileva lo stato dell’account.")
            : qsTr("Impossibile aprire il browser. Apri %1 sul Mac e riprova.").arg(url)
        return launched
    }

    function dispatchAction(action: string, service: string) {
        if (pinLocked) return
        if (action === "website") launchService(service)
        else if (action === "obsidianSync" && syncAvailable) openObsidianSync()
        else if (action === "markdown") openMarkdown()
        else if (action === "apple" && browserAvailable) openApple()
        else if (action === "appleBooks" && browserAvailable) openAppleBooks()
        else if (action === "import" && browserAvailable) importFiles()
        else if (action === "wireless" && wirelessAvailable) openWireless()
        else if (action === "cloud" && cloudAccounts) openCloudAccount(service)
    }

    readonly property var services: [
        {
            key: "obsidianSync", title: "Obsidian Sync",
            state: syncAvailable ? obsidianSync.busy ? qsTr("Sincronizzazione in corso…")
                : obsidianSync.installed ? qsTr("Configurato sul Paper Pro") : qsTr("Da configurare dal Mac")
                : qsTr("Sincronizzazione diretta sul Paper Pro"),
            description: syncAvailable
                ? qsTr("Sincronizza le note Markdown con il vault via Wi-Fi. Le annotazioni, le tabelle e i grafici restano in Quaderno. L’accesso al vault viene verificato durante la sincronizzazione.")
                : qsTr("Configura il vault sul Paper Pro per sincronizzarlo direttamente. Sul Mac puoi usare il vault locale dal pannello Markdown e Obsidian."),
            detail: syncAvailable && !pinLocked ? obsidianSync.status : "",
            actions: syncAvailable
                ? [{key: "obsidianSync", text: qsTr("Apri Obsidian Sync")}]
                : browserAvailable ? [{key: "website", service: "obsidian", text: qsTr("Apri sito Obsidian")}] : []
        },
        {
            key: "markdown", title: qsTr("Markdown, vault e Obsidian Publish"),
            state: browserAvailable ? qsTr("Vault locale e collegamenti web") : qsTr("Importazione web e collegamenti Publish"),
            description: browserAvailable
                ? qsTr("Salva una nuova nota nel vault locale, aprila in Obsidian e configura il sito Publish. Puoi anche importare un file Markdown da un collegamento HTTPS diretto.")
                : qsTr("Importa file Markdown pubblici da un collegamento HTTPS diretto e configura i collegamenti al tuo sito Publish. La pubblicazione si gestisce in Obsidian."),
            detail: "", actions: [{key: "markdown", text: qsTr("Apri Markdown e Obsidian")}]
        },
        {
            key: "apple", title: qsTr("Apple e iCloud"),
            state: browserAvailable ? qsTr("Integrazioni con le app del Mac") : qsTr("Gestito sul Mac"),
            description: browserAvailable
                ? qsTr("Prepara file per Note, Libri, Numbers, Keynote e Diario, oppure importa PDF ed EPUB senza DRM da Libri. Completa l’importazione nell’app scelta; l’account Apple resta gestito da macOS.")
                : qsTr("Trasferisci i quaderni via Wi-Fi a Quaderno sul Mac, poi completa l’importazione nelle app Apple. Il Paper Pro non accede direttamente all’account iCloud."),
            detail: "", actions: browserAvailable
                ? [{key: "apple", text: qsTr("Condividi con Apple")}, {key: "appleBooks", text: qsTr("Importa da Libri")},
                   {key: "website", service: "icloud", text: qsTr("Apri iCloud")}]
                : [{key: "wireless", text: qsTr("Collega il Mac")}]
        },
        cloudService("googleDrive", "Google Drive"),
        cloudService("dropbox", "Dropbox"),
        cloudService("oneDrive", "OneDrive"),
        {
            key: "wireless", title: qsTr("Quaderno sul Mac e Paper Pro"),
            state: !wirelessAvailable ? qsTr("Collegamento non disponibile")
                : wireless.transfer?.connected ? qsTr("Connesso · %1").arg(wireless.bluetoothActive ? "Bluetooth" : qsTr("Wi-Fi locale"))
                : wireless.active ? qsTr("Collegamento in corso…")
                : wireless.paired ? qsTr("Abbinato · non connesso") : qsTr("Da abbinare"),
            description: qsTr("Abbina i due dispositivi, scegli i quaderni da trasferire e controlla la sincronizzazione. Per il Wi-Fi devono essere sulla stessa rete locale."),
            detail: wirelessAvailable && !pinLocked ? wireless.status : "",
            actions: [{key: "wireless", text: qsTr("Apri sincronizzazione wireless")}]
        }
    ]

    function cloudService(key: string, title: string): var {
        const account = cloudAccounts ? cloudAccounts.accounts.find(item => item.key === key) : null
        return {
            key, title,
            state: account ? account.status : qsTr("Collegamento cloud non disponibile"),
            description: qsTr("Accedi nella pagina ufficiale, esplora le cartelle e trasferisci PDF, EPUB, Markdown, CSV, immagini e quaderni direttamente via Internet. Ogni caricamento crea una nuova copia."),
            detail: "", actions: cloudAccounts
                ? [{key: "cloud", service: key, text: account && account.connected ? qsTr("Apri file cloud")
                    : key === "googleDrive" ? qsTr("Accedi con Google") : key === "dropbox" ? qsTr("Accedi con Dropbox") : qsTr("Accedi con Microsoft")}]
                : browserAvailable ? [{key: "website", service: key, text: qsTr("Apri sito")}]
                : []
        }
    }

    component Paragraph: Label {
        Layout.fillWidth: true
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        color: "#515159"
        font.pixelSize: 15 * panel.uiScale
    }

    Paragraph {
        text: qsTr("Gestisci le integrazioni disponibili su questo dispositivo e il collegamento con il Mac.")
    }

    Paragraph {
        objectName: "accountsBrowserStatus"
        visible: text.length > 0
        text: panel.browserStatus
        color: panel.browserError ? "#942922" : "#515159"
        Accessible.name: text
    }

    Repeater {
        model: panel.services

        Control {
            id: serviceRow
            required property var modelData
            objectName: "accountRow_" + modelData.key
            Layout.fillWidth: true
            padding: 16 * panel.uiScale
            implicitHeight: implicitContentHeight + topPadding + bottomPadding

            background: Rectangle {
                color: "white"
                border.color: "#d1d1d6"
                border.width: Math.max(1, Math.round(panel.uiScale))
                radius: 15 * panel.uiScale
            }

            contentItem: ColumnLayout {
                spacing: 8 * panel.uiScale

                Paragraph {
                    text: serviceRow.modelData.title
                    color: "#1c1c1e"
                    font.pixelSize: 19 * panel.uiScale
                    font.weight: Font.DemiBold
                }
                Paragraph {
                    objectName: "accountStatus_" + serviceRow.modelData.key
                    text: serviceRow.modelData.state
                    color: "#0059bd"
                }
                Paragraph { text: serviceRow.modelData.description }
                Paragraph {
                    visible: text.length > 0
                    text: serviceRow.modelData.detail
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 8 * panel.uiScale

                    Repeater {
                        model: serviceRow.modelData.actions

                        PaperButton {
                            required property var modelData
                            objectName: "accountAction_" + serviceRow.modelData.key + "_" + modelData.key
                            uiScale: panel.uiScale
                            // A service-state update can recreate this delegate.
                            // Its visual parent is temporarily null during teardown.
                            width: Math.min(implicitWidth, parent ? parent.width : implicitWidth)
                            text: modelData.text
                            enabled: !panel.pinLocked && (modelData.key !== "wireless" || panel.wirelessAvailable)
                            Accessible.name: serviceRow.modelData.title + ": " + text
                            onClicked: panel.dispatchAction(modelData.key, modelData.service || "")
                        }
                    }
                }
            }
        }
    }
}
