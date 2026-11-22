import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: editor
    objectName: "markdownEditor"
    required property Notebook notebook
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    property string folderId: ""
    property bool creating: false
    property string sourceBook: ""
    property string destinationFolder: ""
    property string initialText: ""
    property string initialName: ""
    property string errorText: ""
    property bool confirmingDiscard: false
    property bool closingApplication: false
    property Item keyboardTarget: null
    readonly property bool dirty: source.text !== initialText || nameField.text !== initialName
    readonly property Item remoteSurface: opened && contentItem ? contentItem.parent : null
    readonly property bool previewing: mode.currentIndex === 1
    readonly property bool keyboardOpen: showTouchKeyboard && keyboardTarget !== null && !previewing
    readonly property int wordCount: source.text.split(/\s+/).filter(token => /[^\s#*_`~>\[\](){},.!?:;|+\-]/.test(token)).length
    signal editingStarted
    signal saved
    signal applicationCloseApproved

    title: creating ? qsTr("Nuovo Markdown") : qsTr("Modifica Markdown")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(650 * uiScale, (parent ? parent.width : 650 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    modal: true
    dim: !tabletMode
    focus: true
    closePolicy: Popup.NoAutoClose

    function begin(create: bool) {
        editingStarted();
        creating = create;
        sourceBook = create ? "" : notebook.bookId;
        destinationFolder = folderId;
        initialName = "";
        initialText = create ? "" : notebook.markdownSource();
        nameField.text = initialName;
        source.text = initialText;
        errorText = "";
        confirmingDiscard = false;
        closingApplication = false;
        mode.currentIndex = 0;
        open();
    }

    function requestDiscard(application: bool) {
        closingApplication = application;
        if (dirty) {
            confirmingDiscard = true;
        } else {
            close();
            if (application)
                applicationCloseApproved();
        }
    }

    function discard() {
        const application = closingApplication;
        close();
        if (application)
            applicationCloseApproved();
    }

    function save() {
        if (creating && !nameField.text.trim().length)
            return;
        const success = creating ? notebook.importMarkdownText(source.text, nameField.text.trim(), destinationFolder) : sourceBook === notebook.bookId && notebook.updateMarkdownText(source.text);
        if (success) {
            close();
            saved();
        } else {
            errorText = notebook.error || qsTr("Riapri il documento prima di salvare.");
        }
    }

    function insertMarkup(prefix: string, suffix: string) {
        if (previewing)
            return;
        const start = source.selectionStart;
        const end = source.selectionEnd;
        const selected = source.selectedText;
        source.remove(start, end);
        source.insert(start, prefix + selected + suffix);
        source.forceActiveFocus();
        source.select(start + prefix.length, start + prefix.length + selected.length);
    }

    function insertLinePrefix(prefix: string) {
        if (previewing)
            return;
        const position = source.cursorPosition;
        const start = position > 0 ? source.text.lastIndexOf("\n", position - 1) + 1 : 0;
        source.insert(start, prefix);
        source.forceActiveFocus();
        source.cursorPosition = position + prefix.length;
    }

    onOpened: creating ? nameField.forceActiveFocus() : source.forceActiveFocus()
    onClosed: {
        keyboardTarget = null;
        nameField.clear();
        source.clear();
        initialText = "";
        initialName = "";
        sourceBook = "";
        confirmingDiscard = false;
    }
    onPreviewingChanged: if (previewing)
        keyboardTarget = null

    Shortcut {
        sequence: "Escape"
        enabled: editor.visible
        onActivated: editor.requestDiscard(false)
    }
    Shortcut {
        sequences: [StandardKey.Save]
        enabled: editor.visible && !editor.confirmingDiscard
        onActivated: editor.save()
    }

    background: Rectangle {
        color: "white"
        radius: 14 * editor.uiScale
        border.color: "#d7dce4"
    }
    header: Label {
        text: editor.title
        color: "#242723"
        font.pixelSize: 22 * editor.uiScale
        font.weight: Font.DemiBold
        leftPadding: editor.leftPadding
        rightPadding: editor.rightPadding
        topPadding: editor.topPadding
    }
    component ModeTab: TabButton {
        id: tab
        required property int modeIndex
        property bool remoteClickable: !editor.confirmingDiscard
        function remoteActivate() { mode.currentIndex = modeIndex }
        background: Rectangle {
            color: tab.checked ? "#e6effb" : "#f2f2f7"
            radius: 7 * editor.uiScale
            border.color: tab.checked ? "#0059bd" : "#d7dce4"
        }
        contentItem: Text {
            text: tab.text
            font.pixelSize: 14 * editor.uiScale
            font.weight: tab.checked ? Font.DemiBold : Font.Normal
            color: tab.checked ? "#0059bd" : "#535862"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    contentItem: ScrollView {
        id: formScroll
        clip: true
        contentWidth: availableWidth
        implicitHeight: Math.min(form.implicitHeight, Math.max(80 * editor.uiScale, (editor.parent ? editor.parent.height : 800) - editor.header.implicitHeight - editor.footer.implicitHeight - editor.topPadding - editor.bottomPadding - 24 * editor.uiScale))
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            id: form
            width: formScroll.availableWidth
            spacing: 7 * editor.uiScale
            TextField {
                id: nameField
                objectName: "markdownName"
                property bool remoteEditable: !editor.confirmingDiscard
                visible: editor.creating
                Layout.fillWidth: true
                maximumLength: 80
                placeholderText: qsTr("Titolo del documento")
                Accessible.name: placeholderText
                onActiveFocusChanged: if (activeFocus)
                    editor.keyboardTarget = nameField
            }
            TabBar {
                id: mode
                objectName: "markdownMode"
                Layout.fillWidth: true
                ModeTab {
                    modeIndex: 0
                    text: qsTr("Scrivi")
                }
                ModeTab {
                    modeIndex: 1
                    text: qsTr("Anteprima")
                }
            }
            Flow {
                visible: !editor.previewing
                Layout.fillWidth: true
                spacing: 4 * editor.uiScale
                PaperButton {
                    property bool remoteClickable: !editor.confirmingDiscard
                    uiScale: editor.uiScale
                    text: qsTr("Titolo")
                    focusPolicy: Qt.NoFocus
                    onClicked: editor.insertLinePrefix("## ")
                }
                PaperButton {
                    property bool remoteClickable: !editor.confirmingDiscard
                    uiScale: editor.uiScale
                    text: qsTr("Grassetto")
                    focusPolicy: Qt.NoFocus
                    onClicked: editor.insertMarkup("**", "**")
                }
                PaperButton {
                    property bool remoteClickable: !editor.confirmingDiscard
                    uiScale: editor.uiScale
                    text: qsTr("Corsivo")
                    focusPolicy: Qt.NoFocus
                    onClicked: editor.insertMarkup("*", "*")
                }
                PaperButton {
                    property bool remoteClickable: !editor.confirmingDiscard
                    uiScale: editor.uiScale
                    text: qsTr("Elenco")
                    focusPolicy: Qt.NoFocus
                    onClicked: editor.insertLinePrefix("- ")
                }
                PaperButton {
                    property bool remoteClickable: !editor.confirmingDiscard
                    uiScale: editor.uiScale
                    text: qsTr("Attività")
                    focusPolicy: Qt.NoFocus
                    onClicked: editor.insertLinePrefix("- [ ] ")
                }
            }
            StackLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(300 * editor.uiScale, Math.max(80 * editor.uiScale, (editor.parent ? editor.parent.height : 800) - (editor.keyboardOpen ? 530 : 345) * editor.uiScale))
                currentIndex: mode.currentIndex
                ScrollView {
                    clip: true
                    TextArea {
                        id: source
                        objectName: "markdownSource"
                        property bool remoteEditable: !editor.confirmingDiscard
                        font.pixelSize: 16 * editor.uiScale
                        placeholderText: qsTr("Scrivi in Markdown")
                        Accessible.name: placeholderText
                        textFormat: TextEdit.PlainText
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        onActiveFocusChanged: if (activeFocus)
                            editor.keyboardTarget = source
                    }
                }
                ScrollView {
                    clip: true
                    TextArea {
                        objectName: "markdownPreview"
                        text: editor.previewing ? source.text : ""
                        textFormat: TextEdit.MarkdownText
                        font.pixelSize: 16 * editor.uiScale
                        wrapMode: TextEdit.Wrap
                        readOnly: true
                        selectByMouse: true
                        Accessible.name: qsTr("Anteprima Markdown")
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("%1 parole · %2 caratteri").arg(editor.wordCount).arg(source.length)
                color: "#535862"
                font.pixelSize: 13 * editor.uiScale
            }
            PaperButton {
                property bool remoteClickable: true
                visible: editor.keyboardOpen
                Layout.alignment: Qt.AlignRight
                uiScale: editor.uiScale
                text: qsTr("Chiudi tastiera")
                quiet: true
                onClicked: editor.keyboardTarget = null
            }
            TouchKeyboard {
                visible: editor.keyboardOpen
                Layout.fillWidth: true
                uiScale: Math.max(0.75, editor.uiScale * 0.78)
                target: editor.keyboardTarget
            }
            Label {
                Layout.fillWidth: true
                visible: editor.errorText.length > 0
                text: editor.errorText
                color: "#8c2d27"
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: editor.confirmingDiscard
                text: qsTr("Ci sono modifiche non salvate. Vuoi continuare a scrivere o scartarle?")
                wrapMode: Text.WordWrap
            }
        }
    }
    footer: Item {
        implicitHeight: buttons.implicitHeight + 20 * editor.uiScale
        Flow {
            id: buttons
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: editor.leftPadding
            anchors.rightMargin: editor.rightPadding
            anchors.topMargin: 4 * editor.uiScale
            layoutDirection: Qt.RightToLeft
            spacing: 5 * editor.uiScale
            PaperButton {
                property bool remoteClickable: true
                uiScale: editor.uiScale
                text: editor.confirmingDiscard ? qsTr("Continua a scrivere") : qsTr("Salva")
                filled: true
                enabled: editor.confirmingDiscard || !editor.creating || nameField.text.trim().length > 0
                onClicked: editor.confirmingDiscard ? editor.confirmingDiscard = false : editor.save()
            }
            PaperButton {
                property bool remoteClickable: !editor.confirmingDiscard
                uiScale: editor.uiScale
                text: editor.confirmingDiscard ? qsTr("Scarta modifiche") : qsTr("Annulla")
                onClicked: editor.confirmingDiscard ? editor.discard() : editor.requestDiscard(false)
            }
        }
    }
}
