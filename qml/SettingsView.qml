import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Control {
    id: settings

    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    required property WifiSettings wifi
    required property PinLock pinLock
    required property DeviceInfo deviceInfo
    required property UsbSsh usbSsh
    required property WirelessSync wireless
    required property EpaperRefresh epaperRefresh
    property var bluetooth: null

    signal openBluetooth()

    property int category: 0
    property string sshActionStatus: ""
    readonly property bool dialogOpen: wifiDialog.visible || pinDialog.visible
                                       || bluetoothPairing.visible || bluetoothForget.visible
    readonly property int bluetoothCategory: 4
    readonly property bool hasBluetoothFinder: tabletMode && Boolean(bluetooth?.available)
    readonly property bool bluetoothPageActive: hasBluetoothFinder && visible && !pinLock.locked
                                                && category === bluetoothCategory
    readonly property color ink: "#1c1c1e"
    readonly property color muted: "#515159"
    readonly property color accent: "#0059bd"
    readonly property color line: "#d1d1d6"
    readonly property color canvas: "#f2f2f7"
    readonly property color grouped: "#e5e5ea"
    readonly property color tinted: "#e7f0fb"
    readonly property color warning: "#942922"
    readonly property real hairline: Math.max(1, Math.round(uiScale))
    readonly property real inset: Math.max(16 * uiScale, (width - 740 * uiScale) / 2)
    padding: 0
    background: Rectangle {
        visible: !settings.tabletMode
        color: settings.canvas
    }

    function open() {
        wifi.refresh()
        deviceInfo.refresh()
        wireless.refreshBluetooth()
        if (bluetoothPageActive)
            bluetooth.refresh()
        sshActionStatus = ""
    }

    function closeDialogs() {
        wifiDialog.close()
        pinDialog.close()
        bluetoothForget.close()
        if (bluetoothPairing.visible)
            bluetooth.cancelPairing()
        bluetoothPairing.close()
    }

    function updateBluetoothPrompt() {
        if (bluetoothPageActive && bluetooth.promptType.length > 0) {
            if (!bluetoothPairing.visible)
                bluetoothPairing.open()
        } else {
            bluetoothPairing.close()
        }
    }

    onBluetoothPageActiveChanged: {
        if (bluetoothPageActive) {
            bluetooth.refresh()
        } else if (hasBluetoothFinder) {
            bluetooth.stopScan()
            bluetooth.cancelPairing()
            bluetoothForget.close()
        }
        updateBluetoothPrompt()
    }

    Connections {
        target: settings.bluetooth
        function onChanged() { settings.updateBluetoothPrompt() }
    }

    component Action: Button {
        id: action

        property bool filled: false
        property bool quiet: false
        property bool segmented: false

        leftPadding: (segmented ? 4 : 13) * settings.uiScale
        hoverEnabled: !settings.tabletMode
        rightPadding: leftPadding
        topPadding: 8 * settings.uiScale
        bottomPadding: topPadding
        implicitHeight: Math.max(44 * settings.uiScale, implicitContentHeight + topPadding + bottomPadding)
        font.pixelSize: 14 * settings.uiScale
        font.weight: Font.DemiBold

        contentItem: Label {
            text: action.text
            textFormat: Text.PlainText
            font: action.font
            color: !action.enabled ? settings.muted
                 : action.filled && !action.segmented ? "white"
                 : action.segmented ? settings.ink : settings.accent
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: !action.enabled ? settings.canvas
                 : action.segmented ? (action.filled ? "white" : action.down ? settings.line : "transparent")
                 : action.filled ? (action.down ? "#004d9e" : settings.accent)
                 : action.down ? settings.tinted : action.quiet ? "transparent" : settings.canvas
            border.color: action.visualFocus ? settings.accent : settings.line
            border.width: action.visualFocus ? 2 * settings.hairline
                        : action.segmented && action.filled ? settings.hairline : 0
            radius: 9 * settings.uiScale
        }
    }

    component Paragraph: Label {
        Layout.fillWidth: true
        color: settings.muted
        textFormat: Text.PlainText
        font.pixelSize: 15 * settings.uiScale
        wrapMode: Text.Wrap
    }

    component Heading: ColumnLayout {
        property alias text: title.text

        Layout.fillWidth: true
        spacing: 10 * settings.uiScale

        Paragraph {
            id: title
            color: settings.ink
            font.pixelSize: 20 * settings.uiScale
            font.weight: Font.DemiBold
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: settings.hairline
            color: settings.line
        }
    }

    component Card: Control {
        default property alias rows: body.data

        Layout.fillWidth: true
        padding: 16 * settings.uiScale
        implicitHeight: implicitContentHeight + topPadding + bottomPadding

        background: Rectangle {
            color: "white"
            border.color: settings.line
            border.width: settings.hairline
            radius: 15 * settings.uiScale
        }

        contentItem: ColumnLayout {
            id: body
            spacing: 12 * settings.uiScale
        }
    }

    component Field: TextField {
        id: field

        Layout.fillWidth: true
        Layout.preferredHeight: 48 * settings.uiScale
        leftPadding: 12 * settings.uiScale
        rightPadding: leftPadding
        color: settings.ink
        placeholderTextColor: settings.muted
        selectionColor: settings.accent
        selectedTextColor: "white"
        font.pixelSize: 16 * settings.uiScale

        background: Rectangle {
            color: settings.canvas
            border.color: field.activeFocus ? settings.accent : settings.line
            border.width: field.activeFocus ? 2 * settings.hairline : settings.hairline
            radius: 9 * settings.uiScale
        }
    }

    component SecretField: Field {
        echoMode: TextInput.Password
        passwordMaskDelay: 0
        inputMethodHints: Qt.ImhHiddenText | Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                          | Qt.ImhNoAutoUppercase
    }

    component PinField: SecretField {
        maximumLength: 8
        validator: RegularExpressionValidator { regularExpression: /[0-9]{0,8}/ }
        inputMethodHints: Qt.ImhDigitsOnly | Qt.ImhHiddenText | Qt.ImhSensitiveData
    }

    component SheetDialog: Dialog {
        id: sheet

        modal: true
        dim: !settings.tabletMode
        anchors.centerIn: Overlay.overlay
        padding: 20 * settings.uiScale

        background: Rectangle {
            color: "white"
            border.color: settings.line
            border.width: settings.hairline
            radius: 16 * settings.uiScale
        }

        header: Label {
            text: sheet.title
            color: settings.ink
            font.pixelSize: 22 * settings.uiScale
            font.weight: Font.DemiBold
            leftPadding: sheet.leftPadding
            rightPadding: sheet.rightPadding
            topPadding: sheet.topPadding
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        Control {
            Layout.fillWidth: true
            Layout.leftMargin: settings.inset
            Layout.rightMargin: Layout.leftMargin
            Layout.topMargin: 16 * settings.uiScale
            Layout.bottomMargin: 12 * settings.uiScale
            padding: 4 * settings.uiScale

            background: Rectangle {
                color: settings.grouped
                border.color: settings.line
                border.width: settings.hairline
                radius: 13 * settings.uiScale
            }

            contentItem: RowLayout {
                spacing: 3 * settings.uiScale

                ButtonGroup { id: categories }

                Repeater {
                    model: {
                        const entries = [qsTr("Rete"), qsTr("USB‑C"), qsTr("Sicurezza"), qsTr("Dispositivo")]
                        if (settings.hasBluetoothFinder)
                            entries.push(qsTr("Bluetooth"))
                        return entries
                    }

                    Action {
                        required property string modelData
                        required property int index
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.minimumWidth: 0
                        text: modelData
                        checkable: true
                        checked: settings.category === index
                        ButtonGroup.group: categories
                        segmented: true
                        filled: checked

                        onClicked: {
                            settings.category = index
                            scroll.contentItem.contentY = 0
                        }
                    }
                }
            }
        }

        Card {
            Layout.fillWidth: true
            Layout.leftMargin: settings.inset
            Layout.rightMargin: Layout.leftMargin
            Layout.bottomMargin: 12 * settings.uiScale
            topPadding: 7 * settings.uiScale
            bottomPadding: topPadding

            RowLayout {
                Layout.fillWidth: true
                spacing: 5 * settings.uiScale

                Paragraph {
                    color: settings.ink
                    font.weight: Font.DemiBold
                    text: qsTr("Lingua dell’app")
                }
                Action {
                    text: "Italiano"
                    filled: AppLanguage.current === "it"
                    onClicked: {
                        AppLanguage.select("it")
                        settings.open()
                    }
                }
                Action {
                    text: "English"
                    filled: AppLanguage.current === "en"
                    onClicked: {
                        AppLanguage.select("en")
                        settings.open()
                    }
                }
            }
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 0

                ColumnLayout {
                    visible: settings.category === 0
                    Layout.fillWidth: true
                    Layout.leftMargin: settings.inset
                    Layout.rightMargin: Layout.leftMargin
                    Layout.bottomMargin: 16 * settings.uiScale
                    spacing: 12 * settings.uiScale

                    Card {
                        Heading { text: qsTr("Wi‑Fi") }
                        Paragraph {
                            text: settings.wifi.connected && !settings.wifi.busy && settings.wifi.ssid.length > 0
                                  ? qsTr("Connesso a %1").arg(settings.wifi.ssid) : settings.wifi.status
                        }
                        Paragraph {
                            visible: settings.wifi.error.length > 0 && !wifiDialog.visible
                            text: settings.wifi.error
                            color: settings.warning
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: 6 * settings.uiScale

                            Action {
                                text: qsTr("Cerca reti")
                                filled: true
                                enabled: settings.wifi.available && !settings.wifi.busy
                                onClicked: settings.wifi.scan()
                            }
                            Action {
                                text: qsTr("Disconnetti")
                                visible: settings.wifi.connected
                                enabled: !settings.wifi.busy
                                onClicked: settings.wifi.disconnectNetwork()
                            }
                        }

                        Repeater {
                            model: settings.wifi.networks

                            delegate: ItemDelegate {
                                id: network
                                required property var modelData
                                Layout.fillWidth: true
                                implicitHeight: 54 * settings.uiScale
                                leftPadding: 6 * settings.uiScale
                                rightPadding: leftPadding
                                text: modelData.ssid
                                enabled: settings.wifi.available && !settings.wifi.busy && modelData.supported
                                Accessible.name: qsTr("%1, %2").arg(modelData.ssid).arg(modelData.security)
                                onClicked: {
                                    if (modelData.secured) {
                                        wifiDialog.networkId = modelData.id
                                        wifiDialog.networkName = modelData.ssid
                                        wifiPassword.text = ""
                                        wifiDialog.open()
                                    } else {
                                        settings.wifi.connectNetwork(modelData.id, "")
                                    }
                                }

                                background: Rectangle {
                                    radius: 7 * settings.uiScale
                                    color: network.down ? settings.tinted : "transparent"
                                    border.color: settings.accent
                                    border.width: network.visualFocus ? 2 * settings.hairline : 0

                                    Rectangle {
                                        anchors.top: parent.top
                                        width: parent.width
                                        height: settings.hairline
                                        color: settings.line
                                    }
                                }

                                contentItem: RowLayout {
                                    spacing: 10 * settings.uiScale
                                    Paragraph {
                                        text: network.modelData.ssid
                                        color: network.enabled ? settings.ink : settings.muted
                                        font.weight: Font.DemiBold
                                        wrapMode: Text.NoWrap
                                        elide: Text.ElideRight
                                    }
                                    Paragraph {
                                        Layout.fillWidth: false
                                        Layout.maximumWidth: network.width * 0.4
                                        text: network.modelData.security
                                        font.pixelSize: 13 * settings.uiScale
                                    }
                                }
                            }
                        }
                    }

                    Card {
                        visible: !settings.hasBluetoothFinder
                        Heading { text: qsTr("Bluetooth") }
                        Paragraph {
                            text: settings.wireless.bluetoothStatus || settings.deviceInfo.bluetoothStatus
                        }
                        Action {
                            visible: settings.wireless.available
                            text: qsTr("Apri sincronizzazione Bluetooth")
                            onClicked: settings.openBluetooth()
                        }
                    }
                }

                ColumnLayout {
                    visible: settings.hasBluetoothFinder && settings.category === settings.bluetoothCategory
                    Layout.fillWidth: true
                    Layout.leftMargin: settings.inset
                    Layout.rightMargin: Layout.leftMargin
                    Layout.bottomMargin: 16 * settings.uiScale
                    spacing: 12 * settings.uiScale

                    Card {
                        Heading { text: qsTr("Dispositivi Bluetooth") }

                        Switch {
                            objectName: "bluetoothPower"
                            Layout.fillWidth: true
                            Layout.minimumHeight: 44 * settings.uiScale
                            font.pixelSize: 16 * settings.uiScale
                            text: qsTr("Bluetooth")
                            enabled: settings.hasBluetoothFinder && !settings.bluetooth.busy
                                     && (settings.bluetooth.powered || settings.bluetooth.canPowerOn)
                            checked: Boolean(settings.bluetooth?.powered)
                            onToggled: settings.bluetooth.setPowered(checked)
                        }

                        Paragraph {
                            visible: text.length > 0
                            text: settings.bluetooth?.status || ""
                        }

                        Paragraph {
                            visible: Boolean(settings.bluetooth?.powered)
                            text: qsTr("Metti la tastiera o l’accessorio in modalità abbinamento, poi avvia la ricerca.")
                        }

                        Action {
                            readonly property bool scanning: Boolean(settings.bluetooth?.scanning)
                            text: scanning ? qsTr("Interrompi ricerca") : qsTr("Cerca dispositivi")
                            visible: Boolean(settings.bluetooth?.powered)
                            enabled: visible && (scanning || (settings.bluetooth.ready && !settings.bluetooth.busy))
                            filled: !scanning
                            onClicked: {
                                if (scanning)
                                    settings.bluetooth.stopScan()
                                else
                                    settings.bluetooth.startScan()
                            }
                        }

                        Repeater {
                            model: settings.bluetooth?.powered ? settings.bluetooth.devices : []

                            ColumnLayout {
                                id: deviceRow
                                required property var modelData
                                readonly property string deviceName: modelData.name || modelData.address
                                readonly property string stateLabel: modelData.connected ? qsTr("Connesso")
                                                                     : modelData.paired ? qsTr("Abbinato") : qsTr("Disponibile")

                                Layout.fillWidth: true
                                spacing: 7 * settings.uiScale

                                Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: settings.hairline
                                    color: settings.line
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8 * settings.uiScale

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        spacing: 2 * settings.uiScale

                                        Paragraph {
                                            text: deviceRow.deviceName
                                            color: settings.ink
                                            font.weight: Font.DemiBold
                                        }
                                        Paragraph {
                                            font.pixelSize: 13 * settings.uiScale
                                            text: {
                                                const labels = {
                                                    keyboard: qsTr("Tastiera"), mouse: qsTr("Mouse"),
                                                    headset: qsTr("Cuffie"), computer: qsTr("Computer"),
                                                    phone: qsTr("Telefono")
                                                }
                                                const label = labels[deviceRow.modelData.kind]
                                                return label ? qsTr("%1 · %2").arg(label).arg(deviceRow.stateLabel)
                                                             : deviceRow.stateLabel
                                            }
                                        }
                                    }

                                    Action {
                                        text: deviceRow.modelData.connected ? qsTr("Disconnetti")
                                              : deviceRow.modelData.paired ? qsTr("Connetti") : qsTr("Abbina")
                                        Accessible.name: qsTr("%1, %2").arg(text).arg(deviceRow.deviceName)
                                        enabled: !settings.bluetooth.busy
                                        onClicked: {
                                            const device = deviceRow.modelData
                                            if (device.connected)
                                                settings.bluetooth.disconnectDevice(device.id)
                                            else if (device.paired)
                                                settings.bluetooth.connectDevice(device.id)
                                            else
                                                settings.bluetooth.pair(device.id)
                                        }
                                    }
                                    Action {
                                        visible: deviceRow.modelData.paired
                                        text: qsTr("Dissocia")
                                        Accessible.name: qsTr("%1, %2").arg(text).arg(deviceRow.deviceName)
                                        enabled: !settings.bluetooth.busy
                                        quiet: true
                                        onClicked: {
                                            bluetoothForget.deviceId = deviceRow.modelData.id
                                            bluetoothForget.deviceName = deviceRow.deviceName
                                            bluetoothForget.open()
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Card {
                        visible: settings.wireless.available
                        Heading { text: qsTr("Sincronizzazione Quaderno") }
                        Paragraph {
                            visible: text.length > 0
                            text: settings.wireless.bluetoothStatus
                        }
                        Action {
                            text: qsTr("Apri sincronizzazione Bluetooth")
                            enabled: !settings.bluetooth?.busy
                            onClicked: {
                                settings.bluetooth.stopScan()
                                settings.openBluetooth()
                            }
                        }
                    }
                }

                Card {
                    visible: settings.category === 1
                    Layout.fillWidth: true
                    Layout.leftMargin: settings.inset
                    Layout.rightMargin: Layout.leftMargin
                    Layout.bottomMargin: 16 * settings.uiScale

                    Heading { text: qsTr("SSH via USB‑C") }
                    Paragraph { text: settings.usbSsh.linkStatus }
                    Paragraph { text: settings.usbSsh.status }
                    Field {
                        Accessible.name: qsTr("Comando SSH")
                        readOnly: true
                        selectByMouse: true
                        font.family: "monospace"
                        text: settings.usbSsh.command
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6 * settings.uiScale

                        Action {
                            text: qsTr("Verifica")
                            filled: true
                            enabled: !settings.usbSsh.busy
                            onClicked: {
                                settings.sshActionStatus = ""
                                settings.usbSsh.refresh()
                            }
                        }
                        Action {
                            visible: !settings.tabletMode
                            text: qsTr("Copia comando")
                            onClicked: {
                                settings.usbSsh.copyCommand()
                                settings.sshActionStatus = qsTr("Comando copiato.")
                            }
                        }
                        Action {
                            text: qsTr("Apri in Terminale")
                            visible: !settings.tabletMode && settings.usbSsh.canOpenTerminal
                            onClicked: {
                                settings.sshActionStatus = settings.usbSsh.openTerminal() ? ""
                                    : qsTr("Impossibile aprire il Terminale. Usa il comando copiato.")
                            }
                        }
                    }
                    Paragraph {
                        visible: settings.sshActionStatus.length > 0
                        text: settings.sshActionStatus
                    }
                    Paragraph {
                        text: qsTr("Collega il tablet in modalità sviluppatore con un cavo dati. Accedi dal computer con le credenziali SSH del tablet.")
                    }
                }

                ColumnLayout {
                    visible: settings.category === 2
                    Layout.fillWidth: true
                    Layout.leftMargin: settings.inset
                    Layout.rightMargin: Layout.leftMargin
                    Layout.bottomMargin: 16 * settings.uiScale
                    spacing: 12 * settings.uiScale

                    Card {
                        Heading { text: qsTr("PIN di Quaderno") }
                        Paragraph {
                            text: settings.pinLock.configured ? qsTr("PIN attivo.") : qsTr("PIN disattivato.")
                        }
                        Paragraph {
                            text: qsTr("Il PIN blocca l’apertura di Quaderno. reMarkable OS gestisce lo sblocco del tablet e l’accesso ai file sul sistema.")
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6 * settings.uiScale

                            Action {
                                text: settings.pinLock.configured ? qsTr("Cambia PIN") : qsTr("Imposta PIN")
                                filled: !settings.pinLock.configured
                                enabled: settings.pinLock.available
                                onClicked: pinDialog.begin(settings.pinLock.configured ? "change" : "set")
                            }
                            Action {
                                visible: settings.pinLock.configured
                                text: qsTr("Rimuovi PIN")
                                enabled: settings.pinLock.available
                                onClicked: pinDialog.begin("remove")
                            }
                            Action {
                                visible: settings.pinLock.configured
                                text: qsTr("Blocca ora")
                                filled: true
                                onClicked: settings.pinLock.lock()
                            }
                        }
                        Paragraph {
                            visible: settings.pinLock.error.length > 0 && !pinDialog.visible
                            text: settings.pinLock.error
                            color: settings.warning
                        }
                    }

                    Card {
                        visible: settings.pinLock.configured
                        Heading { text: qsTr("Blocco automatico") }
                        Paragraph {
                            text: qsTr("Dopo %1 min di inattività.").arg(settings.pinLock.autoLockMinutes)
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6 * settings.uiScale

                            Repeater {
                                model: [1, 5, 15]

                                Action {
                                    required property int modelData
                                    text: qsTr("%1 min").arg(modelData)
                                    enabled: settings.pinLock.available
                                    filled: settings.pinLock.autoLockMinutes === modelData
                                    onClicked: settings.pinLock.setAutoLockMinutes(modelData)
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    visible: settings.category === 3
                    Layout.fillWidth: true
                    Layout.leftMargin: settings.inset
                    Layout.rightMargin: Layout.leftMargin
                    Layout.bottomMargin: 16 * settings.uiScale
                    spacing: 12 * settings.uiScale

                    Card {
                        Heading { text: qsTr("Informazioni") }
                        Paragraph {
                            text: settings.deviceInfo.model
                            color: settings.ink
                            font.weight: Font.DemiBold
                        }
                        Paragraph {
                            visible: settings.deviceInfo.osVersion.length > 0
                            text: qsTr("Software %1").arg(settings.deviceInfo.osVersion)
                        }
                        Action {
                            text: qsTr("Aggiorna informazioni")
                            onClicked: settings.deviceInfo.refresh()
                        }
                    }

                    Card {
                        visible: settings.tabletMode
                        Heading { text: qsTr("Tastiera a schermo") }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6 * settings.uiScale

                            Action {
                                text: "Italiano"
                                filled: AppLanguage.keyboardLayout === "it"
                                onClicked: AppLanguage.selectKeyboardLayout("it")
                            }
                            Action {
                                text: "English (US)"
                                filled: AppLanguage.keyboardLayout === "en"
                                onClicked: AppLanguage.selectKeyboardLayout("en")
                            }
                        }
                    }

                    Card {
                        visible: settings.epaperRefresh.available
                        Heading { text: qsTr("Schermo") }
                        Switch {
                            Layout.fillWidth: true
                            Layout.minimumHeight: 44 * settings.uiScale
                            font.pixelSize: 16 * settings.uiScale
                            text: qsTr("Aggiornamento automatico")
                            checked: settings.epaperRefresh.automaticEnabled
                            onToggled: settings.epaperRefresh.automaticEnabled = checked
                        }
                        Paragraph {
                            text: qsTr("Interviene se la nuova pagina tarda a comparire. Per pulire tutto il pannello, usa Aggiorna schermo.")
                        }
                    }

                    Card {
                        Heading { text: qsTr("Batteria") }
                        Paragraph {
                            text: settings.deviceInfo.batteryPercent >= 0
                                  ? qsTr("%1% · %2").arg(settings.deviceInfo.batteryPercent).arg(settings.deviceInfo.batteryStatus)
                                  : settings.deviceInfo.batteryStatus
                        }
                    }

                    Card {
                        Heading { text: qsTr("Luce di lettura") }
                        Paragraph { text: settings.deviceInfo.frontlightStatus }
                        RowLayout {
                            visible: settings.deviceInfo.frontlightAvailable
                            spacing: 6 * settings.uiScale

                            Action {
                                text: "−"
                                Accessible.name: qsTr("Riduci la luce")
                                Layout.minimumWidth: 46 * settings.uiScale
                                enabled: settings.deviceInfo.frontlightLevel > 0
                                onClicked: settings.deviceInfo.setFrontlightLevel(settings.deviceInfo.frontlightLevel - 1)
                            }
                            Paragraph {
                                Layout.fillWidth: false
                                Layout.minimumWidth: 72 * settings.uiScale
                                horizontalAlignment: Text.AlignHCenter
                                text: settings.deviceInfo.frontlightLevel >= 0
                                      ? qsTr("%1 / 5").arg(settings.deviceInfo.frontlightLevel)
                                      : qsTr("Personalizzata")
                            }
                            Action {
                                text: "+"
                                Accessible.name: qsTr("Aumenta la luce")
                                Layout.minimumWidth: 46 * settings.uiScale
                                enabled: settings.deviceInfo.frontlightLevel < 5
                                onClicked: settings.deviceInfo.setFrontlightLevel(settings.deviceInfo.frontlightLevel + 1)
                            }
                        }
                        Paragraph {
                            visible: settings.deviceInfo.frontlightAvailable
                            text: qsTr("La regolazione vale per la sessione corrente.")
                        }
                        Paragraph {
                            visible: settings.deviceInfo.error.length > 0
                            color: settings.warning
                            text: settings.deviceInfo.error
                        }
                    }
                }
            }
        }
    }

    SheetDialog {
        id: bluetoothPairing

        readonly property string promptType: settings.bluetooth?.promptType || ""
        readonly property string promptCode: settings.bluetooth?.promptCode || ""
        readonly property bool needsInput: promptType === "pin" || promptType === "passkey"
        readonly property bool canSubmit: promptType === "confirm"
                                          || (needsInput && pairingInput.acceptableInput && pairingInput.text.length > 0)

        width: Math.min(440 * settings.uiScale, settings.width - 32 * settings.uiScale)
        title: qsTr("Abbinamento Bluetooth")
        closePolicy: Popup.NoAutoClose
        onPromptTypeChanged: {
            pairingInput.text = ""
            if (visible && needsInput)
                pairingInput.forceActiveFocus()
        }
        onOpened: if (needsInput) pairingInput.forceActiveFocus()
        onClosed: pairingInput.text = ""

        function submit() {
            if (canSubmit)
                settings.bluetooth.answerPrompt(needsInput ? pairingInput.text : "")
        }

        contentItem: ColumnLayout {
            spacing: 8 * settings.uiScale

            Paragraph {
                text: settings.bluetooth?.promptName || ""
                color: settings.ink
                font.weight: Font.DemiBold
            }

            Paragraph {
                text: {
                    switch (bluetoothPairing.promptType) {
                    case "display": return qsTr("Digita il codice sul dispositivo. Se usi una tastiera, premi Invio.")
                    case "confirm": return bluetoothPairing.promptCode.length > 0
                                           ? qsTr("Conferma se il codice corrisponde a quello mostrato sul dispositivo.")
                                           : qsTr("Vuoi abbinare questo dispositivo?")
                    case "pin": return qsTr("Inserisci il PIN richiesto dal dispositivo.")
                    case "passkey": return qsTr("Inserisci il codice numerico mostrato sul dispositivo.")
                    default: return ""
                    }
                }
            }

            Paragraph {
                visible: text.length > 0
                text: bluetoothPairing.promptCode
                color: settings.ink
                font.family: "monospace"
                font.pixelSize: 28 * settings.uiScale
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Field {
                id: pairingInput
                visible: bluetoothPairing.needsInput
                Accessible.name: placeholderText
                placeholderText: bluetoothPairing.promptType === "pin" ? qsTr("PIN del dispositivo")
                                                                       : qsTr("Codice Bluetooth")
                maximumLength: bluetoothPairing.promptType === "pin" ? 16 : 6
                inputMethodHints: bluetoothPairing.promptType === "pin"
                                  ? Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase : Qt.ImhDigitsOnly
                validator: bluetoothPairing.promptType === "passkey" ? pairingPasskeyValidator : pairingPinValidator
                onAccepted: bluetoothPairing.submit()
                RegularExpressionValidator {
                    id: pairingPasskeyValidator
                    regularExpression: /[0-9]{1,6}/
                }
                RegularExpressionValidator {
                    id: pairingPinValidator
                    regularExpression: /[!-~]{1,16}/
                }
            }

            PinKeypad {
                visible: settings.showTouchKeyboard && bluetoothPairing.promptType === "passkey"
                Layout.fillWidth: true
                uiScale: settings.uiScale
                onKeyPressed: key => {
                    const start = pairingInput.selectionStart
                    const end = pairingInput.selectionEnd
                    if (start !== end)
                        pairingInput.remove(start, end)
                    else if (key === "⌫" && start > 0)
                        pairingInput.remove(start - 1, start)
                    if (key !== "⌫")
                        pairingInput.insert(pairingInput.cursorPosition, key)
                }
            }

            TouchKeyboard {
                visible: settings.showTouchKeyboard && bluetoothPairing.promptType === "pin"
                Layout.fillWidth: true
                uiScale: settings.uiScale
                target: pairingInput
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Action {
                    text: qsTr("Annulla")
                    onClicked: settings.bluetooth.cancelPairing()
                }
                Action {
                    visible: bluetoothPairing.promptType !== "display"
                    enabled: bluetoothPairing.canSubmit
                    filled: true
                    text: qsTr("Conferma")
                    onClicked: bluetoothPairing.submit()
                }
            }
        }
    }

    SheetDialog {
        id: bluetoothForget
        property string deviceId: ""
        property string deviceName: ""

        width: Math.min(440 * settings.uiScale, settings.width - 32 * settings.uiScale)
        title: qsTr("Dissocia dispositivo")
        onClosed: {
            deviceId = ""
            deviceName = ""
        }

        contentItem: ColumnLayout {
            spacing: 8 * settings.uiScale

            Paragraph { text: qsTr("Vuoi dissociare «%1»?").arg(bluetoothForget.deviceName) }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Action { text: qsTr("Annulla"); onClicked: bluetoothForget.close() }
                Action {
                    text: qsTr("Dissocia")
                    filled: true
                    onClicked: {
                        settings.bluetooth.forget(bluetoothForget.deviceId)
                        bluetoothForget.close()
                    }
                }
            }
        }
    }

    SheetDialog {
        id: wifiDialog
        property string networkId: ""
        property string networkName: ""

        width: Math.min(440 * settings.uiScale, settings.width - 32 * settings.uiScale)
        title: qsTr("Connetti al Wi‑Fi")
        onOpened: wifiPassword.forceActiveFocus()
        onClosed: wifiPassword.text = ""

        function connectSelected() {
            if (wifiPassword.text.length < 8 || settings.wifi.busy)
                return
            settings.wifi.connectNetwork(networkId, wifiPassword.text)
            wifiPassword.text = ""
            close()
        }

        contentItem: ColumnLayout {
            spacing: 8 * settings.uiScale

            Paragraph { text: wifiDialog.networkName }
            SecretField {
                id: wifiPassword
                Accessible.name: qsTr("Password Wi‑Fi")
                placeholderText: qsTr("Password")
                maximumLength: 64
                onAccepted: wifiDialog.connectSelected()
            }
            TouchKeyboard {
                visible: settings.showTouchKeyboard
                Layout.fillWidth: true
                uiScale: settings.uiScale
                target: wifiPassword
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Action { text: qsTr("Annulla"); onClicked: wifiDialog.close() }
                Action {
                    text: qsTr("Connetti")
                    enabled: wifiPassword.text.length >= 8 && !settings.wifi.busy
                    filled: true
                    onClicked: wifiDialog.connectSelected()
                }
            }
        }
    }

    SheetDialog {
        id: pinDialog
        property string operation: "set"
        property string validationError: ""
        property TextInput activeField: newPin
        readonly property bool usesCurrent: operation !== "set"
        readonly property bool usesNew: operation !== "remove"

        width: Math.min(360 * settings.uiScale, settings.width - 32 * settings.uiScale)
        title: operation === "set" ? qsTr("Imposta PIN")
             : operation === "change" ? qsTr("Cambia PIN") : qsTr("Rimuovi PIN")
        onOpened: (usesCurrent ? currentPin : newPin).forceActiveFocus()
        onClosed: clearFields()

        function clearFields() {
            currentPin.text = ""
            newPin.text = ""
            confirmPin.text = ""
            validationError = ""
        }

        function begin(action: string) {
            operation = action
            clearFields()
            activeField = usesCurrent ? currentPin : newPin
            open()
        }

        function typeDigit(key: string) {
            const first = activeField.selectionStart
            const last = activeField.selectionEnd
            if (first !== last) {
                activeField.remove(first, last)
                activeField.cursorPosition = first
            } else if (key === "⌫" && first > 0) {
                activeField.remove(first - 1, first)
                return
            }
            if (key !== "⌫")
                activeField.insert(activeField.cursorPosition, key)
        }

        function submit() {
            if ((usesCurrent && currentPin.text.length < 4) || (usesNew && newPin.text.length < 4)) {
                validationError = qsTr("Il PIN deve contenere da 4 a 8 cifre.")
                return
            }
            if (usesNew && newPin.text !== confirmPin.text) {
                validationError = qsTr("I due PIN inseriti non coincidono.")
                return
            }
            validationError = ""
            const ok = operation === "set" ? settings.pinLock.setPin(newPin.text)
                     : operation === "change" ? settings.pinLock.changePin(currentPin.text, newPin.text)
                     : settings.pinLock.removePin(currentPin.text)
            if (ok) {
                close()
            } else {
                currentPin.text = ""
                if (usesCurrent && !settings.pinLock.locked)
                    currentPin.forceActiveFocus()
            }
        }

        contentItem: ColumnLayout {
            spacing: 7 * settings.uiScale

            PinField {
                id: currentPin
                visible: pinDialog.usesCurrent
                placeholderText: qsTr("PIN attuale")
                Accessible.name: placeholderText
                onActiveFocusChanged: if (activeFocus) pinDialog.activeField = currentPin
                onAccepted: if (pinDialog.usesNew) newPin.forceActiveFocus(); else pinDialog.submit()
            }
            PinField {
                id: newPin
                visible: pinDialog.usesNew
                placeholderText: qsTr("Nuovo PIN (4–8 cifre)")
                Accessible.name: placeholderText
                onActiveFocusChanged: if (activeFocus) pinDialog.activeField = newPin
                onAccepted: confirmPin.forceActiveFocus()
            }
            PinField {
                id: confirmPin
                visible: pinDialog.usesNew
                placeholderText: qsTr("Conferma PIN")
                Accessible.name: placeholderText
                onActiveFocusChanged: if (activeFocus) pinDialog.activeField = confirmPin
                onAccepted: pinDialog.submit()
            }
            PinKeypad {
                visible: settings.showTouchKeyboard
                Layout.fillWidth: true
                uiScale: settings.uiScale
                onKeyPressed: key => pinDialog.typeDigit(key)
            }
            Paragraph {
                visible: text.length > 0
                color: settings.warning
                text: pinDialog.validationError || settings.pinLock.error
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Action { text: qsTr("Annulla"); onClicked: pinDialog.close() }
                Action {
                    text: pinDialog.usesNew ? qsTr("Salva") : qsTr("Rimuovi")
                    filled: true
                    onClicked: pinDialog.submit()
                }
            }
        }
    }
}
