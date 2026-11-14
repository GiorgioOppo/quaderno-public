pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Quaderno

GridLayout {
    id: keypad

    required property real uiScale
    signal keyPressed(string key)

    columns: 3
    rowSpacing: 6 * uiScale
    columnSpacing: 6 * uiScale

    Repeater {
        model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "", "0", "⌫"]

        PaperButton {
            id: keyButton

            required property string modelData

            uiScale: keypad.uiScale
            font.pixelSize: 18 * uiScale
            opacity: modelData.length === 0 ? 0 : 1
            enabled: modelData.length > 0
            focusPolicy: Qt.NoFocus
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.preferredHeight: 46 * uiScale
            Layout.minimumWidth: 0
            text: modelData
            Accessible.name: modelData === "⌫" ? qsTr("Cancella cifra") : modelData
            Accessible.ignored: modelData.length === 0
            onClicked: keypad.keyPressed(modelData)

            background: Rectangle {
                color: keyButton.down ? "#e2ebf9" : keyButton.hovered && keyButton.enabled ? "#edf2fa"
                                     : "#f4f5f7"
                border.color: keyButton.down ? "#155cc2" : "#d9dce2"
                border.width: Math.max(1, Math.round(keypad.uiScale))
                radius: 13 * keypad.uiScale
            }
        }
    }
}
