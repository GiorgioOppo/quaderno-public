import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: dialog

    required property real uiScale
    required property string currentTemplate
    property string errorText: ""
    signal templateChosen(string paper)

    anchors.centerIn: Overlay.overlay
    width: Math.min(476 * uiScale, (parent ? parent.width : 476 * uiScale) - 24 * uiScale)
    modal: true
    title: qsTr("Modello di pagina")
    padding: 16 * uiScale
    onAboutToShow: errorText = ""

    background: Rectangle {
        color: "#ffffff"
        border.color: "#d9dce2"
        radius: 16 * dialog.uiScale
    }

    header: Label {
        text: dialog.title
        font.pixelSize: 22 * dialog.uiScale
        font.weight: Font.DemiBold
        color: "#24262c"
        leftPadding: dialog.leftPadding
        rightPadding: dialog.rightPadding
        topPadding: dialog.topPadding
        bottomPadding: 8 * dialog.uiScale
    }

    contentItem: ColumnLayout {
        spacing: 12 * dialog.uiScale

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            rowSpacing: 8 * dialog.uiScale
            columnSpacing: 8 * dialog.uiScale

            Repeater {
                model: [
                    { paper: "blank", label: qsTr("Bianco") },
                    { paper: "ruled", label: qsTr("Righe") },
                    { paper: "grid", label: qsTr("Quadretti") },
                    { paper: "dotted", label: qsTr("Puntini") },
                    { paper: "planner", label: qsTr("Planner") }
                ]

                ItemDelegate {
                    id: card
                    required property var modelData
                    readonly property bool selected: dialog.currentTemplate === modelData.paper

                    text: modelData.label
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 1
                    implicitHeight: 117 * dialog.uiScale
                    padding: 6 * dialog.uiScale
                    onClicked: dialog.templateChosen(modelData.paper)

                    background: Rectangle {
                        color: card.selected || card.down ? "#edf3ff" : "#f4f5f7"
                        border.color: card.selected || card.activeFocus ? "#155cc2" : "#d9dce2"
                        border.width: card.selected || card.activeFocus ? 2 : 1
                        radius: 10 * dialog.uiScale
                    }

                    contentItem: ColumnLayout {
                        spacing: 4 * dialog.uiScale

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredHeight: 74 * dialog.uiScale
                            Layout.preferredWidth: Layout.preferredHeight * 0.75
                            color: "white"
                            border.color: "#d9dce2"

                            PageTemplateItem {
                                anchors.fill: parent
                                anchors.margins: 1
                                paper: card.modelData.paper
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                            text: card.text
                            color: card.selected ? "#155cc2" : "#24262c"
                            font.weight: Font.DemiBold
                            font.pixelSize: 13 * dialog.uiScale
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        Label {
            visible: dialog.errorText.length > 0
            Layout.fillWidth: true
            text: dialog.errorText
            color: "#87332c"
            wrapMode: Text.WordWrap
        }

        PaperButton {
            Layout.alignment: Qt.AlignRight
            uiScale: dialog.uiScale
            text: qsTr("Chiudi")
            quiet: true
            onClicked: dialog.close()
        }
    }
}
