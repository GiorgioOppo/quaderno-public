pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: automation
    objectName: "syncAutomation"
    visible: false
    required property var preferences
    required property var wireless
    required property var wifi
    required property var deviceInfo
    required property var notebook
    property bool allowed: true
    property bool pending: false
    property bool initialized: false
    readonly property var transfer: wireless.transfer
    readonly property bool enabledByPreference: preferences.wifiAutoSync !== "manual"
    readonly property bool connected: transfer.connected
    readonly property bool continuous: preferences.wifiAutoSync === "onChange"
    readonly property string currentSsid: wifi && wifi.available && wifi.connected ? wifi.ssid : ""
    readonly property bool trustedNetwork: preferences.trustedWifiSsids.length === 0
        || (currentSsid.length > 0 && preferences.trustedWifiSsids.includes(currentSsid))
    readonly property bool chargingAllowed: !wireless.tablet || !preferences.wifiChargingOnly || deviceInfo.charging
    readonly property bool eligible: enabledByPreference && allowed && wireless.paired
        && transfer.pinnedBooks.length > 0 && chargingAllowed
        && (preferences.syncTransport !== "wifi" || trustedNetwork)
    // Actual transport is deliberately excluded from eligible: its notifications
    // must never feed the synchronous automaticAllowed policy binding.
    readonly property bool transportAllowed: wireless.bluetoothActive
        ? preferences.syncTransport !== "wifi"
        : preferences.syncTransport !== "bluetooth" && trustedNetwork
    readonly property string status: !enabledByPreference ? qsTr("Sincronizzazione automatica disattivata.")
        : !wireless.paired ? qsTr("Abbina un dispositivo per sincronizzare automaticamente.")
        : transfer.pinnedBooks.length === 0 ? qsTr("Scegli i quaderni da sincronizzare nella finestra wireless.")
        : !allowed ? qsTr("Sincronizzazione automatica in pausa durante il blocco o la modifica.")
        : preferences.syncTransport === "wifi" && !trustedNetwork ? (currentSsid.length === 0
            ? qsTr("In pausa: il nome della rete Wi-Fi non è disponibile.")
            : qsTr("In pausa: la rete Wi-Fi non è nell’elenco delle reti attendibili."))
        : !chargingAllowed ? qsTr("In pausa: attendi la ricarica del tablet.")
        : wireless.connectionPaused ? qsTr("Collegamento sospeso. Premi «Riprendi collegamento» per riattivarlo.")
        : wireless.reconnectPending ? qsTr("Nuovo tentativo di collegamento tra circa %1 secondi.").arg(wireless.reconnectDelaySeconds)
        : !connected ? qsTr("In attesa del dispositivo abbinato. Le modifiche restano salvate qui.")
        : !transportAllowed ? qsTr("Il collegamento attivo non è consentito dalle impostazioni automatiche.")
        : transfer.previewInfo.action ? qsTr("Controlla l’anteprima wireless prima di continuare.")
        : transfer.busy ? qsTr("Sincronizzazione dei quaderni selezionati in corso…")
        : transfer.automaticRetryPending ? qsTr("Trasferimento interrotto: nuovo tentativo automatico in attesa.")
        : pending || transfer.pendingAutomaticCount > 0 ? qsTr("Modifiche in attesa di sincronizzazione.")
        : continuous ? qsTr("Sincronizzazione dopo le modifiche · %1.").arg(wireless.bluetoothActive ? qsTr("Bluetooth") : qsTr("Wi-Fi"))
        : preferences.wifiAutoSync === "interval"
            ? qsTr("Sincronizzazione programmata ogni %1 minuti.").arg(preferences.wifiSyncMinutes)
            : qsTr("Sincronizzazione dei quaderni selezionati alla loro apertura.")

    function bookOpened(bookId: string) {
        if (preferences.wifiAutoSync !== "onOpen" || !bookId || !transfer.pinnedBooks.includes(bookId)) return
        pending = true
        dispatchPending()
    }
    function catchUp() {
        if (!initialized || !enabledByPreference || !wireless.paired || transfer.pinnedBooks.length === 0) return
        pending = true
        Qt.callLater(dispatchPending)
    }
    function refreshChargingGate() {
        if (enabledByPreference && allowed && wireless.tablet && preferences.wifiChargingOnly)
            // DeviceInfo reads tablet charging synchronously. Marker telemetry is
            // asynchronous but does not participate in this gate.
            deviceInfo.refresh()
    }
    function dispatchPending() {
        if (!pending || !enabledByPreference || !allowed) return
        refreshChargingGate()
        if (!eligible || !connected || !transportAllowed
            || transfer.busy || transfer.resumeBusy || transfer.previewInfo.action) return
        // Clear before calling C++: synchronous state notifications cannot submit
        // the same request twice. One pending flag bounds deferred work.
        pending = false
        if (!transfer.requestAutomaticSync()) pending = true
    }
    function syncNow() {
        if (!enabledByPreference || transfer.pinnedBooks.length === 0) return false
        pending = true
        dispatchPending()
        return true
    }
    onEligibleChanged: if (eligible) Qt.callLater(dispatchPending)
    onTransportAllowedChanged: if (transportAllowed) Qt.callLater(dispatchPending)
    onEnabledByPreferenceChanged: {
        if (!enabledByPreference) pending = false
        else catchUp()
    }
    onConnectedChanged: {
        if (connected) catchUp()
        else if (initialized && enabledByPreference) pending = true
    }
    Component.onCompleted: { initialized = true; catchUp() }

    Binding { target: automation.transfer; property: "triggeredAutomaticSync"; value: !automation.continuous }
    Binding { target: automation.wireless; property: "automaticTransport"; value: automation.preferences.syncTransport }
    Binding { target: automation.wireless; property: "automaticWifiAllowed"; value: automation.trustedNetwork }
    Binding { target: automation.wireless; property: "automaticAllowed"; value: automation.eligible }
    Binding { target: automation.transfer; property: "autoSync"; value: automation.enabledByPreference && automation.wireless.paired }
    Binding { target: automation.transfer; property: "conflictPolicy"; value: automation.preferences.syncConflictPolicy }

    Timer {
        interval: Math.max(5, Math.min(60, automation.preferences.wifiSyncMinutes)) * 60 * 1000
        repeat: true
        running: automation.preferences.wifiAutoSync === "interval" && automation.wireless.paired
        onTriggered: automation.syncNow()
    }
    Timer {
        interval: 15000
        repeat: true
        triggeredOnStart: true
        running: automation.enabledByPreference && automation.allowed && automation.wireless.paired
            && automation.wireless.tablet && automation.preferences.wifiChargingOnly
        onTriggered: automation.refreshChargingGate()
    }
    Connections {
        target: automation.transfer
        function onChanged() { if (automation.pending) Qt.callLater(automation.dispatchPending) }
        function onResumeChanged() { if (automation.pending) Qt.callLater(automation.dispatchPending) }
        function onPinnedBooksChanged() {
            if (automation.transfer.pinnedBooks.length === 0) automation.pending = false
            else automation.catchUp()
        }
    }
    Connections {
        target: automation.wireless
        function onChanged() { if (automation.pending) Qt.callLater(automation.dispatchPending) }
        function onPairedChanged() {
            if (!automation.wireless.paired) automation.pending = false
            else automation.catchUp()
        }
    }
    Connections {
        target: automation.preferences
        function onWifiAutoSyncChanged() { automation.catchUp() }
        function onSyncTransportChanged() { automation.catchUp() }
    }
}
