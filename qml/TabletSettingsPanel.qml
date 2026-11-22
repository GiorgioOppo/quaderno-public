pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

ColumnLayout {
    id: panel
    objectName: "tabletSettingsPanel"
    required property real uiScale
    required property bool tabletMode
    required property int category
    required property var preferences
    required property var standby
    required property var deviceInfo
    required property var epaperRefresh
    property var externalKeyboard: null
    property var wifi: null
    property var maintenance: null
    property var screenOrientation: null
    property bool showTouchKeyboard: true
    property var activeField: null
    readonly property color textColor: preferences.highContrast ? "#000000" : "#1c1c1e"
    readonly property color secondaryTextColor: preferences.highContrast ? "#000000" : "#515159"
    readonly property color borderColor: preferences.highContrast ? "#000000" : "#d1d1d6"
    readonly property var allToolbarActions: [
        { value: "pen", label: qsTr("Penna") }, { value: "highlighter", label: qsTr("Evidenziatore") },
        { value: "eraser", label: qsTr("Gomma") }, { value: "lasso", label: qsTr("Lazo") },
        { value: "shapes", label: qsTr("Forme") }, { value: "text", label: qsTr("Testo") },
        { value: "table", label: qsTr("Tabella") }, { value: "chart", label: qsTr("Grafico") }]
    readonly property var orderedToolbarActions: preferences.toolbarActions.concat(
        allToolbarActions.map(action => action.value).filter(action => !preferences.toolbarActions.includes(action)))
    function toggleToolbarAction(action: string) {
        const actions = preferences.toolbarActions.slice()
        const index = actions.indexOf(action)
        if (index < 0) actions.push(action)
        else if (actions.length > 1) actions.splice(index, 1)
        preferences.toolbarActions = actions
    }
    function moveToolbarAction(action: string, offset: int) {
        const actions = preferences.toolbarActions.slice()
        const index = actions.indexOf(action)
        const target = index + offset
        if (index < 0 || target < 0 || target >= actions.length) return
        actions.splice(index, 1)
        actions.splice(target, 0, action)
        preferences.toolbarActions = actions
    }
    function storageSize(value: real): string {
        return qsTr("%1 MiB").arg((Math.max(0, value || 0) / (1024 * 1024)).toFixed(1))
    }
    signal navigate(int category)
    signal refreshScreen()
    signal sleepNow()
    spacing: 12 * uiScale

    readonly property string batteryText: deviceInfo.batteryPercent >= 0
        ? qsTr("%1% · %2").arg(deviceInfo.batteryPercent).arg(deviceInfo.batteryStatus)
        : deviceInfo.batteryStatus
    readonly property string markerText: deviceInfo.markerBatteryPercent >= 0
        ? qsTr("%1% · %2").arg(deviceInfo.markerBatteryPercent).arg(deviceInfo.markerStatus)
        : deviceInfo.markerStatus

    component Body: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        color: panel.secondaryTextColor
        font.bold: panel.preferences.boldLabels
        font.pixelSize: 15 * panel.uiScale
    }
    component Card: Control {
        id: card
        property string title
        default property alias rows: body.data
        Layout.fillWidth: true
        padding: 16 * panel.uiScale
        implicitHeight: implicitContentHeight + topPadding + bottomPadding
        background: Rectangle { color: "white"; radius: 15 * panel.uiScale; border.color: panel.borderColor }
        contentItem: ColumnLayout {
            spacing: 10 * panel.uiScale
            Body { text: card.title; color: panel.textColor; font.pixelSize: 20 * panel.uiScale; font.weight: Font.DemiBold }
            ColumnLayout { id: body; Layout.fillWidth: true; spacing: 10 * panel.uiScale }
        }
    }
    component Action: PaperButton {
        uiScale: panel.uiScale
        hoverEnabled: !panel.tabletMode
        implicitHeight: Math.max(44 * panel.uiScale, implicitContentHeight + topPadding + bottomPadding)
    }
    component Toggle: Switch {
        id: toggle
        Layout.fillWidth: true
        Layout.minimumHeight: 44 * panel.uiScale
        font.pixelSize: 15 * panel.uiScale
        font.bold: panel.preferences.boldLabels
        contentItem: Text {
            text: toggle.text
            font: toggle.font
            color: panel.textColor
            leftPadding: toggle.indicator.width + toggle.spacing
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }

    component Choice: ColumnLayout {
        id: choice
        required property string preferenceKey
        required property string label
        required property var choices
        Layout.fillWidth: true
        spacing: 4 * panel.uiScale
        Body { text: choice.label; color: panel.textColor }
        ComboBox {
            objectName: "preference_" + choice.preferenceKey
            Layout.fillWidth: true
            Layout.minimumHeight: 44 * panel.uiScale
            model: choice.choices
            textRole: "label"
            valueRole: "value"
            currentIndex: choice.choices.findIndex(option => option.value === panel.preferences[choice.preferenceKey])
            font.pixelSize: 15 * panel.uiScale
            font.bold: panel.preferences.boldLabels
            Accessible.name: choice.label
            onActivated: panel.preferences[choice.preferenceKey] = currentValue
        }
    }
    component ListEditor: ColumnLayout {
        id: listEditor
        required property string preferenceKey
        required property string label
        property string hint: ""
        property string validationError: ""
        Layout.fillWidth: true
        spacing: 6 * panel.uiScale
        Body { text: listEditor.label; color: panel.textColor }
        Body { visible: listEditor.hint.length > 0; text: listEditor.hint }
        TextArea {
            id: listField
            objectName: "preference_" + listEditor.preferenceKey
            Layout.fillWidth: true
            Layout.preferredHeight: 110 * panel.uiScale
            font.pixelSize: 15 * panel.uiScale
            font.bold: panel.preferences.boldLabels
            color: panel.textColor
            text: panel.preferences[listEditor.preferenceKey].join("\n")
            selectByMouse: !panel.tabletMode
            wrapMode: TextEdit.Wrap
            Accessible.name: listEditor.label
            background: Rectangle { color: "white"; border.color: panel.borderColor; radius: 6 * panel.uiScale }
            onActiveFocusChanged: if (activeFocus) panel.activeField = listField
        }
        TouchKeyboard {
            visible: panel.showTouchKeyboard && panel.activeField === listField
            Layout.fillWidth: true
            uiScale: panel.uiScale
            target: listField
        }
        Action {
            objectName: "save_" + listEditor.preferenceKey
            text: qsTr("Salva elenco")
            onClicked: {
                const keepSpaces = listEditor.preferenceKey === "trustedWifiSsids"
                const entries = listField.text.split("\n").map(line => keepSpaces ? line : line.trim())
                    .filter(line => line.length > 0)
                const unique = entries.filter((entry, index) => entries.indexOf(entry) === index)
                panel.preferences[listEditor.preferenceKey] = unique
                listEditor.validationError = JSON.stringify(panel.preferences[listEditor.preferenceKey]) === JSON.stringify(unique)
                    ? "" : qsTr("Elenco non salvato: controlla il formato e la lunghezza delle voci.")
                if (listEditor.validationError.length === 0) {
                    listField.text = panel.preferences[listEditor.preferenceKey].join("\n")
                    panel.activeField = null
                    panel.forceActiveFocus()
                }
            }
        }
        Body { visible: listEditor.validationError.length > 0; text: listEditor.validationError; color: "#942922" }
    }

    Card {
        visible: panel.category === 9
        title: qsTr("A colpo d’occhio")
        Body { text: panel.deviceInfo.model; color: panel.textColor; font.weight: Font.DemiBold }
        Body { text: qsTr("Batteria: %1").arg(panel.batteryText) }
        Body { visible: panel.tabletMode; text: qsTr("Penna: %1").arg(panel.markerText) }
        Body { text: !panel.wifi?.available ? qsTr("Wi-Fi gestito dal sistema") : panel.wifi.connected ? qsTr("Wi-Fi: %1").arg(panel.wifi.ssid) : qsTr("Wi-Fi non connesso") }
        Flow {
            Layout.fillWidth: true
            spacing: 6 * panel.uiScale
            Action { text: qsTr("Wi-Fi"); onClicked: panel.navigate(0) }
            Action { text: qsTr("Schermo"); onClicked: panel.navigate(5) }
            Action { text: qsTr("Account e servizi"); onClicked: panel.navigate(6) }
            Action { text: qsTr("Penna e batteria"); onClicked: panel.navigate(7) }
        }
    }
    Card {
        visible: panel.category === 9
        title: qsTr("Controlli rapidi")
        Toggle { objectName: "quickPanelEnabled"; text: qsTr("Mostra accesso ai controlli rapidi"); checked: panel.preferences.quickPanelEnabled; onToggled: panel.preferences.quickPanelEnabled = checked }
        Flow {
            Layout.fillWidth: true
            spacing: 6 * panel.uiScale
            Action {
                objectName: "settingsQuickRefresh"
                text: qsTr("Aggiorna schermo")
                enabled: panel.epaperRefresh.available
                onClicked: panel.refreshScreen()
            }
            Action {
                objectName: "settingsSleepNow"
                text: qsTr("Riposa adesso")
                enabled: panel.tabletMode && !panel.standby.sleeping
                onClicked: panel.sleepNow()
            }
        }
        Body { visible: !panel.tabletMode; text: qsTr("I controlli del display e della sospensione sono disponibili sul Paper Pro.") }
    }

    Card {
        visible: panel.category === 5
        title: qsTr("Orientamento dello schermo")
        Body { text: qsTr("Scegli come tenere il tablet. L’orientamento viene ricordato al prossimo avvio.") }
        Flow {
            Layout.fillWidth: true
            spacing: 6 * panel.uiScale
            Repeater {
                model: [{ angle: 0, label: qsTr("Verticale") }, { angle: 90, label: qsTr("Orizzontale a destra") },
                        { angle: 180, label: qsTr("Verticale capovolto") }, { angle: 270, label: qsTr("Orizzontale a sinistra") }]
                Action {
                    required property var modelData
                    objectName: "orientation_" + modelData.angle
                    text: modelData.label
                    filled: !panel.preferences.automaticOrientation && panel.preferences.orientation === modelData.angle
                    enabled: panel.tabletMode
                    onClicked: {
                        panel.preferences.automaticOrientation = false
                        panel.preferences.orientation = modelData.angle
                    }
                }
            }
        }
        Toggle {
            objectName: "automaticOrientation"
            text: qsTr("Ruota automaticamente con il tablet")
            visible: panel.tabletMode && (Boolean(panel.screenOrientation?.available) || panel.preferences.automaticOrientation)
            enabled: panel.tabletMode && (Boolean(panel.screenOrientation?.available) || checked)
            checked: panel.preferences.automaticOrientation
            onToggled: panel.preferences.automaticOrientation = checked
        }
        Body {
            objectName: "orientationSensorStatus"
            text: !panel.tabletMode
                ? qsTr("Sul Mac usa le dimensioni della finestra per adattare lo spazio di lavoro.")
                : (panel.screenOrientation?.status || qsTr("Sensore di orientamento non disponibile. Usa la rotazione manuale."))
        }
        Body {
            visible: panel.tabletMode && Boolean(panel.screenOrientation?.available)
            text: qsTr("Scegliere un orientamento manuale disattiva la rotazione automatica. La pagina resta invariata.")
        }
    }
    Card {
        visible: panel.category === 5
        title: qsTr("Dimensione dell’interfaccia")
        Flow {
            Layout.fillWidth: true
            spacing: 6 * panel.uiScale
            Repeater {
                model: [{ value: 100, label: qsTr("Standard") }, { value: 115, label: qsTr("Grande") }, { value: 130, label: qsTr("Molto grande") }]
                Action {
                    required property var modelData
                    objectName: "interfaceScale_" + modelData.value
                    text: modelData.label
                    filled: panel.preferences.interfaceScale === modelData.value
                    onClicked: panel.preferences.interfaceScale = modelData.value
                }
            }
        }
        Toggle { objectName: "showStatusBar"; text: qsTr("Mostra barra di stato"); checked: panel.preferences.showStatusBar; onToggled: panel.preferences.showStatusBar = checked }
        Toggle { objectName: "showClock"; text: qsTr("Mostra orologio"); checked: panel.preferences.showClock; enabled: panel.preferences.showStatusBar; onToggled: panel.preferences.showClock = checked }
        Toggle { objectName: "showPenBattery"; text: qsTr("Mostra batteria della penna"); checked: panel.preferences.showPenBattery; enabled: panel.preferences.showStatusBar; onToggled: panel.preferences.showPenBattery = checked }
        Toggle { objectName: "startFullPage"; text: qsTr("Apri i fogli a schermo intero"); checked: panel.preferences.startFullPage; onToggled: panel.preferences.startFullPage = checked }
        Toggle { objectName: "gestureNavigationEnabled"; text: qsTr("Gesture per libri e fogli"); checked: panel.preferences.gestureNavigationEnabled; onToggled: panel.preferences.gestureNavigationEnabled = checked }
    }
    Card {
        visible: panel.category === 5 && panel.epaperRefresh.available
        title: qsTr("Aggiornamento del display")
        Toggle { objectName: "settingsAutomaticRefresh"; text: qsTr("Aggiornamento automatico"); checked: panel.epaperRefresh.automaticEnabled; onToggled: panel.epaperRefresh.automaticEnabled = checked }
        Body { text: qsTr("Interviene se la nuova pagina tarda a comparire. Il pulsante manuale resta sempre disponibile.") }
        Action { objectName: "settingsManualRefresh"; text: qsTr("Aggiorna schermo"); onClicked: panel.refreshScreen() }
    }
    Card {
        visible: panel.category === 5
        title: qsTr("Ripristina visualizzazione")
        Body { text: qsTr("Ripristina schermo, orologio, strumenti, gesture e preferenze di lettura. Scrittura, sincronizzazione, backup, quaderni, account e PIN vengono conservati.") }
        Action { objectName: "resetDisplay"; text: qsTr("Ripristina visualizzazione"); onClicked: panel.preferences.resetDisplay() }
    }

    Card {
        visible: panel.category === 7
        title: qsTr("Batteria del tablet")
        Body { text: panel.batteryText }
    }
    Card {
        visible: panel.category === 7
        title: qsTr("Penna e ricarica")
        Body { objectName: "markerBatteryReadout"; text: panel.markerText; color: panel.textColor; font.weight: Font.DemiBold }
        Body { text: qsTr("Aggancia la penna al lato magnetico per la ricarica. Se il firmware non fornisce una lettura corrente, la percentuale non viene mostrata.") }
        Action { objectName: "refreshMarkerBattery"; text: qsTr("Aggiorna stato penna"); onClicked: panel.deviceInfo.refreshMarker() }
    }
    Card {
        visible: panel.category === 7 && panel.tabletMode
        title: qsTr("Sospensione di Quaderno")
        Body { text: qsTr("Metti in pausa lo schermo dopo %1 minuti di inattività.").arg(panel.preferences.idleMinutes) }
        Flow {
            Layout.fillWidth: true
            spacing: 6 * panel.uiScale
            Repeater {
                model: [2, 5, 10, 20, 30, 60]
                Action {
                    required property int modelData
                    objectName: "idleMinutes_" + modelData
                    text: qsTr("%1 min").arg(modelData)
                    filled: panel.preferences.idleMinutes === modelData
                    onClicked: panel.preferences.idleMinutes = modelData
                }
            }
        }
        Toggle { objectName: "keepAwakeWhileCharging"; text: qsTr("Mantieni attivo durante la ricarica"); checked: panel.preferences.keepAwakeWhileCharging; onToggled: panel.preferences.keepAwakeWhileCharging = checked }
        Body { text: qsTr("Il blocco automatico con PIN mantiene il proprio intervallo, anche durante la ricarica.") }
        Action { text: qsTr("Riposa adesso"); onClicked: panel.sleepNow() }
    }
    Card {
        visible: panel.category === 7 || panel.category === 9
        title: qsTr("Luce di lettura")
        Body { text: panel.deviceInfo.frontlightStatus }
        RowLayout {
            visible: panel.deviceInfo.frontlightAvailable
            spacing: 8 * panel.uiScale
            Action {
                text: "−"
                Accessible.name: qsTr("Riduci la luce")
                enabled: panel.deviceInfo.frontlightLevel > 0
                onClicked: panel.deviceInfo.setFrontlightLevel(panel.deviceInfo.frontlightLevel - 1)
            }
            Body { Layout.fillWidth: false; text: qsTr("%1 / 5").arg(panel.deviceInfo.frontlightLevel) }
            Action {
                text: "+"
                Accessible.name: qsTr("Aumenta la luce")
                enabled: panel.deviceInfo.frontlightLevel >= 0 && panel.deviceInfo.frontlightLevel < 5
                onClicked: panel.deviceInfo.setFrontlightLevel(panel.deviceInfo.frontlightLevel + 1)
            }
        }
        Body { visible: panel.deviceInfo.frontlightAvailable; text: qsTr("La regolazione vale per la sessione corrente.") }
    }

    Card {
        visible: panel.category === 8
        title: qsTr("Lingua dell’app")
        Flow {
            Layout.fillWidth: true
            spacing: 6 * panel.uiScale
            Action { text: "Italiano"; filled: AppLanguage.current === "it"; onClicked: AppLanguage.select("it") }
            Action { text: "English"; filled: AppLanguage.current === "en"; onClicked: AppLanguage.select("en") }
        }
    }
    Card {
        visible: panel.category === 8
        title: qsTr("Tastiera a schermo")
        Flow {
            Layout.fillWidth: true
            spacing: 6 * panel.uiScale
            Action { text: "Italiano QWERTY"; filled: AppLanguage.keyboardLayout === "it"; onClicked: AppLanguage.selectKeyboardLayout("it") }
            Action { text: "English (US) QWERTY"; filled: AppLanguage.keyboardLayout === "en"; onClicked: AppLanguage.selectKeyboardLayout("en") }
        }
        Body { text: panel.externalKeyboard?.connected ? qsTr("Tastiera fisica connessa: la tastiera a schermo resta nascosta.") : qsTr("La tastiera a schermo si nasconde automaticamente quando colleghi una tastiera fisica.") }
        Action { text: qsTr("Gestisci dispositivi Bluetooth"); onClicked: panel.navigate(4) }
    }

    Card {
        visible: panel.category === 5
        title: qsTr("Profili d’uso")
        Body { text: qsTr("Applica insieme strumenti, schermo intero e gesture. Puoi poi personalizzare ogni opzione; i tuoi documenti restano invariati.") }
        Flow {
            Layout.fillWidth: true
            spacing: 6 * panel.uiScale
            Repeater {
                model: [{ value: "reading", label: qsTr("Lettura") }, { value: "writing", label: qsTr("Scrittura") }, { value: "drawing", label: qsTr("Disegno") }]
                Action {
                    required property var modelData
                    objectName: "usageProfile_" + modelData.value
                    text: modelData.label
                    filled: panel.preferences.usageProfile === modelData.value
                    onClicked: panel.preferences.applyUsageProfile(modelData.value)
                }
            }
        }
        Choice { preferenceKey: "handedness"; label: qsTr("Mano preferita"); choices: [{ value: "right", label: qsTr("Destra") }, { value: "left", label: qsTr("Sinistra") }] }
    }
    Card {
        visible: panel.category === 5
        title: qsTr("Orologio e accessibilità")
        Choice {
            preferenceKey: "timeZoneId"; label: qsTr("Fuso orario dell’orologio")
            choices: [{ value: "system", label: qsTr("Usa quello del dispositivo") }, { value: "UTC", label: "UTC" },
                { value: "Europe/Rome", label: qsTr("Roma") }, { value: "Europe/London", label: qsTr("Londra") },
                { value: "Europe/Paris", label: qsTr("Parigi") }, { value: "America/New_York", label: "New York" },
                { value: "America/Chicago", label: "Chicago" }, { value: "America/Los_Angeles", label: "Los Angeles" },
                { value: "Asia/Tokyo", label: "Tokyo" }, { value: "Asia/Shanghai", label: "Shanghai" },
                { value: "Asia/Dubai", label: "Dubai" }, { value: "Australia/Sydney", label: "Sydney" }]
        }
        Body { text: qsTr("Cambia solo l’orologio di Quaderno, senza modificare l’ora del sistema.") }
        Toggle { objectName: "clock24Hour"; text: qsTr("Formato 24 ore"); checked: panel.preferences.clock24Hour; onToggled: panel.preferences.clock24Hour = checked }
        Toggle { objectName: "highContrast"; text: qsTr("Contrasto elevato"); checked: panel.preferences.highContrast; onToggled: panel.preferences.highContrast = checked }
        Toggle { objectName: "boldLabels"; text: qsTr("Etichette in grassetto"); checked: panel.preferences.boldLabels; onToggled: panel.preferences.boldLabels = checked }
    }
    Card {
        visible: panel.category === 5
        title: qsTr("Strumenti visibili e ordine")
        Body { text: qsTr("Mostra gli strumenti che usi di più e cambia il loro ordine. Mantieni almeno uno strumento visibile.") }
        Repeater {
            model: panel.orderedToolbarActions
            RowLayout {
                id: toolRow
                required property string modelData
                readonly property int orderIndex: panel.preferences.toolbarActions.indexOf(modelData)
                readonly property string label: panel.allToolbarActions.find(action => action.value === modelData).label
                Layout.fillWidth: true
                Toggle {
                    objectName: "toolbarAction_" + toolRow.modelData
                    text: toolRow.label
                    checked: toolRow.orderIndex >= 0
                    enabled: !checked || panel.preferences.toolbarActions.length > 1
                    onToggled: panel.toggleToolbarAction(toolRow.modelData)
                }
                Action {
                    objectName: "toolbarUp_" + toolRow.modelData
                    text: "↑"
                    Accessible.name: qsTr("Sposta %1 prima").arg(toolRow.label)
                    enabled: toolRow.orderIndex > 0
                    onClicked: panel.moveToolbarAction(toolRow.modelData, -1)
                }
                Action {
                    objectName: "toolbarDown_" + toolRow.modelData
                    text: "↓"
                    Accessible.name: qsTr("Sposta %1 dopo").arg(toolRow.label)
                    enabled: toolRow.orderIndex >= 0 && toolRow.orderIndex < panel.preferences.toolbarActions.length - 1
                    onClicked: panel.moveToolbarAction(toolRow.modelData, 1)
                }
            }
        }
    }
    Card {
        visible: panel.category === 5
        title: qsTr("Gesture e lettura")
        Choice { preferenceKey: "doubleTapAction"; label: qsTr("Doppio tocco sul libro"); choices: [{ value: "zoom", label: qsTr("Alterna zoom") }, { value: "fullPage", label: qsTr("Alterna schermo intero") }, { value: "refresh", label: qsTr("Aggiorna schermo") }, { value: "none", label: qsTr("Nessuna azione") }] }
        Choice { preferenceKey: "swipeAction"; label: qsTr("Scorrimento sul libro"); choices: [{ value: "pages", label: qsTr("Cambia pagina") }, { value: "scroll", label: qsTr("Scorrimento verticale") }, { value: "none", label: qsTr("Nessuna azione") }] }
        Choice { preferenceKey: "readingZoom"; label: qsTr("Zoom iniziale dei libri"); choices: [{ value: 100, label: "100%" }, { value: 125, label: "125%" }, { value: 150, label: "150%" }, { value: 200, label: "200%" }] }
        Choice { preferenceKey: "readingMargins"; label: qsTr("Margine di lettura"); choices: [{ value: 0, label: qsTr("Nessuno") }, { value: 8, label: qsTr("Piccolo") }, { value: 16, label: qsTr("Medio") }, { value: 24, label: qsTr("Grande") }] }
        Choice { preferenceKey: "readingOrientation"; label: qsTr("Orientamento iniziale dei libri"); enabled: panel.tabletMode; choices: [{ value: -1, label: qsTr("Come l’app") }, { value: 0, label: qsTr("Verticale") }, { value: 90, label: qsTr("Orizzontale a destra") }, { value: 180, label: qsTr("Verticale capovolto") }, { value: 270, label: qsTr("Orizzontale a sinistra") }] }
        Choice { preferenceKey: "readingPanSensitivity"; label: qsTr("Velocità di spostamento"); choices: [{ value: 75, label: qsTr("Lenta") }, { value: 100, label: qsTr("Normale") }, { value: 125, label: qsTr("Rapida") }] }
        Body { text: qsTr("Le preferenze del singolo libro, quando impostate, hanno precedenza su questi valori iniziali.") }
    }
    Card {
        visible: panel.category === 5 && panel.epaperRefresh.available
        title: qsTr("Profilo di aggiornamento")
        Choice { preferenceKey: "refreshProfile"; label: qsTr("Aggiornamento automatico del display"); choices: [{ value: "balanced", label: qsTr("Bilanciato") }, { value: "clean", label: qsTr("Più pulito") }, { value: "fast", label: qsTr("Più rapido") }] }
        Body { text: qsTr("Regola gli interventi automatici del display. Il comando manuale resta disponibile con qualsiasi profilo.") }
    }
    Card {
        visible: panel.category === 10
        title: qsTr("Penna e tocco")
        Choice { preferenceKey: "penPressureCurve"; label: qsTr("Risposta alla pressione della penna"); choices: [{ value: "light", label: qsTr("Leggera") }, { value: "normal", label: qsTr("Normale") }, { value: "firm", label: qsTr("Decisa") }] }
        Choice { preferenceKey: "palmRejection"; label: qsTr("Protezione dai tocchi del palmo"); choices: [{ value: "off", label: qsTr("Disattivata") }, { value: "writing", label: qsTr("Durante la scrittura") }, { value: "extended", label: qsTr("Estesa dopo la scrittura") }] }
        Choice { preferenceKey: "shapeTolerance"; label: qsTr("Riconoscimento delle forme"); choices: [{ value: "strict", label: qsTr("Preciso") }, { value: "normal", label: qsTr("Normale") }, { value: "relaxed", label: qsTr("Tollerante") }] }
    }
    Card {
        visible: panel.category === 10
        title: qsTr("Lazo e riconoscimento")
        Toggle {
            objectName: "autoDiagramRecognition"
            text: qsTr("Riconosci automaticamente i diagrammi")
            checked: panel.preferences.autoDiagramRecognition
            onToggled: panel.preferences.autoDiagramRecognition = checked
        }
        Body { text: qsTr("Nella sezione Diagrammi propone la conversione dopo una pausa. Controlla la proposta prima di applicarla; i tratti ambigui restano sul foglio.") }
        Toggle { objectName: "lassoKeepInk"; text: qsTr("Conserva i tratti dopo la conversione"); checked: panel.preferences.lassoKeepInk; onToggled: panel.preferences.lassoKeepInk = checked }
        Choice { preferenceKey: "lassoInsertTarget"; label: qsTr("Destinazione preferita del testo riconosciuto"); choices: [{ value: "ask", label: qsTr("Scegli ogni volta") }, { value: "text", label: qsTr("Testo sul foglio") }, { value: "cells", label: qsTr("Celle della tabella") }] }
        Choice { preferenceKey: "ocrLanguage"; label: qsTr("Lingua del riconoscimento"); choices: [{ value: "auto", label: qsTr("Automatica") }, { value: "it", label: "Italiano" }, { value: "en", label: "English" }] }
        Toggle { objectName: "ocrNumericMode"; text: qsTr("Preferisci numeri per le celle"); checked: panel.preferences.ocrNumericMode; onToggled: panel.preferences.ocrNumericMode = checked }
        Body { text: qsTr("Le preferenze preparano l’anteprima: puoi controllare e correggere il testo prima di applicarlo al foglio. La lingua automatica usa quella del quaderno.") }
        ListEditor { preferenceKey: "ocrWordList"; label: qsTr("Parole personali"); hint: qsTr("Una parola per riga, fino a 100. Mantiene la grafia delle parole riconosciute, senza aggiungere parole al documento.") }
        ListEditor { preferenceKey: "ocrCorrections"; label: qsTr("Correzioni personali"); hint: qsTr("Una correzione per riga nel formato origine=correzione, fino a 100. Viene proposta nell’anteprima, prima della conferma.") }
    }
    Card {
        visible: panel.category === 0 || panel.category === 1
        title: qsTr("Sincronizzazione automatica di Quaderno")
        Choice { preferenceKey: "wifiAutoSync"; label: qsTr("Quando sincronizzare"); choices: [{ value: "manual", label: qsTr("Solo manualmente") }, { value: "onChange", label: qsTr("Dopo le modifiche") }, { value: "onOpen", label: qsTr("All’apertura di un quaderno") }, { value: "interval", label: qsTr("A intervalli regolari") }] }
        Choice { preferenceKey: "syncTransport"; label: qsTr("Collegamento automatico"); choices: [{ value: "auto", label: qsTr("Preferisci Wi-Fi, altrimenti Bluetooth") }, { value: "wifi", label: qsTr("Solo Wi-Fi") }, { value: "bluetooth", label: qsTr("Solo Bluetooth") }] }
        Choice { preferenceKey: "wifiSyncMinutes"; label: qsTr("Intervallo"); visible: panel.preferences.wifiAutoSync === "interval"; choices: [{ value: 5, label: qsTr("5 minuti") }, { value: 15, label: qsTr("15 minuti") }, { value: 30, label: qsTr("30 minuti") }, { value: 60, label: qsTr("60 minuti") }] }
        Toggle { objectName: "wifiChargingOnly"; visible: panel.tabletMode; text: qsTr("Sincronizza solo durante la ricarica del tablet"); checked: panel.preferences.wifiChargingOnly; onToggled: panel.preferences.wifiChargingOnly = checked }
        ListEditor { preferenceKey: "trustedWifiSsids"; label: qsTr("Reti Wi-Fi attendibili"); hint: qsTr("Una rete per riga, massimo 32. Lascia vuoto per non limitare le reti. I nomi devono corrispondere esattamente; non inserire password.") }
        Body { text: qsTr("L’elenco delle reti limita solo il Wi-Fi. Il Bluetooth usa il dispositivo abbinato e deve essere già attivo. Un trasferimento in corso mantiene il proprio collegamento.") }
        Choice { preferenceKey: "syncConflictPolicy"; label: qsTr("Se entrambe le copie sono state modificate"); choices: [{ value: "ask", label: qsTr("Chiedi cosa fare") }, { value: "keepBoth", label: qsTr("Conserva entrambe le copie") }] }
        Body { text: qsTr("Dopo le modifiche attende una pausa nella scrittura. Alla riconnessione recupera i quaderni selezionati. Tieni Quaderno aperto e sbloccato su entrambi i dispositivi; il primo invio richiede una revisione e i conflitti seguono la politica scelta.") }
    }
    Card {
        visible: panel.category === 11
        title: qsTr("Backup locali")
        Choice { preferenceKey: "backupSchedule"; label: qsTr("Frequenza del backup"); choices: [{ value: "manual", label: qsTr("Solo manualmente") }, { value: "daily", label: qsTr("Ogni giorno") }, { value: "weekly", label: qsTr("Ogni settimana") }] }
        Choice { preferenceKey: "backupRetention"; label: qsTr("Quanti backup conservare"); choices: [{ value: 1, label: "1" }, { value: 5, label: "5" }, { value: 10, label: "10" }, { value: 20, label: "20" }] }
        Body { text: qsTr("I backup pianificati vengono creati quando Quaderno è aperto. Restano su questo dispositivo; copia i file altrove per proteggerli dalla perdita del dispositivo.") }
        Action { objectName: "backupNow"; visible: panel.maintenance !== null; text: qsTr("Crea backup adesso"); enabled: panel.maintenance !== null && !panel.maintenance.busy && panel.maintenance.allowed; onClicked: panel.maintenance.backup() }
    }
    Card {
        visible: panel.category === 11
        title: qsTr("Spazio e cache")
        Choice { preferenceKey: "cacheLimitMiB"; label: qsTr("Limite della cache dei libri"); choices: [{ value: 32, label: "32 MiB" }, { value: 64, label: "64 MiB" }, { value: 128, label: "128 MiB" }, { value: 256, label: "256 MiB" }] }
        Body { visible: panel.maintenance !== null; text: qsTr("Libreria: %1 · Oggetti: %2 · Cache: %3 · Backup: %4").arg(panel.storageSize(panel.maintenance?.storage.libraryBytes)).arg(panel.storageSize(panel.maintenance?.storage.objectBytes)).arg(panel.storageSize(panel.maintenance?.storage.cacheBytes)).arg(panel.storageSize(panel.maintenance?.storage.backupBytes)) }
        Body { text: qsTr("Svuotare la cache conserva documenti e annotazioni. Le pagine necessarie verranno caricate di nuovo.") }
        Flow {
            visible: panel.maintenance !== null
            Layout.fillWidth: true
            spacing: 6 * panel.uiScale
            Action { objectName: "clearBookCache"; text: qsTr("Svuota cache"); enabled: panel.maintenance !== null && !panel.maintenance.busy && panel.maintenance.allowed; onClicked: panel.maintenance.clearCache() }
            Action { objectName: "refreshStorageStatus"; text: qsTr("Aggiorna spazio"); enabled: panel.maintenance !== null && !panel.maintenance.busy; onClicked: panel.maintenance.refreshStatus() }
        }
        Body { visible: (panel.maintenance?.status || "").length > 0; text: panel.maintenance?.status || "" }
        Body { visible: (panel.maintenance?.error || "").length > 0; text: panel.maintenance?.error || ""; color: "#942922" }
    }
    Card {
        visible: panel.category === 6
        title: qsTr("Destinazioni Apple preferite")
        Body { text: qsTr("Configura sul Mac l’app proposta quando condividi i contenuti. Dal Paper Pro trasferisci prima il quaderno al Mac; queste preferenze sono locali e non collegano un Apple Account al tablet.") }
        Choice { enabled: !panel.tabletMode && Qt.platform.os === "osx"; preferenceKey: "appleWritingDestination"; label: qsTr("Scrittura"); choices: [{ value: "notes", label: "Apple Notes" }, { value: "files", label: qsTr("Salva file") }] }
        Choice { enabled: !panel.tabletMode && Qt.platform.os === "osx"; preferenceKey: "appleTableDestination"; label: qsTr("Tabelle"); choices: [{ value: "numbers", label: "Apple Numbers" }, { value: "files", label: qsTr("Salva file") }] }
        Choice { enabled: !panel.tabletMode && Qt.platform.os === "osx"; preferenceKey: "appleChartDestination"; label: qsTr("Grafici"); choices: [{ value: "keynote", label: "Apple Keynote" }, { value: "files", label: qsTr("Salva file") }] }
        Choice { enabled: !panel.tabletMode && Qt.platform.os === "osx"; preferenceKey: "appleBookDestination"; label: qsTr("Libri"); choices: [{ value: "books", label: "Apple Books" }, { value: "files", label: qsTr("Salva file") }] }
    }
    Body { visible: panel.preferences.error.length > 0; text: panel.preferences.error; color: "#942922" }
    Body { visible: panel.deviceInfo.error.length > 0; text: panel.deviceInfo.error; color: "#942922" }
}
