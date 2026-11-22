pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Popup {
    id: popup
    objectName: "quickControlsPopup"
    required property var preferences
    required property var deviceInfo
    required property var epaper
    required property bool tabletMode
    required property real uiScale
    property int currentOrientation: preferences.orientation
    signal settingsRequested(int category)
    signal sleepRequested()
    signal refreshRequested()
    signal rotationRequested(int angle)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520 * uiScale, (parent ? parent.width : 560 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    modal: true
    dim: !tabletMode
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openSettings(category: int) {
        close()
        settingsRequested(category)
    }
    function rotate() {
        if (!tabletMode) return
        const angle = (currentOrientation + 90) % 360
        close()
        Qt.callLater(() => popup.rotationRequested(angle))
    }
    component Action: PaperButton {
        uiScale: popup.uiScale
        hoverEnabled: !popup.tabletMode
    }
    component Body: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        color: popup.preferences.highContrast ? "black" : "#1c1c1e"
        font.pixelSize: 15 * popup.uiScale
        font.bold: popup.preferences.boldLabels
    }

    background: Rectangle {
        color: "white"
        radius: 16 * popup.uiScale
        border.color: popup.preferences.highContrast ? "black" : "#a4a4ac"
        border.width: Math.max(1, popup.uiScale)
    }
    contentItem: ScrollView {
        id: quickScroll
        clip: true
        implicitHeight: Math.min(content.implicitHeight, Math.max(100 * popup.uiScale,
            (popup.parent ? popup.parent.height : 800 * popup.uiScale) - 110 * popup.uiScale))
        contentWidth: availableWidth
        contentHeight: content.implicitHeight
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            id: content
            width: quickScroll.availableWidth
            spacing: 12 * popup.uiScale
            Body { text: qsTr("Controlli rapidi"); font.pixelSize: 24 * popup.uiScale; font.bold: true }
            Flow {
                Layout.fillWidth: true
                spacing: 6 * popup.uiScale
                Action { objectName: "quickRotate"; text: qsTr("Ruota schermo"); enabled: popup.tabletMode; onClicked: popup.rotate() }
                Action { objectName: "quickRefresh"; text: qsTr("Aggiorna schermo"); enabled: popup.epaper.available; onClicked: { popup.close(); popup.refreshRequested() } }
                Action { objectName: "quickSleep"; text: qsTr("Riposa"); enabled: popup.tabletMode; onClicked: { popup.close(); popup.sleepRequested() } }
                Action { objectName: "quickWifi"; text: qsTr("Wi-Fi"); onClicked: popup.openSettings(0) }
                Action { objectName: "quickBluetooth"; text: qsTr("Bluetooth"); onClicked: popup.openSettings(4) }
                Action { objectName: "quickSettings"; text: qsTr("Impostazioni"); onClicked: popup.openSettings(9) }
            }
            Body { text: qsTr("Profilo d’uso"); font.bold: true }
            Flow {
                Layout.fillWidth: true
                spacing: 6 * popup.uiScale
                Repeater {
                    model: [{ value: "reading", label: qsTr("Lettura") }, { value: "writing", label: qsTr("Scrittura") }, { value: "drawing", label: qsTr("Disegno") }]
                    Action {
                        required property var modelData
                        objectName: "quickProfile_" + modelData.value
                        text: modelData.label
                        filled: popup.preferences.usageProfile === modelData.value
                        onClicked: {
                            const profile = modelData.value
                            popup.close()
                            Qt.callLater(() => popup.preferences.applyUsageProfile(profile))
                        }
                    }
                }
            }
            Body { text: qsTr("I profili impostano strumenti, schermo intero e gesture senza modificare i documenti.") }
            Body { visible: popup.deviceInfo.frontlightAvailable; text: qsTr("Luce di lettura"); font.bold: true }
            RowLayout {
                visible: popup.deviceInfo.frontlightAvailable
                Layout.fillWidth: true
                Action {
                    objectName: "quickLightDown"
                    text: "−"
                    Accessible.name: qsTr("Riduci la luce")
                    enabled: popup.deviceInfo.frontlightLevel > 0
                    onClicked: popup.deviceInfo.setFrontlightLevel(popup.deviceInfo.frontlightLevel - 1)
                }
                Body { Layout.fillWidth: false; text: qsTr("%1 / 5").arg(popup.deviceInfo.frontlightLevel) }
                Action {
                    objectName: "quickLightUp"
                    text: "+"
                    Accessible.name: qsTr("Aumenta la luce")
                    enabled: popup.deviceInfo.frontlightLevel >= 0 && popup.deviceInfo.frontlightLevel < 5
                    onClicked: popup.deviceInfo.setFrontlightLevel(popup.deviceInfo.frontlightLevel + 1)
                }
            }
            Body { visible: popup.preferences.error.length > 0; text: popup.preferences.error; color: "#942922" }
            Body { visible: popup.deviceInfo.error.length > 0; text: popup.deviceInfo.error; color: "#942922" }
            Action { objectName: "closeQuickControls"; Layout.alignment: Qt.AlignRight; text: qsTr("Chiudi"); onClicked: popup.close() }
        }
    }
}
