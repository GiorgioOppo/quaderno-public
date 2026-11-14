import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Control {
    id: handle

    required property MacDocumentExchange exchange
    required property bool currentPage
    required property real uiScale
    required property string text

    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    padding: 9 * uiScale

    background: Rectangle {
        color: mouse.containsMouse ? "#e7effe" : "#f6f7f9"
        border.color: "#dedfe4"
        radius: 8 * handle.uiScale
    }

    contentItem: RowLayout {
        spacing: 6 * handle.uiScale

        PaperIcon {
            name: "paper"
            tint: "#155cc2"
            Layout.preferredWidth: 16 * handle.uiScale
            Layout.preferredHeight: Layout.preferredWidth
            Accessible.ignored: true
        }

        Label {
            Layout.fillWidth: true
            text: handle.text
            textFormat: Text.PlainText
            color: "#155cc2"
            font.pixelSize: 14 * handle.uiScale
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
        enabled: handle.enabled

        property point startPosition
        property bool started: false

        onPressed: event => {
            startPosition = Qt.point(event.x, event.y)
            started = false
        }

        onPositionChanged: event => {
            if (!pressed || started)
                return
            if (Math.hypot(event.x - startPosition.x, event.y - startPosition.y) < Qt.styleHints.startDragDistance)
                return
            started = true
            handle.exchange.beginPdfDrag(handle.currentPage)
        }
    }
}
