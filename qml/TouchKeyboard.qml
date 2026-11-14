import QtQuick
import QtQuick.Layouts
import Quaderno

ColumnLayout {
    id: keyboard

    required property Item target
    required property real uiScale
    property bool upperCase: false
    property bool symbols: false
    readonly property bool italian: AppLanguage.keyboardLayout === "it"
    readonly property var italianRows: [
        ["\\1234567890'ì", "|!\"£$%&/()=?Ì"],
        ["qwertyuiopè+é", "QWERTYUIOPÈ*É"],
        ["asdfghjklòàù", "ASDFGHJKLÒÀÙ"],
        ["<zxcvbnm,.-⌫", ">ZXCVBNM;:_⌫"]
    ]
    readonly property var englishRows: [
        ["\x601234567890-=", "~!@#$%^&*()_+"],
        ["qwertyuiop[]\\", "QWERTYUIOP{}|"],
        ["asdfghjkl;'", "ASDFGHJKL:\""],
        ["zxcvbnm,./⌫", "ZXCVBNM<>?⌫"]
    ]
    readonly property var symbolRows: [["!@#$%^&*()"], ["-_+=[]{}|\\"], [":;\"'<>,.?/"], ["\x60~€£ç°§⌫"]]

    spacing: 4 * uiScale
    onVisibleChanged: {
        if (!visible) {
            upperCase = false
            symbols = false
        }
    }

    function typeKey(key: string) {
        if (!target)
            return
        const start = target.selectionStart
        const end = target.selectionEnd
        if (start !== end) {
            target.remove(start, end)
            target.cursorPosition = start
        } else if (key === "⌫" && start > 0) {
            target.remove(start - 1, start)
            return
        }
        if (key !== "⌫")
            target.insert(target.cursorPosition, key)
    }

    Repeater {
        model: keyboard.symbols ? keyboard.symbolRows : keyboard.italian ? keyboard.italianRows : keyboard.englishRows

        RowLayout {
            id: keyRow
            required property var modelData
            readonly property string letters: modelData[0]
            readonly property string shifted: modelData[1] ?? letters
            Layout.fillWidth: true
            spacing: 3 * keyboard.uiScale

            Repeater {
                model: keyRow.letters.length

                PaperButton {
                    required property int index
                    uiScale: keyboard.uiScale
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 1
                    focusPolicy: Qt.NoFocus
                    leftPadding: 0
                    rightPadding: 0
                    text: (keyboard.upperCase ? keyRow.shifted : keyRow.letters)[index]
                    Accessible.name: text === "⌫" ? qsTr("Cancella carattere") : text
                    onClicked: keyboard.typeKey(text)
                }
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true

        PaperButton {
            visible: !keyboard.symbols
            uiScale: keyboard.uiScale
            focusPolicy: Qt.NoFocus
            text: "⇧"
            Accessible.name: qsTr("Maiuscole")
            checkable: true
            checked: keyboard.upperCase
            filled: checked
            onClicked: keyboard.upperCase = !keyboard.upperCase
        }

        PaperButton {
            uiScale: keyboard.uiScale
            focusPolicy: Qt.NoFocus
            text: keyboard.symbols ? qsTr("ABC") : qsTr("Simboli")
            onClicked: keyboard.symbols = !keyboard.symbols
        }

        PaperButton {
            uiScale: keyboard.uiScale
            Layout.fillWidth: true
            focusPolicy: Qt.NoFocus
            text: qsTr("Spazio")
            onClicked: keyboard.typeKey(" ")
        }

        PaperButton {
            uiScale: keyboard.uiScale
            focusPolicy: Qt.NoFocus
            text: keyboard.italian ? "IT" : "EN"
            Accessible.name: keyboard.italian ? qsTr("Tastiera italiana. Passa all’inglese.") : qsTr("Tastiera inglese. Passa all’italiano.")
            onClicked: {
                AppLanguage.selectKeyboardLayout(keyboard.italian ? "en" : "it")
                keyboard.upperCase = false
                keyboard.symbols = false
            }
        }
    }
}
