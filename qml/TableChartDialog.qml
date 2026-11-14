import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: editor

    required property Notebook notebook
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    signal deleteRequested(string id)
    property var source: ({})
    property var original: ({})
    property string errorText: ""
    property bool confirmingDelete: false
    property int revision: 0
    readonly property var options: ({
        id: original.id || "", sourceTable: source.id || "", chartKind: kind.currentIndex === 1 ? "bars" : "line",
        title: titleField.text.trim(), hasHeader: headerToggle.checked, xColumn: axisX.currentIndex + 1,
        yColumn: axisY.currentIndex + 1
    })
    readonly property var preview: visible && revision >= 0 ? notebook.tableChartPreview(options) : ({})

    title: qsTr("Grafico da tabella")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(490 * uiScale, parent.width - 24 * uiScale)
    padding: 14 * uiScale
    modal: true
    dim: !tabletMode

    function openFor(table, item) {
        source = table || ({})
        original = item || ({})
        errorText = ""
        confirmingDelete = false
        titleField.text = original.title || ""
        headerToggle.checked = original.hasHeader === undefined ? true : original.hasHeader
        kind.currentIndex = original.chartKind === "bars" ? 1 : 0
        axisX.currentIndex = (original.xColumn || 1) - 1
        axisY.currentIndex = (original.yColumn || 2) - 1
        revision++
        open()
    }

    function save() {
        if (notebook.saveTableChart(options))
            close()
        else
            errorText = notebook.error
    }

    contentItem: ScrollView {
        id: scroll
        clip: true
        implicitHeight: Math.min(form.implicitHeight, editor.parent.height - 140 * editor.uiScale)
        contentWidth: availableWidth
        contentHeight: form.implicitHeight
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: form
            width: scroll.availableWidth
            spacing: 8 * editor.uiScale

            ChartItem {
                Layout.fillWidth: true
                Layout.preferredHeight: 148 * editor.uiScale
                visible: !!editor.preview.points
                chart: editor.preview
            }

            TextField {
                id: titleField
                Layout.fillWidth: true
                placeholderText: qsTr("Titolo del grafico")
                maximumLength: 80
            }

            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Tipo") }
                ComboBox {
                    id: kind
                    Layout.fillWidth: true
                    model: [qsTr("Linee"), qsTr("Barre")]
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Label { text: qsTr("Colonna X") }
                ComboBox {
                    id: axisX
                    Layout.fillWidth: true
                    model: editor.source.cols || 0
                    delegate: ItemDelegate { required property int index; text: String(index + 1); width: axisX.width }
                    displayText: currentIndex < 0 ? "" : String(currentIndex + 1)
                }
                Label { text: qsTr("Colonna Y") }
                ComboBox {
                    id: axisY
                    Layout.fillWidth: true
                    model: axisX.model
                    delegate: ItemDelegate { required property int index; text: String(index + 1); width: axisY.width }
                    displayText: currentIndex < 0 ? "" : String(currentIndex + 1)
                }
            }

            CheckBox { id: headerToggle; text: qsTr("Prima riga come intestazione") }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: editor.errorText || (editor.preview.points ? qsTr("Il grafico si aggiorna quando modifichi la tabella.")
                                                          : qsTr("Scegli due colonne con valori numerici validi."))
            }

            TouchKeyboard {
                Layout.fillWidth: true
                visible: editor.showTouchKeyboard && titleField.activeFocus
                uiScale: editor.uiScale
                target: titleField
            }
        }
    }

    footer: RowLayout {
        spacing: 4 * editor.uiScale

        PaperButton {
            uiScale: editor.uiScale
            visible: !!editor.original.id
            text: editor.confirmingDelete ? qsTr("Conferma") : qsTr("Elimina")
            onClicked: {
                if (editor.confirmingDelete)
                    editor.deleteRequested(editor.original.id)
                else
                    editor.confirmingDelete = true
            }
        }
        Item { Layout.fillWidth: true }
        PaperButton { uiScale: editor.uiScale; text: qsTr("Annulla"); onClicked: editor.close() }
        PaperButton {
            uiScale: editor.uiScale
            text: qsTr("Salva")
            filled: true
            enabled: !!editor.preview.points && !editor.confirmingDelete
            onClicked: editor.save()
        }
    }
}
