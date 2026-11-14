pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: tagsDialog

    required property Notebook notebook
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    readonly property color ink: "#24262c"
    readonly property color muted: "#555b67"
    readonly property color blue: "#155cc2"
    readonly property color border: "#d9dce2"
    property var draft: []
    property string errorText: ""
    property bool showingSuggestions: false

    readonly property var suggestions: {
        const prefix = input.text.trim().replace(/\s+/g, " ").toLocaleLowerCase()
        return Array.from(notebook.knownTags).filter(tag => !hasTag(tag)
            && (prefix.length === 0 || tag.toLocaleLowerCase().includes(prefix)))
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    title: qsTr("Tag della pagina")
    width: Math.min(490 * uiScale, (parent ? parent.width : 490 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    spacing: 9 * uiScale
    modal: true
    dim: !tabletMode
    focus: true
    closePolicy: Popup.CloseOnEscape

    onAboutToShow: {
        draft = Array.from(notebook.pageTags)
        errorText = ""
        showingSuggestions = false
        input.clear()
        scroll.contentItem.contentY = 0
    }

    onOpened: input.forceActiveFocus()

    function hasTag(name: string): bool {
        const key = name.toLocaleLowerCase()
        return draft.some(tag => tag.toLocaleLowerCase() === key)
    }

    function addTag(value: string): bool {
        const name = value.trim().replace(/\s+/g, " ")
        if (name.length === 0)
            return true
        if (!hasTag(name)) {
            if (draft.length >= 20) {
                errorText = qsTr("Puoi assegnare fino a 20 tag a una pagina.")
                return false
            }
            draft = [...draft, name]
        }
        errorText = ""
        input.clear()
        return true
    }

    function save() {
        if (!addTag(input.text))
            return
        if (notebook.setPageTags(draft))
            close()
        else
            errorText = notebook.error
    }

    component TagButton: PaperButton {
        uiScale: tagsDialog.uiScale
    }

    component TagLabel: Label {
        textFormat: Text.PlainText
        color: tagsDialog.ink
        font.pixelSize: 14 * tagsDialog.uiScale
    }

    background: Rectangle {
        color: "white"
        border.color: tagsDialog.border
        radius: 14 * tagsDialog.uiScale
    }

    header: TagLabel {
        text: tagsDialog.title
        font.pixelSize: 21 * tagsDialog.uiScale
        font.weight: Font.DemiBold
        padding: tagsDialog.padding
        bottomPadding: 0
    }

    contentItem: ScrollView {
        id: scroll

        clip: true
        contentWidth: availableWidth
        contentHeight: form.implicitHeight
        implicitHeight: Math.min(form.implicitHeight,
                                  Math.max(110 * tagsDialog.uiScale, tagsDialog.parent.height - 135 * tagsDialog.uiScale))
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: form

            width: scroll.availableWidth
            spacing: 7 * tagsDialog.uiScale

            Flow {
                Layout.fillWidth: true
                visible: tagsDialog.draft.length > 0
                spacing: 5 * tagsDialog.uiScale

                Repeater {
                    model: tagsDialog.draft

                    TagButton {
                        required property int index
                        required property string modelData

                        width: Math.min(implicitWidth, form.width)
                        text: qsTr("%1 ×").arg(modelData)
                        Accessible.name: qsTr("Rimuovi tag %1").arg(modelData)
                        onClicked: {
                            tagsDialog.draft = tagsDialog.draft.filter((tag, position) => position !== index)
                            tagsDialog.errorText = ""
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6 * tagsDialog.uiScale

                TextField {
                    id: input

                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: 44 * tagsDialog.uiScale
                    placeholderText: qsTr("Nuovo tag")
                    Accessible.name: placeholderText
                    maximumLength: 40
                    selectByMouse: true
                    color: tagsDialog.ink
                    placeholderTextColor: tagsDialog.muted
                    font.pixelSize: 15 * tagsDialog.uiScale
                    leftPadding: 10 * tagsDialog.uiScale
                    background: Rectangle {
                        color: "white"
                        border.color: input.activeFocus ? tagsDialog.blue : tagsDialog.border
                        border.width: Math.max(1, Math.round(tagsDialog.uiScale))
                        radius: 8 * tagsDialog.uiScale
                    }
                    onAccepted: tagsDialog.addTag(text)
                }

                TagButton {
                    text: qsTr("Aggiungi")
                    enabled: input.text.trim().length > 0
                    onClicked: tagsDialog.addTag(input.text)
                }
            }

            TouchKeyboard {
                Layout.fillWidth: true
                visible: tagsDialog.showTouchKeyboard
                target: input
                uiScale: tagsDialog.uiScale
            }

            TagButton {
                visible: tagsDialog.suggestions.length > 0
                text: tagsDialog.showingSuggestions ? qsTr("Nascondi tag esistenti") : qsTr("Mostra tag esistenti")
                quiet: true
                onClicked: tagsDialog.showingSuggestions = !tagsDialog.showingSuggestions
            }

            ListView {
                id: suggestedTags

                visible: tagsDialog.showingSuggestions && count > 0
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(145, count * 48) * tagsDialog.uiScale
                boundsBehavior: Flickable.StopAtBounds
                spacing: 4 * tagsDialog.uiScale
                clip: true
                model: tagsDialog.showingSuggestions ? tagsDialog.suggestions : []
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: TagButton {
                    required property string modelData

                    width: suggestedTags.width - 6 * tagsDialog.uiScale
                    text: modelData
                    alignLeft: true
                    onClicked: tagsDialog.addTag(modelData)
                }
            }

            TagLabel {
                visible: tagsDialog.errorText.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: tagsDialog.errorText
                color: "#87332c"
            }
        }
    }

    footer: RowLayout {
        spacing: 6 * tagsDialog.uiScale

        Item { Layout.fillWidth: true }

        TagButton {
            Layout.bottomMargin: tagsDialog.bottomPadding
            text: qsTr("Annulla")
            quiet: true
            onClicked: tagsDialog.close()
        }

        TagButton {
            Layout.rightMargin: tagsDialog.rightPadding
            Layout.bottomMargin: tagsDialog.bottomPadding
            text: qsTr("Salva")
            filled: true
            onClicked: tagsDialog.save()
        }
    }
}
