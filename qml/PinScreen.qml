import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

FocusScope {
    id: screen

    required property PinLock pinLock
    required property real uiScale
    property bool showExit: false
    property bool showTouchKeyboard: true
    signal exitRequested()

    readonly property real panelScale: Math.min(uiScale,
        Math.max(0.8, (height - 24 * uiScale) / (showExit ? 510 : 462)))
    readonly property color ink: "#24262c"
    readonly property color accent: "#155cc2"
    readonly property color border: "#d9dce2"
    readonly property color surface: "#f4f5f7"

    visible: pinLock.locked
    focus: visible

    function resetInput() {
        pinField.clear()
        if (visible && pinField.enabled)
            pinField.forceActiveFocus()
    }

    function submit() {
        if (!pinField.enabled || pinField.length < 4)
            return
        const attempt = pinField.text
        pinField.clear()
        pinLock.unlock(attempt)
    }

    onVisibleChanged: resetInput()
    Component.onCompleted: resetInput()

    Rectangle {
        anchors.fill: parent
        color: screen.surface
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
    }

    Rectangle {
        id: panel

        anchors.centerIn: parent
        width: Math.min(parent.width - 28 * screen.uiScale, 370 * screen.panelScale)
        height: contents.implicitHeight + 2 * contents.anchors.margins
        color: "#ffffff"
        border.color: screen.border
        border.width: 1
        radius: 22 * screen.panelScale

        ColumnLayout {
            id: contents

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 22 * screen.panelScale
            spacing: 8 * screen.panelScale

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                color: screen.ink
                font.pixelSize: 25 * screen.panelScale
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                text: qsTr("Sblocca Quaderno")
            }

            Label {
                Layout.fillWidth: true
                Layout.minimumHeight: 32 * screen.panelScale
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                color: screen.pinLock.error.length === 0 ? "#595d66" : "#a42c35"
                wrapMode: Text.Wrap
                font.pixelSize: 14 * screen.panelScale
                text: screen.pinLock.secondsRemaining > 0
                    ? qsTr("Riprova tra %1 s.").arg(screen.pinLock.secondsRemaining)
                    : screen.pinLock.error || qsTr("Inserisci il PIN di Quaderno.")
            }

            TextField {
                id: pinField

                Layout.fillWidth: true
                Layout.preferredHeight: 46 * screen.panelScale
                enabled: screen.pinLock.available && screen.pinLock.secondsRemaining === 0
                Accessible.name: qsTr("PIN")
                color: screen.ink
                selectionColor: screen.accent
                selectedTextColor: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 20 * screen.panelScale
                font.letterSpacing: 4 * screen.panelScale
                echoMode: TextInput.Password
                passwordMaskDelay: 0
                inputMethodHints: Qt.ImhHiddenText | Qt.ImhSensitiveData | Qt.ImhDigitsOnly
                                  | Qt.ImhNoPredictiveText
                maximumLength: 8
                validator: RegularExpressionValidator { regularExpression: /[0-9]{0,8}/ }
                onAccepted: screen.submit()
                onEnabledChanged: {
                    if (enabled && screen.visible)
                        forceActiveFocus()
                }

                background: Rectangle {
                    color: screen.surface
                    border.color: pinField.activeFocus ? screen.accent : screen.border
                    border.width: pinField.activeFocus ? Math.max(2, screen.panelScale) : 1
                    radius: 12 * screen.panelScale
                }
            }

            PinKeypad {
                visible: screen.showTouchKeyboard
                Layout.fillWidth: true
                uiScale: screen.panelScale
                enabled: pinField.enabled

                onKeyPressed: key => {
                    const start = pinField.selectionStart
                    const end = pinField.selectionEnd
                    if (start !== end) {
                        pinField.remove(start, end)
                        pinField.cursorPosition = start
                    } else if (key === "⌫" && start > 0) {
                        pinField.remove(start - 1, start)
                        return
                    }
                    if (key !== "⌫")
                        pinField.insert(pinField.cursorPosition, key)
                }
            }

            PaperButton {
                Layout.fillWidth: true
                uiScale: screen.panelScale
                filled: true
                enabled: pinField.enabled && pinField.length >= 4
                text: qsTr("Sblocca")
                onClicked: screen.submit()
            }

            PaperButton {
                Layout.alignment: Qt.AlignHCenter
                uiScale: screen.panelScale
                quiet: true
                visible: screen.showExit
                iconName: "exit"
                text: qsTr("Esci da Quaderno")
                onClicked: screen.exitRequested()
            }
        }
    }
}
