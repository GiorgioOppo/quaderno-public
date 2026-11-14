import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

DropArea {
    id: area

    required property MacDocumentExchange exchange
    required property string folderId
    required property real uiScale

    visible: exchange.available
    keys: ["text/uri-list"]

    onEntered: drag => {
        if (drag.hasUrls && exchange.hasSupportedFiles(drag.urls))
            drag.accept(Qt.CopyAction)
        else
            drag.accepted = false
    }

    onDropped: drop => {
        if (drop.hasUrls && exchange.importFiles(drop.urls, folderId))
            drop.accept(Qt.CopyAction)
        else
            drop.accepted = false
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
                text: qsTr("Rilascia per importare PDF, EPUB o Markdown")
                textFormat: Text.PlainText
                color: "#133456"
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                font.pixelSize: 17 * area.uiScale
                font.weight: Font.DemiBold
            }
        }
    }
}
