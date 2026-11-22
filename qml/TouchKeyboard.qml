import QtQuick
import QtQuick.Layouts
import Quaderno

ColumnLayout {
    id: keyboard

    required property Item target
    required property real uiScale
    property bool upperCase: false
    property bool symbols: false
    property bool editingTools: false
    property bool multiline: Boolean(target && target.textFormat !== undefined)
    readonly property bool writable: Boolean(target && target.enabled && !target.readOnly)
    readonly property bool hasSelection: Boolean(target && target.selectionStart !== target.selectionEnd)
    readonly property bool clipboardAllowed: Boolean(target && (target.echoMode === undefined || target.echoMode === TextInput.Normal))
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
            editingTools = false
        }
    }

    function typeKey(key: string) {
        if (!writable)
            return
        const start = target.selectionStart
        const end = target.selectionEnd
        if (start !== end) {
            target.remove(start, end)
            target.cursorPosition = start
        } else if (key === "⌫" && start > 0) {
            target.remove(previousPosition(start), start)
            return
        }
        if (key !== "⌫")
            target.insert(target.cursorPosition, key)
    }

    // Text positions use UTF-16 units. Keep supplementary characters intact.
    function previousPosition(position: int): int {
        if (position <= 0) return 0
        const text = target.text
        const last = text.charCodeAt(position - 1)
        const preceding = position > 1 ? text.charCodeAt(position - 2) : 0
        return position - (last >= 0xdc00 && last <= 0xdfff && preceding >= 0xd800 && preceding <= 0xdbff ? 2 : 1)
    }
    function nextPosition(position: int): int {
        const text = target.text
        if (position >= text.length) return text.length
        const first = text.charCodeAt(position)
        const following = text.charCodeAt(position + 1)
        return position + (first >= 0xd800 && first <= 0xdbff && following >= 0xdc00 && following <= 0xdfff ? 2 : 1)
    }
    function moveCursor(direction: int) {
        if (!target) return
        const position = hasSelection ? (direction < 0 ? target.selectionStart : target.selectionEnd)
            : direction < 0 ? previousPosition(target.cursorPosition) : nextPosition(target.cursorPosition)
        target.cursorPosition = position
        target.forceActiveFocus()
    }
    function moveToEnd(end: bool) {
        if (!target) return
        target.cursorPosition = end ? target.text.length : 0
        target.forceActiveFocus()
    }
    function deleteForward() {
        if (!writable) return
        if (hasSelection) typeKey("⌫")
        else target.remove(target.cursorPosition, nextPosition(target.cursorPosition))
    }
    function editCommand(command: string) {
        if (!target || typeof target[command] !== "function") return
        if (["copy", "cut", "paste"].includes(command) && !clipboardAllowed) return
        if (["cut", "paste", "undo", "redo"].includes(command) && !writable) return
        target[command]()
        target.forceActiveFocus()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 3 * keyboard.uiScale
        PaperButton {
            uiScale: keyboard.uiScale
            focusPolicy: Qt.NoFocus
            text: "|←"
            Accessible.name: qsTr("Inizio del testo")
            enabled: Boolean(keyboard.target)
            onClicked: keyboard.moveToEnd(false)
        }
        PaperButton {
            objectName: "keyboardLeft"
            uiScale: keyboard.uiScale
            focusPolicy: Qt.NoFocus
            text: "←"
            Accessible.name: qsTr("Sposta cursore a sinistra")
            enabled: Boolean(keyboard.target)
            onClicked: keyboard.moveCursor(-1)
        }
        PaperButton {
            objectName: "keyboardRight"
            uiScale: keyboard.uiScale
            focusPolicy: Qt.NoFocus
            text: "→"
            Accessible.name: qsTr("Sposta cursore a destra")
            enabled: Boolean(keyboard.target)
            onClicked: keyboard.moveCursor(1)
        }
        PaperButton {
            uiScale: keyboard.uiScale
            focusPolicy: Qt.NoFocus
            text: "→|"
            Accessible.name: qsTr("Fine del testo")
            enabled: Boolean(keyboard.target)
            onClicked: keyboard.moveToEnd(true)
        }
        PaperButton {
            objectName: "keyboardDelete"
            uiScale: keyboard.uiScale
            focusPolicy: Qt.NoFocus
            text: "⌦"
            Accessible.name: qsTr("Cancella in avanti")
            enabled: keyboard.writable
            onClicked: keyboard.deleteForward()
        }
        PaperButton {
            uiScale: keyboard.uiScale; focusPolicy: Qt.NoFocus; Layout.fillWidth: true; Layout.minimumWidth: 0
            text: qsTr("Modifica"); checkable: true; checked: keyboard.editingTools; filled: checked
            onClicked: keyboard.editingTools = !keyboard.editingTools
        }
    }

    Flow {
        visible: keyboard.editingTools
        Layout.fillWidth: true
        spacing: 3 * keyboard.uiScale
        PaperButton { uiScale: keyboard.uiScale; focusPolicy: Qt.NoFocus; text: qsTr("Seleziona tutto"); enabled: Boolean(keyboard.target && keyboard.target.text.length); onClicked: keyboard.editCommand("selectAll") }
        PaperButton { uiScale: keyboard.uiScale; focusPolicy: Qt.NoFocus; text: qsTr("Copia"); enabled: keyboard.clipboardAllowed && keyboard.hasSelection; onClicked: keyboard.editCommand("copy") }
        PaperButton { uiScale: keyboard.uiScale; focusPolicy: Qt.NoFocus; text: qsTr("Taglia"); enabled: keyboard.clipboardAllowed && keyboard.writable && keyboard.hasSelection; onClicked: keyboard.editCommand("cut") }
        PaperButton { uiScale: keyboard.uiScale; focusPolicy: Qt.NoFocus; text: qsTr("Incolla"); enabled: keyboard.clipboardAllowed && keyboard.writable && Boolean(keyboard.target.canPaste); onClicked: keyboard.editCommand("paste") }
        PaperButton { uiScale: keyboard.uiScale; focusPolicy: Qt.NoFocus; text: qsTr("Annulla"); enabled: keyboard.writable && Boolean(keyboard.target.canUndo); onClicked: keyboard.editCommand("undo") }
        PaperButton { uiScale: keyboard.uiScale; focusPolicy: Qt.NoFocus; text: qsTr("Ripeti"); enabled: keyboard.writable && Boolean(keyboard.target.canRedo); onClicked: keyboard.editCommand("redo") }
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
                    enabled: keyboard.writable
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
            enabled: keyboard.writable
            onClicked: keyboard.typeKey(" ")
        }

        PaperButton {
            objectName: "keyboardReturn"
            visible: keyboard.multiline
            uiScale: keyboard.uiScale
            focusPolicy: Qt.NoFocus
            text: "↵"
            Accessible.name: qsTr("A capo")
            enabled: keyboard.writable
            onClicked: keyboard.typeKey("\n")
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
