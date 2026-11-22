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
    signal editCellRequested(var item, int row, int column)
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
            required property int index
            readonly property bool isTable: modelData.type === "table"
            readonly property bool isText: modelData.type === "text"
            readonly property bool isChart: modelData.type === "chart"
            readonly property bool isDiagram: modelData.type === "diagram"
            readonly property bool isImage: modelData.type === "image"
            readonly property bool isNote: modelData.type === "note"
            readonly property int noteNumber: isNote ? decorations.items.slice(0, index + 1).filter(item => item.type === "note").length : 0
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

            visible: isText || isChart || isDiagram || isImage || isNote || (isTable && rows > 0 && cols > 0)
            x: (dragging ? previewPosition.x : modelData.x) * decorations.width
            y: (dragging ? previewPosition.y : modelData.y) * decorations.height
            width: modelData.width * decorations.width
            height: modelData.height * decorations.height
            rotation: modelData.rotation || 0
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

            function editAt(position: point) {
                if (isTable && cellWidth > 0 && cellHeight > 0) {
                    const row = Math.max(0, Math.min(rows - 1, Math.floor(position.y / cellHeight)))
                    const column = Math.max(0, Math.min(cols - 1, Math.floor(position.x / cellWidth)))
                    decorations.editCellRequested(modelData, row, column)
                } else {
                    decorations.editRequested(modelData)
                }
            }

            Text {
                visible: decoration.isText && width > 0 && height > 0
                anchors.fill: parent
                anchors.margins: decorations.textPadding
                text: decoration.isText ? decoration.modelData.text : ""
                color: decorations.ink
                font.pixelSize: decorations.textSize * (decoration.modelData.textScale || 1)
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                clip: true
            }

            Rectangle {
                visible: decoration.isNote
                anchors.fill: parent
                color: ({yellow: "#fff2a6", blue: "#d8eaff", green: "#dff0d5", pink: "#f7dce8"})[decoration.modelData.noteColor || "yellow"] || "#fff2a6"
                border.color: "#6a6041"
                border.width: Math.max(1, decorations.pageScale)
                Text {
                    anchors.centerIn: parent
                    text: decoration.noteNumber
                    color: "#242723"
                    font.pixelSize: Math.max(1, Math.round(Math.min(parent.width, parent.height) * .48))
                    font.weight: Font.DemiBold
                }
                Rectangle {
                    width: Math.min(parent.width, parent.height) * .22
                    height: width
                    anchors.top: parent.top; anchors.right: parent.right
                    color: "#6a6041"; opacity: .25
                }
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
                    font.pixelSize: decorations.cellTextSize * (decoration.modelData.textScale || 1)
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
                Accessible.name: decoration.isNote ? qsTr("Apri post-it %1").arg(decoration.noteNumber) : decoration.isImage ? qsTr("Proprietà immagine") : decoration.isChart ? qsTr("Modifica grafico") : decoration.isDiagram ? qsTr("Modifica diagramma") : decoration.isTable ? qsTr("Modifica tabella") : qsTr("Modifica testo")
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
                        decoration.editAt(Qt.point(mouse.x, mouse.y))
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
