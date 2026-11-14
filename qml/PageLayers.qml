pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: layerDialog

    required property Notebook notebook
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    readonly property real hairline: Math.max(1, Math.round(uiScale))
    readonly property color ink: "#24262c"
    readonly property color muted: "#555b67"
    readonly property color blue: "#155cc2"
    readonly property color border: "#d9dce2"
    property string selectedId: ""
    property string action: ""
    property string errorText: ""
    readonly property int selectedIndex: notebook.layers.findIndex(layer => layer.id === selectedId)
    readonly property var selectedLayer: selectedIndex < 0 ? null : notebook.layers[selectedIndex]
    readonly property bool naming: action === "add" || action === "rename"
    readonly property bool confirmingDelete: action === "delete"
    readonly property bool managing: action === ""

    parent: Overlay.overlay
    anchors.centerIn: parent
    title: qsTr("Livelli")
    width: Math.min(490 * uiScale, (parent ? parent.width : 490 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    spacing: 9 * uiScale
    modal: true
    dim: !tabletMode
    focus: true
    closePolicy: Popup.CloseOnEscape

    onAboutToShow: {
        selectedId = notebook.activeLayer
        action = ""
        errorText = ""
    }

    function begin(kind: string) {
        errorText = ""
        nameInput.text = kind === "rename" && selectedLayer ? selectedLayer.name : ""
        action = kind
        if (naming)
            Qt.callLater(focusName)
    }

    function focusName() {
        if (!visible || !naming)
            return
        nameInput.forceActiveFocus()
        nameInput.selectAll()
    }

    function finish(saved: bool) {
        if (saved) {
            action = ""
            errorText = ""
            if (selectedIndex < 0)
                selectedId = notebook.activeLayer
        } else {
            errorText = notebook.error
        }
    }

    function saveName() {
        const name = nameInput.text.trim()
        if (name.length === 0)
            return
        if (action === "add") {
            const saved = notebook.addLayer(name)
            if (saved)
                selectedId = notebook.activeLayer
            finish(saved)
        } else if (selectedLayer) {
            finish(notebook.renameLayer(selectedId, name))
        }
    }

    component LayerButton: PaperButton {
        uiScale: layerDialog.uiScale
        Layout.minimumWidth: 0
    }

    component LayerLabel: Label {
        textFormat: Text.PlainText
        color: layerDialog.ink
        font.pixelSize: 14 * layerDialog.uiScale
    }

    background: Rectangle {
        color: "white"
        border.color: layerDialog.border
        border.width: layerDialog.hairline
        radius: 14 * layerDialog.uiScale
    }

    header: RowLayout {
        spacing: 6 * layerDialog.uiScale

        LayerLabel {
            Layout.fillWidth: true
            Layout.leftMargin: layerDialog.leftPadding
            Layout.topMargin: layerDialog.topPadding
            text: layerDialog.action === "add" ? qsTr("Nuovo livello")
                : layerDialog.action === "rename" ? qsTr("Rinomina livello") : layerDialog.title
            font.pixelSize: 21 * layerDialog.uiScale
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        LayerButton {
            Layout.topMargin: 8 * layerDialog.uiScale
            Layout.rightMargin: layerDialog.rightPadding
            text: qsTr("Chiudi")
            iconName: "close"
            iconOnly: true
            quiet: true
            onClicked: layerDialog.close()
        }
    }

    contentItem: ScrollView {
        id: scroll

        clip: true
        contentWidth: availableWidth
        contentHeight: form.implicitHeight
        implicitHeight: Math.min(form.implicitHeight, Math.max(120 * layerDialog.uiScale,
                            (layerDialog.parent ? layerDialog.parent.height : 500) - 148 * layerDialog.uiScale))
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: form

            width: scroll.availableWidth
            spacing: 7 * layerDialog.uiScale

            Repeater {
                model: layerDialog.managing ? Array.from(layerDialog.notebook.layers).reverse() : []

                RowLayout {
                    id: layerRow

                    required property var modelData
                    readonly property bool active: modelData.id === layerDialog.notebook.activeLayer
                    readonly property bool selected: modelData.id === layerDialog.selectedId
                    Layout.fillWidth: true
                    spacing: 5 * layerDialog.uiScale

                    LayerButton {
                        Layout.fillWidth: true
                        text: layerRow.modelData.name
                        Accessible.name: layerRow.active ? qsTr("%1, livello attivo").arg(text) : text
                        filled: layerRow.selected
                        alignLeft: true
                        iconName: layerRow.active ? "rename" : ""
                        onClicked: {
                            layerDialog.selectedId = layerRow.modelData.id
                            layerDialog.finish(layerDialog.notebook.setActiveLayer(layerRow.modelData.id))
                        }
                    }

                    LayerButton {
                        text: layerRow.modelData.visible ? qsTr("Nascondi") : qsTr("Mostra")
                        Accessible.name: qsTr("%1 %2").arg(text).arg(layerRow.modelData.name)
                        onClicked: {
                            layerDialog.selectedId = layerRow.modelData.id
                            layerDialog.finish(layerDialog.notebook.setLayerVisible(layerRow.modelData.id, !layerRow.modelData.visible))
                        }
                    }
                }
            }

            LayerLabel {
                visible: layerDialog.confirmingDelete && layerDialog.selectedLayer !== null
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: layerDialog.selectedLayer
                      ? qsTr("Eliminare «%1» e tutti i suoi tratti?").arg(layerDialog.selectedLayer.name) : ""
                color: "#87332c"
            }

            TextField {
                id: nameInput

                visible: layerDialog.naming
                Layout.fillWidth: true
                Layout.preferredHeight: 44 * layerDialog.uiScale
                placeholderText: qsTr("Nome livello")
                Accessible.name: placeholderText
                maximumLength: 50
                selectByMouse: true
                color: layerDialog.ink
                placeholderTextColor: layerDialog.muted
                font.pixelSize: 15 * layerDialog.uiScale
                leftPadding: 10 * layerDialog.uiScale
                background: Rectangle {
                    color: "white"
                    border.color: nameInput.activeFocus ? layerDialog.blue : layerDialog.border
                    border.width: layerDialog.hairline
                    radius: 8 * layerDialog.uiScale
                }
                onAccepted: layerDialog.saveName()
            }

            TouchKeyboard {
                visible: layerDialog.naming && layerDialog.showTouchKeyboard
                Layout.fillWidth: true
                target: nameInput
                uiScale: layerDialog.uiScale
            }

            LayerLabel {
                visible: layerDialog.errorText.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: layerDialog.errorText
                color: "#87332c"
            }
        }
    }

    footer: ColumnLayout {
        spacing: 5 * layerDialog.uiScale

        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: layerDialog.leftPadding
            Layout.rightMargin: layerDialog.rightPadding
            visible: layerDialog.managing && layerDialog.selectedIndex >= 0
            spacing: 5 * layerDialog.uiScale

            LayerButton {
                text: qsTr("Alza livello")
                iconName: "left"
                iconOnly: true
                rotation: 90
                enabled: layerDialog.selectedIndex < layerDialog.notebook.layers.length - 1
                onClicked: layerDialog.finish(layerDialog.notebook.moveLayer(layerDialog.selectedId, layerDialog.selectedIndex + 1))
            }

            LayerButton {
                text: qsTr("Abbassa livello")
                iconName: "left"
                iconOnly: true
                rotation: -90
                enabled: layerDialog.selectedIndex > 0
                onClicked: layerDialog.finish(layerDialog.notebook.moveLayer(layerDialog.selectedId, layerDialog.selectedIndex - 1))
            }

            LayerButton {
                text: qsTr("Rinomina")
                onClicked: layerDialog.begin("rename")
            }

            LayerButton {
                text: qsTr("Unisci sotto")
                enabled: layerDialog.selectedIndex > 0 && layerDialog.selectedLayer.visible
                         === layerDialog.notebook.layers[layerDialog.selectedIndex - 1].visible
                onClicked: {
                    const lower = layerDialog.notebook.layers[layerDialog.selectedIndex - 1].id
                    const saved = layerDialog.notebook.mergeLayerDown(layerDialog.selectedId)
                    if (saved)
                        layerDialog.selectedId = lower
                    layerDialog.finish(saved)
                }
            }

            LayerButton {
                text: qsTr("Elimina")
                enabled: layerDialog.notebook.layers.length > 1
                onClicked: layerDialog.begin("delete")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: layerDialog.leftPadding
            Layout.rightMargin: layerDialog.rightPadding
            Layout.bottomMargin: layerDialog.bottomPadding
            spacing: 5 * layerDialog.uiScale

            Item { Layout.fillWidth: true }

            LayerButton {
                visible: !layerDialog.managing
                text: qsTr("Annulla")
                quiet: true
                onClicked: layerDialog.begin("")
            }

            LayerButton {
                visible: layerDialog.managing
                text: qsTr("Nuovo livello")
                iconName: "plus"
                filled: true
                enabled: layerDialog.notebook.layers.length < 12
                onClicked: layerDialog.begin("add")
            }

            LayerButton {
                visible: !layerDialog.managing
                text: layerDialog.confirmingDelete ? qsTr("Elimina livello") : qsTr("Salva")
                filled: true
                enabled: layerDialog.confirmingDelete || nameInput.text.trim().length > 0
                onClicked: {
                    if (layerDialog.confirmingDelete)
                        layerDialog.finish(layerDialog.notebook.deleteLayer(layerDialog.selectedId))
                    else
                        layerDialog.saveName()
                }
            }
        }
    }
}
