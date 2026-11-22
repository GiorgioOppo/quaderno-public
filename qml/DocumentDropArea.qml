pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

DropArea {
    id: area

    required property MacDocumentExchange exchange
    required property string folderId
    required property real uiScale
    property string folderLabel: qsTr("Libreria")
    property var dropPreview: ({})
    readonly property bool dialogOpen: report.visible

    function closeDialogs() { report.close() }

    visible: exchange.available
    keys: ["text/uri-list"]

    onEntered: drag => {
        dropPreview = drag.hasUrls ? exchange.importPreview(drag.urls) : ({})
        if (dropPreview.allowed && dropPreview.accepted > 0)
            drag.accept(Qt.CopyAction)
        else
            drag.accepted = false
    }

    onDropped: drop => {
        if (drop.hasUrls && exchange.importFiles(drop.urls, folderId))
            drop.accept(Qt.CopyAction)
        else
            drop.accepted = false
        dropPreview = ({})
    }
    onExited: dropPreview = ({})

    PaperButton {
        property bool blocksInk: true
        objectName: "importReportButton"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12 * area.uiScale
        visible: area.enabled && area.exchange.importReport.length > 0 && !area.containsDrag && !report.visible
        uiScale: area.uiScale
        text: qsTr("Importazioni (%1)").arg(area.exchange.importReport.length)
        iconName: "download"
        onClicked: report.open()
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 8 * area.uiScale
        visible: area.enabled && area.containsDrag
        color: "#eaf0f4f9"
        border.color: "#0757ad"
        border.width: Math.max(2, area.uiScale)
        radius: 15 * area.uiScale

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.max(0, Math.min(parent.width - 32 * area.uiScale, implicitWidth))
            spacing: 10 * area.uiScale

            PaperIcon {
                name: "download"
                tint: "#0757ad"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 27 * area.uiScale
                Layout.preferredHeight: Layout.preferredWidth
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Importa %1 documenti in «%2»").arg(area.dropPreview.accepted || 0).arg(area.folderLabel)
                textFormat: Text.PlainText
                color: "#133456"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: 17 * area.uiScale
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("Duplicati: %1 · Non supportati: %2 · Non leggibili: %3")
                    .arg(area.dropPreview.duplicates || 0).arg(area.dropPreview.unsupported || 0).arg(area.dropPreview.unreadable || 0)
                textFormat: Text.PlainText
                color: "#133456"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: 14 * area.uiScale
            }
        }
    }

    Dialog {
        id: report
        objectName: "importReportDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(590 * area.uiScale, parent.width - 24 * area.uiScale)
        height: Math.min(540 * area.uiScale, parent.height - 24 * area.uiScale)
        title: qsTr("Rapporto importazione")
        modal: true
        focus: true
        standardButtons: Dialog.Close
        closePolicy: Popup.CloseOnEscape

        contentItem: ColumnLayout {
            spacing: 10 * area.uiScale
            Label {
                Layout.fillWidth: true
                text: area.exchange.status
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                font.pixelSize: 15 * area.uiScale
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6 * area.uiScale
                model: area.exchange.importReport
                ScrollBar.vertical: ScrollBar {}
                delegate: Frame {
                    id: importRow
                    required property var modelData
                    width: ListView.view.width
                    padding: 10 * area.uiScale
                    contentItem: ColumnLayout {
                        Label {
                            Layout.fillWidth: true
                            text: importRow.modelData.name
                            textFormat: Text.PlainText
                            wrapMode: Text.WrapAnywhere
                            font.weight: Font.DemiBold
                            font.pixelSize: 15 * area.uiScale
                        }
                        Label {
                            Layout.fillWidth: true
                            text: ({queued: qsTr("In attesa"), importing: qsTr("Importazione in corso"),
                                    imported: qsTr("Importato"), failed: qsTr("Non riuscito"),
                                    duplicate: qsTr("Duplicato ignorato"), unsupported: qsTr("Formato ignorato"),
                                    cancelled: qsTr("Annullato")})[importRow.modelData.state] || ""
                            font.pixelSize: 13 * area.uiScale
                            color: importRow.modelData.state === "failed" ? "#87332c" : "#0757ad"
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: importRow.modelData.message.length > 0
                            text: importRow.modelData.message
                            textFormat: Text.PlainText
                            wrapMode: Text.WordWrap
                            font.pixelSize: 13 * area.uiScale
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                PaperButton {
                    uiScale: area.uiScale
                    visible: area.exchange.importing
                    text: qsTr("Annulla importazione")
                    onClicked: area.exchange.cancelImports()
                }
                PaperButton {
                    uiScale: area.uiScale
                    visible: area.exchange.canRetryImports
                    text: qsTr("Riprova non riusciti")
                    onClicked: {
                        report.close()
                        Qt.callLater(function() { area.exchange.retryFailedImports() })
                    }
                }
                Item { Layout.fillWidth: true }
                PaperButton {
                    uiScale: area.uiScale
                    visible: !area.exchange.importing
                    text: qsTr("Cancella rapporto")
                    onClicked: { area.exchange.clearImportReport(); report.close() }
                }
            }
        }
    }
}
