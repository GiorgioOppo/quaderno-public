import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: dialog
    objectName: "smtpDialog"

    required property Notebook notebook
    property var mail: null
    required property real uiScale
    required property bool tabletMode
    required property bool showTouchKeyboard
    property bool locked: false
    property bool configuring: true
    property bool confirmingRemoval: false
    property bool showStatus: false
    property bool sent: false
    property var pdfOption: null
    property string sourceBook: ""
    property string sourcePage: ""
    property Item activeField: null
    readonly property bool busy: Boolean(mail?.busy)
    readonly property bool validPdf: !locked && pdfOption !== null && notebook.bookId === sourceBook
        && notebook.pageId === sourcePage
    readonly property bool keyboardVisible: showTouchKeyboard && activeField !== null && activeField.activeFocus && !busy

    title: configuring ? qsTr("Server di posta in uscita (SMTP)") : qsTr("Invia PDF via email")
    anchors.centerIn: Overlay.overlay
    width: Math.min(610 * uiScale, (parent ? parent.width : 610 * uiScale) - 24 * uiScale)
    height: Math.min((configuring ? 690 : 570) * uiScale, (parent ? parent.height : 690 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    modal: true
    dim: !tabletMode
    closePolicy: busy ? Popup.NoAutoClose : Popup.CloseOnEscape

    function beginSettings() {
        if (locked || !mail || busy) return
        pdfOption = null
        fillSettings()
        open()
    }

    function beginPdf(option: var) {
        if (locked || !mail || busy || !option) return
        sourceBook = notebook.bookId
        sourcePage = notebook.pageId
        pdfOption = option
        if (mail.configured)
            fillComposer()
        else
            fillSettings()
        open()
    }

    function fillSettings() {
        configuring = true
        confirmingRemoval = false
        showStatus = false
        activeField = null
        const saved = mail.settings
        host.text = saved.host || ""
        username.text = saved.username || ""
        sender.text = saved.sender || ""
        security.currentIndex = saved.security === "tls" ? 1 : 0
        port.text = String(saved.port || (security.currentIndex === 1 ? 465 : 587))
        password.clear()
        scroll.contentItem.contentY = 0
    }

    function fillComposer() {
        configuring = false
        showStatus = false
        sent = false
        activeField = null
        recipient.clear()
        subject.text = notebook.title.slice(0, subject.maximumLength)
        message.clear()
        scroll.contentItem.contentY = 0
    }

    function saveSettings() {
        showStatus = true
        if (mail.saveSettings({host: host.text, port: Number(port.text), username: username.text,
            sender: sender.text, security: security.currentIndex === 1 ? "tls" : "starttls"}, password.text)) {
            password.clear()
            if (validPdf)
                fillComposer()
            else
                close()
        }
    }

    function keepFieldVisible() {
        if (!keyboardVisible || !activeField) return
        const area = scroll.contentItem
        const top = activeField.mapToItem(form, 0, 0).y
        const bottom = top + activeField.height
        if (top < area.contentY)
            area.contentY = top
        else if (bottom > area.contentY + scroll.availableHeight)
            area.contentY = bottom - scroll.availableHeight
    }

    onLockedChanged: if (locked) {
        password.clear()
        recipient.clear()
        subject.clear()
        message.clear()
        close()
    }
    onClosed: {
        activeField = null
        password.clear()
        recipient.clear()
        subject.clear()
        message.clear()
        pdfOption = null
        sourceBook = ""
        sourcePage = ""
    }

    Connections {
        target: dialog.notebook
        function onDocumentChanged() {
            if (dialog.visible && dialog.pdfOption !== null && (dialog.notebook.bookId !== dialog.sourceBook
                    || dialog.notebook.pageId !== dialog.sourcePage)) {
                if (dialog.busy) dialog.mail.cancel()
                dialog.close()
            }
        }
    }

    Connections {
        target: dialog.mail
        function onFinished(success) {
            if (dialog.visible && !dialog.configuring)
                dialog.sent = success
        }
    }

    component Caption: Label {
        Layout.fillWidth: true
        color: "#515159"
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        font.pixelSize: 14 * dialog.uiScale
    }

    component Field: TextField {
        id: field
        Layout.fillWidth: true
        Layout.minimumHeight: 44 * dialog.uiScale
        font.pixelSize: 16 * dialog.uiScale
        selectByMouse: !dialog.tabletMode
        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        Accessible.name: placeholderText
        onActiveFocusChanged: if (activeFocus) {
            dialog.activeField = field
            Qt.callLater(dialog.keepFieldVisible)
        }
    }

    component Action: PaperButton {
        uiScale: dialog.uiScale
    }

    contentItem: ColumnLayout {
        spacing: 10 * dialog.uiScale

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            onHeightChanged: Qt.callLater(dialog.keepFieldVisible)
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: form
                width: scroll.availableWidth
                spacing: 8 * dialog.uiScale

                ColumnLayout {
                    visible: dialog.configuring
                    Layout.fillWidth: true
                    spacing: 8 * dialog.uiScale

                    Field {
                        id: host
                        objectName: "smtpHost"
                        placeholderText: qsTr("Nome del server SMTP")
                        maximumLength: 253
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        ComboBox {
                            id: security
                            Layout.fillWidth: true
                            Layout.minimumHeight: 44 * dialog.uiScale
                            model: [qsTr("STARTTLS"), qsTr("TLS implicito")]
                            Accessible.name: qsTr("Sicurezza della connessione")
                            onActivated: {
                                if (!port.text.length || port.text === "587" || port.text === "465")
                                    port.text = currentIndex === 1 ? "465" : "587"
                            }
                        }
                        Field {
                            id: port
                            objectName: "smtpPort"
                            Layout.fillWidth: false
                            Layout.preferredWidth: 110 * dialog.uiScale
                            placeholderText: qsTr("Porta")
                            maximumLength: 5
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator { bottom: 1; top: 65535 }
                        }
                    }
                    Field {
                        id: username
                        objectName: "smtpUsername"
                        placeholderText: qsTr("Nome utente SMTP (se richiesto)")
                        maximumLength: 254
                    }
                    Field {
                        id: sender
                        objectName: "smtpSender"
                        placeholderText: qsTr("Indirizzo mittente")
                        maximumLength: 254
                    }
                    Field {
                        id: password
                        objectName: "smtpPassword"
                        placeholderText: dialog.mail?.settings.hasPassword
                            ? qsTr("Nuova password (facoltativa)") : qsTr("Password o password per app")
                        maximumLength: 512
                        echoMode: TextInput.Password
                        passwordMaskDelay: 0
                        inputMethodHints: Qt.ImhHiddenText | Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    }
                    Caption {
                        text: qsTr("Usa i parametri SMTP del tuo gestore di posta. La password salvata resta su questo dispositivo.")
                    }
                    Action {
                        visible: Boolean(dialog.mail?.configured) && !dialog.confirmingRemoval
                        text: qsTr("Rimuovi configurazione")
                        onClicked: {
                            dialog.confirmingRemoval = true
                            dialog.activeField = null
                        }
                    }
                    RowLayout {
                        visible: dialog.confirmingRemoval
                        Caption { text: qsTr("Rimuovere il server e la password salvata?") }
                        Action {
                            text: qsTr("Rimuovi")
                            onClicked: {
                                if (dialog.mail.clearSettings())
                                    dialog.fillSettings()
                                dialog.showStatus = true
                            }
                        }
                    }
                }

                ColumnLayout {
                    visible: !dialog.configuring
                    Layout.fillWidth: true
                    spacing: 8 * dialog.uiScale

                    Caption {
                        text: qsTr("Allegato: %1 · %2").arg(dialog.notebook.title)
                            .arg(dialog.pdfOption?.format === "pdf-page" ? qsTr("Pagina corrente") : qsTr("Intero quaderno"))
                    }
                    Field {
                        id: recipient
                        objectName: "smtpRecipient"
                        placeholderText: qsTr("Indirizzo destinatario")
                        maximumLength: 254
                        enabled: !dialog.busy && !dialog.sent
                        inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                    }
                    Field {
                        id: subject
                        placeholderText: qsTr("Oggetto")
                        maximumLength: 160
                        enabled: !dialog.busy && !dialog.sent
                        inputMethodHints: Qt.ImhNone
                    }
                    TextArea {
                        id: message
                        Layout.fillWidth: true
                        Layout.minimumHeight: 108 * dialog.uiScale
                        placeholderText: qsTr("Messaggio (facoltativo)")
                        font.pixelSize: 16 * dialog.uiScale
                        enabled: !dialog.busy && !dialog.sent
                        textFormat: TextEdit.PlainText
                        wrapMode: TextEdit.Wrap
                        Accessible.name: placeholderText
                        onTextChanged: if (length > 4096) remove(4096, length)
                        onActiveFocusChanged: if (activeFocus) {
                            dialog.activeField = message
                            Qt.callLater(dialog.keepFieldVisible)
                        }
                    }
                }

                Caption {
                    objectName: "smtpStatus"
                    visible: dialog.showStatus && text.length > 0
                    text: dialog.mail?.status || ""
                }
                ProgressBar {
                    Layout.fillWidth: true
                    visible: dialog.busy
                    indeterminate: true
                }
            }
        }

        TouchKeyboard {
            Layout.fillWidth: true
            visible: dialog.keyboardVisible
            target: dialog.activeField
            uiScale: dialog.uiScale
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Action {
                text: dialog.busy ? qsTr("Annulla invio") : qsTr("Chiudi")
                onClicked: {
                    if (dialog.busy)
                        dialog.mail.cancel()
                    else
                        dialog.close()
                }
            }
            Action {
                objectName: "smtpSubmit"
                visible: !dialog.sent
                text: dialog.configuring ? qsTr("Salva") : qsTr("Invia PDF")
                filled: true
                enabled: dialog.mail !== null && !dialog.busy && !dialog.confirmingRemoval
                    && (dialog.configuring ? host.text.trim().length > 0 && port.acceptableInput && sender.text.trim().length > 0
                        : dialog.validPdf && recipient.text.trim().length > 0)
                onClicked: {
                    if (dialog.configuring)
                        dialog.saveSettings()
                    else if (dialog.validPdf) {
                        dialog.showStatus = true
                        dialog.mail.sendPdf(dialog.notebook, dialog.pdfOption, recipient.text, subject.text, message.text)
                    }
                }
            }
        }
    }
}
