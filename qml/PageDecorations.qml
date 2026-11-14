pragma ComponentBehavior: Bound

import QtQuick
import Quaderno

Item {
    id: decorations

    required property var items
    required property real uiScale
    required property Notebook notebook
    property bool editingItems: false
    property string selectedId: ""

    signal editRequested(var item)
    signal moveRequested(string id, real x, real y)

    // Match the layout of saved and exported pages in PageContent::paintItems.
    readonly property real pageScale: width / 600
    readonly property real textSize: Math.max(1, Math.round(16 * pageScale))
    readonly property real cellTextSize: Math.max(1, Math.round(14 * pageScale))
    readonly property real textPadding: 3 * pageScale
    readonly property real cellPadding: 4 * pageScale
    readonly property real lineWidth: 0.9 * pageScale
    readonly property color ink: "#202833"
    readonly property color grid: "#9ea9b6"
    readonly property color accent: "#155cc2"

    clip: true

    Repeater {
        model: decorations.items

        Item {
            id: decoration

            required property var modelData
            readonly property bool isTable: modelData.type === "table"
            readonly property bool isText: modelData.type === "text"
            readonly property bool isChart: modelData.type === "chart"
            readonly property bool isDiagram: modelData.type === "diagram"
            readonly property bool isImage: modelData.type === "image"
            readonly property int rows: isTable ? modelData.rows : 0
            readonly property int cols: isTable ? modelData.cols : 0
            readonly property var cellValues: isTable ? decorations.notebook.tableCellValues(modelData) : []
            readonly property bool selected: decorations.editingItems
                && modelData.id === decorations.selectedId
            readonly property real cellWidth: cols > 0 ? width / cols : 0
            readonly property real cellHeight: rows > 0 ? height / rows : 0

            property bool dragging: false
            property point pressPosition
            property point previewPosition

            visible: isText || isChart || isDiagram || isImage || (isTable && rows > 0 && cols > 0)
            x: (dragging ? previewPosition.x : modelData.x) * decorations.width
            y: (dragging ? previewPosition.y : modelData.y) * decorations.height
            width: modelData.width * decorations.width
            height: modelData.height * decorations.height
            z: dragging || selected ? 1 : 0
            clip: true

            function updateDrag(pagePoint: point) {
                const dx = pagePoint.x - pressPosition.x
                const dy = pagePoint.y - pressPosition.y
                if (!dragging && Math.hypot(dx, dy) < 7 * decorations.uiScale)
                    return

                previewPosition = Qt.point(
                    Math.max(0, Math.min(1 - modelData.width, modelData.x + dx / decorations.width)),
                    Math.max(0, Math.min(1 - modelData.height, modelData.y + dy / decorations.height)))
                dragging = true
            }

            Text {
                visible: decoration.isText && width > 0 && height > 0
                anchors.fill: parent
                anchors.margins: decorations.textPadding
                text: decoration.isText ? decoration.modelData.text : ""
                color: decorations.ink
                font.pixelSize: decorations.textSize
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                clip: true
            }

            Loader {
                active: decoration.isChart
                anchors.fill: parent
                sourceComponent: ChartItem { chart: decoration.modelData }
            }

            Loader {
                active: decoration.isDiagram
                anchors.fill: parent
                sourceComponent: DiagramItem { diagram: decoration.modelData }
            }

            Image {
                visible: decoration.isImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                cache: false
                source: decoration.isImage ? "data:image/" + decoration.modelData.imageFormat
                    + ";base64," + decoration.modelData.imageData : ""
            }

            Rectangle {
                visible: decoration.isTable
                anchors.fill: parent
                color: "white"
                border.color: decorations.grid
                border.width: decorations.lineWidth
            }

            Repeater {
                model: decoration.isTable ? decoration.cols - 1 : 0

                Rectangle {
                    required property int index
                    x: (index + 1) * decoration.cellWidth - width / 2
                    width: decorations.lineWidth
                    height: decoration.height
                    color: decorations.grid
                }
            }

            Repeater {
                model: decoration.isTable ? decoration.rows - 1 : 0

                Rectangle {
                    required property int index
                    y: (index + 1) * decoration.cellHeight - height / 2
                    width: decoration.width
                    height: decorations.lineWidth
                    color: decorations.grid
                }
            }

            Repeater {
                model: decoration.rows * decoration.cols

                Text {
                    required property int index
                    readonly property real inset: decorations.cellPadding

                    x: (index % decoration.cols) * decoration.cellWidth + inset
                    y: Math.floor(index / decoration.cols) * decoration.cellHeight + inset
                    width: decoration.cellWidth - 2 * inset
                    height: decoration.cellHeight - 2 * inset
                    visible: width > 0 && height > 0
                    text: decoration.cellValues[index] || ""
                    color: decorations.ink
                    font.pixelSize: decorations.cellTextSize
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    clip: true
                }
            }

            Rectangle {
                visible: decorations.editingItems
                anchors.fill: parent
                color: "transparent"
                border.color: decoration.selected || decoration.dragging || pointer.activeFocus
                    ? decorations.accent : decorations.grid
                border.width: decoration.selected || decoration.dragging || pointer.activeFocus
                    ? Math.max(2, decorations.lineWidth) : decorations.lineWidth
            }

            MouseArea {
                id: pointer

                anchors.fill: parent
                enabled: decorations.editingItems
                activeFocusOnTab: enabled
                preventStealing: true
                cursorShape: decoration.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                Accessible.ignored: !enabled
                Accessible.role: Accessible.Button
                Accessible.name: decoration.isImage ? qsTr("Proprietà immagine") : decoration.isChart ? qsTr("Modifica grafico") : decoration.isTable ? qsTr("Modifica tabella") : qsTr("Modifica testo")
                Accessible.onPressAction: decorations.editRequested(decoration.modelData)
                Keys.onReturnPressed: decorations.editRequested(decoration.modelData)
                Keys.onSpacePressed: decorations.editRequested(decoration.modelData)

                onPressed: mouse => {
                    decoration.pressPosition = mapToItem(decorations, mouse.x, mouse.y)
                    decoration.dragging = false
                }
                onPositionChanged: mouse => {
                    if (pressed)
                        decoration.updateDrag(mapToItem(decorations, mouse.x, mouse.y))
                }
                onReleased: mouse => {
                    decoration.updateDrag(mapToItem(decorations, mouse.x, mouse.y))
                    if (!decoration.dragging) {
                        decorations.editRequested(decoration.modelData)
                        return
                    }
                    const destination = decoration.previewPosition
                    decoration.dragging = false
                    if (destination.x !== decoration.modelData.x || destination.y !== decoration.modelData.y)
                        decorations.moveRequested(decoration.modelData.id, destination.x, destination.y)
                }
                onCanceled: decoration.dragging = false
            }
        }
    }
}
