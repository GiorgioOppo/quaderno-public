pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno
import "DiagramArrangement.js" as Arrangement

Dialog {
    id: editor

    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    property bool draftOnly: false
    readonly property Item markerTarget: visible && !labelDialog.visible && !deleteDialog.visible
        && !arrangeDialog.visible && !overflow.visible ? canvas : null
    property string errorText: ""
    property string importNotice: ""
    property string importTitle: ""
    property var original: null
    property var nodes: []
    property var edges: []
    property var history: []
    property var future: []
    property var selectedNodes: []
    property string selectedEdge: ""
    property string connectFrom: ""
    property bool connecting: false
    property bool gridEnabled: false
    property bool lassoEnabled: false
    property bool panEnabled: false
    property real zoom: 1
    property point viewOffset: Qt.point(0, 0)
    property var lassoPoints: []
    property var pointerStart: null
    property var previewNodes: null
    property var draggedShape: null
    property point shapeDrop: Qt.point(-1, -1)
    property string connectorSource: ""
    property point connectorPoint: Qt.point(0, 0)
    property int nextId: 0

    signal saved(var item)
    signal saveOnNewPageRequested(var item)
    signal deleteRequested(string id)
    signal exportRequested(var item)
    signal importRequested()

    title: importTitle.length ? qsTr("Diagramma · %1").arg(importTitle) : qsTr("Diagramma")
    anchors.centerIn: Overlay.overlay
    width: Math.min(1140 * uiScale, parent ? parent.width - 16 * uiScale : 1140 * uiScale)
    height: Math.min(880 * uiScale, parent ? parent.height - 16 * uiScale : 880 * uiScale)
    padding: 12 * uiScale
    modal: true
    dim: !tabletMode
    closePolicy: Popup.CloseOnEscape

    component Surface: Rectangle {
        color: "white"
        border.color: "#d9dce2"
        radius: 10 * editor.uiScale
    }

    background: Surface { }
    Overlay.modal: Rectangle { color: "#73000000" }

    header: Label {
        text: editor.title
        textFormat: Text.PlainText
        elide: Text.ElideRight
        padding: editor.padding
        font.weight: Font.DemiBold
        font.pixelSize: 16 * editor.uiScale
        color: "#24262c"
    }

    readonly property var palette: [
        { shape: "terminal", title: qsTr("Inizio / fine") },
        { shape: "process", title: qsTr("Processo") },
        { shape: "decision", title: qsTr("Decisione") },
        { shape: "data", title: qsTr("Dati") },
        { shape: "note", title: qsTr("Nota") }
    ]
    readonly property string selectedNode: selectedNodes.length === 1 ? selectedNodes[0] : ""
    readonly property bool hasSelection: selectedNodes.length > 0 || selectedEdge.length > 0
    readonly property var activeNode: nodes.find(node => node.id === selectedNode) || null
    readonly property var activeEdge: edges.find(edge => edge.id === selectedEdge) || null
    readonly property var shownNodes: previewNodes || nodes
    readonly property var resizeNode: !connecting && !lassoEnabled && !panEnabled && activeNode
        ? shownNodes.find(node => node.id === selectedNode) || null : null
    readonly property real gridStep: 0.05
    readonly property bool sidePanels: availableWidth >= 840 * uiScale && availableHeight >= 360 * uiScale
    readonly property bool compactActions: availableWidth < 365 * uiScale
    readonly property rect view: Qt.rect(
        Math.max(canvas.width * (1 - zoom), Math.min(0, viewOffset.x)),
        Math.max(canvas.height * (1 - zoom), Math.min(0, viewOffset.y)),
        canvas.width * zoom, canvas.height * zoom)

    function openFor(item) {
        original = item
        nodes = item ? item.nodes.slice() : []
        edges = item ? item.edges.slice() : []
        history = []
        future = []
        nextId = 0
        selectedNodes = []
        selectedEdge = ""
        connecting = false
        lassoEnabled = false
        fitView()
        connectFrom = ""
        errorText = ""
        importNotice = ""
        importTitle = ""
        open()
    }

    onClosed: {
        labelDialog.close()
        deleteDialog.close()
        arrangeDialog.close()
        overflow.close()
        markerCanceled()
        draggedShape = null
        connectorSource = ""
        original = null
        nodes = []
        edges = []
        history = []
        future = []
        selectedNodes = []
        selectedEdge = ""
        connectFrom = ""
        errorText = ""
        importNotice = ""
        importTitle = ""
    }

    function payload() {
        const result = { type: "diagram", nodes: nodes, edges: edges }
        if (original) {
            result.id = original.id
            result.x = original.x
            result.y = original.y
            result.width = original.width
            result.height = original.height
        }
        return result
    }

    function remember() {
        history = history.slice(-23).concat([snapshot()])
        future = []
        errorText = ""
    }

    function snapshot() {
        return { nodes: nodes, edges: edges, notice: importNotice, title: importTitle,
            selection: selectedNodes, edge: selectedEdge }
    }

    function restore(state) {
        markerCanceled()
        nodes = state.nodes
        edges = state.edges
        importNotice = state.notice
        importTitle = state.title
        selectedNodes = state.selection || []
        selectedEdge = state.edge || ""
        connecting = false
        lassoEnabled = false
        connectFrom = ""
        errorText = ""
    }

    function undo() {
        if (!history.length)
            return
        const last = history[history.length - 1]
        future = future.concat([snapshot()])
        history = history.slice(0, -1)
        restore(last)
    }

    function redo() {
        if (!future.length)
            return
        const next = future[future.length - 1]
        history = history.slice(-23).concat([snapshot()])
        future = future.slice(0, -1)
        restore(next)
    }

    function importDiagram(result) {
        if (result.error) {
            errorText = result.error
            return
        }
        if (nodes.length || !result.diagram)
            return
        remember()
        restore({ nodes: result.diagram.nodes.slice(), edges: result.diagram.edges.slice(),
            notice: result.warnings || "", title: result.title || "" })
    }

    function flowTemplate() {
        if (nodes.length)
            return
        const start = { id: idFor("n", []), shape: "terminal", text: qsTr("Inizio"), x: 0.37, y: 0.06, width: 0.26, height: 0.12 }
        const action = { id: idFor("n", []), shape: "process", text: qsTr("Processo"), x: 0.37, y: 0.29, width: 0.26, height: 0.14 }
        const choice = { id: idFor("n", []), shape: "decision", text: qsTr("Decisione"), x: 0.35, y: 0.54, width: 0.30, height: 0.18 }
        const done = { id: idFor("n", []), shape: "terminal", text: qsTr("Fine"), x: 0.37, y: 0.84, width: 0.26, height: 0.12 }
        remember()
        nodes = [start, action, choice, done]
        edges = [
            { id: idFor("e", []), from: start.id, to: action.id, text: "" },
            { id: idFor("e", []), from: action.id, to: choice.id, text: "" },
            { id: idFor("e", []), from: choice.id, to: done.id, text: qsTr("Sì") },
            { id: idFor("e", []), from: choice.id, to: action.id, text: qsTr("No"), route: "orthogonal" }
        ]
        selectedNodes = [start.id]
    }

    function snap(value) {
        return gridEnabled ? Math.round(value / gridStep) * gridStep : value
    }

    function duplicateShape() {
        if (!activeNode || nodes.length >= 64)
            return
        const node = activeNode
        const offset = gridEnabled ? gridStep : 0.04
        const x = Math.max(0, Math.min(1 - node.width, snap(node.x + (node.x + node.width + offset <= 1 ? offset : -offset))))
        const y = Math.max(0, Math.min(1 - node.height, snap(node.y + (node.y + node.height + offset <= 1 ? offset : -offset))))
        remember()
        const copy = Object.assign({}, node, { id: idFor("n", nodes), x: x, y: y })
        nodes = nodes.concat([copy])
        selectedNodes = [copy.id]
    }

    function updateEdge(updated) {
        edges = edges.map(edge => edge.id === updated.id ? updated : edge)
    }

    function reverseEdge() {
        const edge = activeEdge
        if (!edge || edges.some(other => other.from === edge.to && other.to === edge.from))
            return
        remember()
        updateEdge(Object.assign({}, edge, { from: edge.to, to: edge.from }))
    }

    function toggleEdgeRoute() {
        if (!activeEdge)
            return
        const updated = Object.assign({}, activeEdge)
        if (updated.route === "orthogonal")
            delete updated.route
        else
            updated.route = "orthogonal"
        remember()
        updateEdge(updated)
    }

    function idFor(prefix, entries) {
        let id
        do {
            id = prefix + (++nextId)
        } while (entries.some(entry => entry.id === id))
        return id
    }

    function shapeSize(kind) {
        return kind.shape === "decision" ? Qt.size(0.29, 0.18) : Qt.size(0.25, 0.13)
    }

    function positionShape(kind, point) {
        const size = shapeSize(kind)
        return Qt.point(
            Math.max(0, Math.min(1 - size.width, snap((point.x - view.x) / view.width - size.width / 2))),
            Math.max(0, Math.min(1 - size.height, snap((point.y - view.y) / view.height - size.height / 2))))
    }

    function insideCanvas(point) {
        return point.x >= 0 && point.y >= 0 && point.x < canvas.width && point.y < canvas.height
    }

    function addShape(kind, point) {
        if (nodes.length >= 64)
            return
        const size = shapeSize(kind)
        const shapeWidth = size.width
        const shapeHeight = size.height
        let position = point && insideCanvas(point) ? positionShape(kind, point) : null
        for (let row = 0; row < 5 && !position; ++row) {
            for (let col = 0; col < 3 && !position; ++col) {
                const x = 0.04 + col * 0.32
                const y = 0.05 + row * 0.18
                const gap = 0.01
                if (!nodes.some(node => x < node.x + node.width + gap && x + shapeWidth + gap > node.x
                        && y < node.y + node.height + gap && y + shapeHeight + gap > node.y))
                    position = Qt.point(x, y)
            }
        }
        if (!position)
            position = Qt.point(0.36 + (nodes.length % 5) * 0.018, 0.36 + (nodes.length % 7) * 0.025)
        const node = { id: idFor("n", nodes), shape: kind.shape, text: kind.title,
            x: position.x, y: position.y, width: shapeWidth, height: shapeHeight }
        remember()
        nodes = nodes.concat([node])
        selectedNodes = [node.id]
        selectedEdge = ""
        connectFrom = ""
        connecting = false
        lassoEnabled = false
        panEnabled = false
    }

    function updateNode(updated) {
        nodes = nodes.map(node => node.id === updated.id ? updated : node)
    }

    function startConnect() {
        const source = connecting ? "" : selectedNode
        connecting = !connecting
        lassoEnabled = false
        panEnabled = false
        markerCanceled()
        connectFrom = source
        selectedEdge = ""
        errorText = ""
    }

    function connectTo(id) {
        if (!id)
            return
        selectedNodes = [id]
        if (!connectFrom.length) {
            connectFrom = id
            return
        }
        if (connectFrom === id)
            return
        const prior = edges.find(edge => edge.from === connectFrom && edge.to === id)
        if (prior) {
            selectedEdge = prior.id
        } else if (edges.length < 128) {
            remember()
            const edge = { id: idFor("e", edges), from: connectFrom, to: id, text: "" }
            edges = edges.concat([edge])
            selectedEdge = edge.id
        }
        if (selectedEdge)
            selectedNodes = []
        connectFrom = ""
        connecting = false
    }

    function finishConnector(point, moved) {
        const source = connectorSource
        connectorSource = ""
        if (!source)
            return
        if (!moved) {
            startConnect()
            return
        }
        const target = insideCanvas(point) ? canvas.nodeAt(point.x, point.y) : ""
        if (target && target !== source) {
            startConnect()
            connectTo(target)
        }
    }

    function changeShape(shape) {
        if (!activeNode || activeNode.shape === shape)
            return
        remember()
        updateNode(Object.assign({}, activeNode, { shape: shape }))
    }

    function markerPressed(x, y) {
        if (pointerStart)
            return
        if (panEnabled) {
            pointerStart = { x: x, y: y, pan: Qt.point(view.x, view.y) }
            return
        }
        if (lassoEnabled) {
            pointerStart = { lasso: true }
            lassoPoints = [Qt.point(x, y)]
            return
        }
        if (resizeNode && Math.abs(x - resizeHandle.x - resizeHandle.width / 2) <= 18 * uiScale
                && Math.abs(y - resizeHandle.y - resizeHandle.height / 2) <= 18 * uiScale) {
            pointerStart = { x: x, y: y, node: resizeNode, resizing: true }
            return
        }
        const id = canvas.nodeAt(x, y)
        if (connecting) {
            connectTo(id)
            return
        }
        if (!id || !selectedNodes.includes(id))
            selectedNodes = id ? [id] : []
        selectedEdge = id ? "" : canvas.edgeAt(x, y)
        const node = id ? nodes.find(entry => entry.id === id) : null
        if (node)
            pointerStart = { x: x, y: y, node: node,
                bounds: Arrangement.bounds(nodes.filter(entry => selectedNodes.includes(entry.id))) }
    }

    function markerMoved(x, y) {
        if (!pointerStart || view.width <= 0 || view.height <= 0)
            return
        if (pointerStart.lasso) {
            const previous = lassoPoints[lassoPoints.length - 1]
            const point = Qt.point(Math.max(0, Math.min(canvas.width, x)), Math.max(0, Math.min(canvas.height, y)))
            if (Math.hypot(point.x - previous.x, point.y - previous.y) >= 3 * uiScale) {
                const path = lassoPoints.length >= 512 ? lassoPoints.filter((_, index) => index % 2 === 0) : lassoPoints
                lassoPoints = path.concat([point])
            }
            return
        }
        const dx = x - pointerStart.x
        const dy = y - pointerStart.y
        if (pointerStart.pan) {
            setViewOffset(pointerStart.pan.x + dx, pointerStart.pan.y + dy)
            return
        }
        if (!previewNodes && Math.hypot(dx, dy) < 5 * uiScale)
            return
        const node = pointerStart.node
        if (pointerStart.resizing) {
            const width = Math.max(Math.min(0.12, node.width), Math.min(1 - node.x, snap(node.x + node.width + dx / view.width) - node.x))
            const height = Math.max(Math.min(0.08, node.height), Math.min(1 - node.y, snap(node.y + node.height + dy / view.height) - node.y))
            previewNodes = width === node.width && height === node.height ? nodes
                : nodes.map(entry => entry.id === node.id ? Object.assign({}, node, { width: width, height: height }) : entry)
        } else {
            const bounds = pointerStart.bounds
            previewNodes = Arrangement.move(nodes, selectedNodes,
                snap(bounds.x + dx / view.width) - bounds.x, snap(bounds.y + dy / view.height) - bounds.y)
        }
    }

    function markerReleased(x, y) {
        markerMoved(x, y)
        if (pointerStart && pointerStart.lasso && lassoPoints.length >= 3) {
            selectedNodes = Array.from(canvas.nodesInLasso(lassoPoints))
            selectedEdge = ""
            lassoEnabled = false
        } else if (previewNodes && previewNodes !== nodes) {
            remember()
            nodes = previewNodes
        }
        markerCanceled()
    }

    function markerCanceled() {
        pointerStart = null
        previewNodes = null
        lassoPoints = []
    }

    function toggleLasso() {
        markerCanceled()
        lassoEnabled = !lassoEnabled
        panEnabled = false
        connecting = false
        connectFrom = ""
    }

    function togglePan() {
        markerCanceled()
        panEnabled = !panEnabled
        lassoEnabled = false
        connecting = false
        connectFrom = ""
    }

    function setViewOffset(x, y) {
        viewOffset = Qt.point(Math.max(canvas.width * (1 - zoom), Math.min(0, x)),
            Math.max(canvas.height * (1 - zoom), Math.min(0, y)))
    }

    function changeZoom(factor, x, y) {
        if (view.width <= 0 || view.height <= 0)
            return
        markerCanceled()
        const scale = Math.max(1, Math.min(3, Math.round(zoom * factor * 100) / 100))
        const localX = (x - view.x) / zoom
        const localY = (y - view.y) / zoom
        zoom = scale
        setViewOffset(x - localX * scale, y - localY * scale)
        if (zoom === 1)
            panEnabled = false
    }

    function fitView() {
        markerCanceled()
        zoom = 1
        viewOffset = Qt.point(0, 0)
        panEnabled = false
    }

    function arrange(action, value) {
        const result = action === "align" ? Arrangement.align(nodes, selectedNodes, value)
            : Arrangement.distribute(nodes, selectedNodes, value)
        if (result !== nodes) {
            remember()
            nodes = result
        }
        arrangeDialog.close()
    }

    function changeLabel(value) {
        const node = nodes.find(entry => entry.id === selectedNode)
        const edge = edges.find(entry => entry.id === selectedEdge)
        const entry = node || edge
        if (!entry || entry.text === value)
            return
        remember()
        if (node)
            updateNode(Object.assign({}, node, { text: value }))
        else
            updateEdge(Object.assign({}, edge, { text: value }))
    }

    function removeSelection() {
        if (!hasSelection)
            return
        remember()
        if (selectedNodes.length) {
            nodes = nodes.filter(node => !selectedNodes.includes(node.id))
            edges = edges.filter(edge => !selectedNodes.includes(edge.from) && !selectedNodes.includes(edge.to))
        } else {
            edges = edges.filter(edge => edge.id !== selectedEdge)
        }
        selectedNodes = []
        selectedEdge = ""
        connectFrom = ""
        connecting = false
    }

    function openLabel() {
        const entry = selectedNode ? nodes.find(node => node.id === selectedNode)
            : edges.find(edge => edge.id === selectedEdge)
        if (!entry)
            return
        labelInput.textLimit = selectedNode ? 200 : 120
        labelInput.text = entry.text
        labelDialog.open()
        labelInput.forceActiveFocus()
        labelInput.selectAll()
    }

    component Action: PaperButton {
        uiScale: editor.uiScale
    }

    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: editor.markerTarget !== null && editor.history.length > 0
        onActivated: editor.undo()
    }

    Shortcut {
        sequences: [StandardKey.Redo]
        enabled: editor.markerTarget !== null && editor.future.length > 0
        onActivated: editor.redo()
    }

    component Tool: Action {
        iconOnly: true
        ToolTip.visible: !editor.tabletMode && hovered
        ToolTip.text: text
    }

    contentItem: ColumnLayout {
        spacing: 7 * editor.uiScale

        Flow {
            Layout.fillWidth: true
            spacing: 5 * editor.uiScale

            Action {
                objectName: "diagramConnectTool"
                text: qsTr("Collega")
                iconName: "edge"
                filled: editor.connecting
                enabled: editor.nodes.length > 1 && editor.edges.length < 128
                onClicked: editor.startConnect()
            }
            Tool {
                text: qsTr("Lazo")
                iconName: "lasso"
                filled: editor.lassoEnabled
                enabled: editor.nodes.length > 0
                onClicked: editor.toggleLasso()
            }
            Tool {
                text: qsTr("Annulla ultima modifica")
                iconName: "undo"
                enabled: editor.history.length > 0
                onClicked: editor.undo()
            }
            Tool {
                text: qsTr("Ripristina ultima modifica")
                iconName: "redo"
                enabled: editor.future.length > 0
                onClicked: editor.redo()
            }
            Action {
                text: qsTr("Griglia")
                checkable: true
                checked: editor.gridEnabled
                filled: checked
                onToggled: editor.gridEnabled = checked
            }
            Tool {
                text: qsTr("Sposta vista")
                iconName: "pan"
                enabled: editor.zoom > 1
                filled: editor.panEnabled
                onClicked: editor.togglePan()
            }
            Tool {
                text: qsTr("Riduci vista")
                iconName: "zoom-out"
                enabled: editor.zoom > 1
                onClicked: editor.changeZoom(0.8, canvas.width / 2, canvas.height / 2)
            }
            Tool {
                text: qsTr("Ingrandisci vista")
                iconName: "zoom-in"
                enabled: editor.zoom < 3
                onClicked: editor.changeZoom(1.25, canvas.width / 2, canvas.height / 2)
            }
            Action {
                text: Math.round(editor.zoom * 100) + "%"
                quiet: true
                enabled: editor.zoom > 1
                Accessible.name: qsTr("Adatta alla vista")
                ToolTip.visible: !editor.tabletMode && hovered
                ToolTip.text: qsTr("Adatta alla vista")
                onClicked: editor.fitView()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: editor.sidePanels ? 3 : 1
            rowSpacing: 7 * editor.uiScale
            columnSpacing: 8 * editor.uiScale

            Rectangle {
                objectName: "diagramShapePalette"
                Layout.row: 0
                Layout.column: 0
                Layout.fillWidth: !editor.sidePanels
                Layout.fillHeight: editor.sidePanels
                Layout.preferredWidth: editor.sidePanels ? 124 * editor.uiScale : 0
                implicitHeight: shapeList.implicitHeight + 12 * editor.uiScale
                color: "#f6f6f8"
                border.color: "#d9dce2"
                radius: 6 * editor.uiScale

                ColumnLayout {
                    id: shapeList
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 6 * editor.uiScale
                    spacing: 5 * editor.uiScale

                    Label {
                        visible: editor.sidePanels
                        text: qsTr("Forme")
                        font.weight: Font.DemiBold
                        font.pixelSize: 13 * editor.uiScale
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: editor.sidePanels ? 1 : editor.palette.length
                        columnSpacing: 4 * editor.uiScale
                        rowSpacing: 4 * editor.uiScale

                        Repeater {
                            model: editor.palette
                            delegate: Action {
                                id: shapeButton
                                required property var modelData
                                objectName: "diagramShape_" + modelData.shape
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                Layout.preferredWidth: 1
                                text: modelData.title
                                leftPadding: 0
                                rightPadding: 0
                                topPadding: 4 * editor.uiScale
                                bottomPadding: topPadding
                                enabled: editor.nodes.length < 64
                                ToolTip.text: text
                                ToolTip.visible: !editor.tabletMode && hovered
                                onClicked: editor.addShape(modelData, null)

                                contentItem: ColumnLayout {
                                    spacing: 2 * editor.uiScale
                                    DiagramItem {
                                        Layout.alignment: Qt.AlignHCenter
                                        Layout.preferredWidth: Math.min(38 * editor.uiScale, shapeButton.availableWidth)
                                        Layout.preferredHeight: 20 * editor.uiScale
                                        diagram: ({
                                            styleScale: 7,
                                            nodes: [{id: "preview", shape: shapeButton.modelData.shape, text: "",
                                                x: 0.12, y: 0.14, width: 0.76, height: 0.72}]
                                        })
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: shapeButton.text
                                        textFormat: Text.PlainText
                                        color: shapeButton.labelColor
                                        font.pixelSize: 11 * editor.uiScale
                                        horizontalAlignment: Text.AlignHCenter
                                        elide: Text.ElideRight
                                    }
                                }

                                DragHandler {
                                    target: null
                                    enabled: shapeButton.enabled
                                    onActiveChanged: {
                                        if (active)
                                            editor.draggedShape = shapeButton.modelData
                                    }
                                    onActiveTranslationChanged: if (active && editor.draggedShape?.shape === shapeButton.modelData.shape)
                                        editor.shapeDrop = canvas.mapFromItem(shapeButton, centroid.position)
                                    onGrabChanged: (transition, point) => {
                                        if (editor.draggedShape?.shape !== shapeButton.modelData.shape)
                                            return
                                        if (transition === PointerDevice.UngrabExclusive || transition === PointerDevice.CancelGrabExclusive) {
                                            editor.draggedShape = null
                                            if (transition === PointerDevice.UngrabExclusive && point.state === Qt.TouchPointReleased) {
                                                const position = canvas.mapFromItem(null, point.scenePosition)
                                                if (editor.insideCanvas(position))
                                                    editor.addShape(shapeButton.modelData, position)
                                            }
                                            editor.shapeDrop = Qt.point(-1, -1)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                objectName: "diagramWorkspaceCanvas"
                Layout.row: editor.sidePanels ? 0 : 1
                Layout.column: editor.sidePanels ? 1 : 0
                Layout.minimumHeight: 70 * editor.uiScale
                color: "white"
                border.color: "#d9dce2"
                radius: 6 * editor.uiScale
                clip: true

                DiagramItem {
                    id: canvas
                    objectName: "diagramCanvas"
                    z: 1
                    anchors.fill: parent
                    anchors.margins: 8 * editor.uiScale
                    diagram: ({
                        nodes: editor.shownNodes,
                        edges: editor.edges,
                        styleScale: editor.original && editor.original.styleScale !== undefined ? editor.original.styleScale : 1
                    })
                    viewRect: editor.view
                    selectedNode: editor.connecting ? editor.connectFrom : ""
                    selectedNodes: editor.connecting ? [] : editor.selectedNodes
                    selectedEdge: editor.selectedEdge

                    Rectangle {
                        id: resizeHandle
                        visible: editor.resizeNode !== null
                        x: editor.resizeNode ? Math.min(canvas.width - width,
                            editor.view.x + (editor.resizeNode.x + editor.resizeNode.width) * editor.view.width - width / 2) : 0
                        y: editor.resizeNode ? Math.min(canvas.height - height,
                            editor.view.y + (editor.resizeNode.y + editor.resizeNode.height) * editor.view.height - height / 2) : 0
                        width: 13 * editor.uiScale
                        height: width
                        border.width: 2 * editor.uiScale
                        border.color: "#155cc2"
                        color: "white"
                    }

                    MouseArea {
                        anchors.fill: parent
                        preventStealing: true
                        onPressed: mouse => editor.markerPressed(mouse.x, mouse.y)
                        onPositionChanged: mouse => editor.markerMoved(mouse.x, mouse.y)
                        onReleased: mouse => editor.markerReleased(mouse.x, mouse.y)
                        onCanceled: editor.markerCanceled()
                        onDoubleClicked: mouse => {
                            if (!editor.connecting && !editor.panEnabled && !editor.lassoEnabled
                                    && (canvas.nodeAt(mouse.x, mouse.y) || canvas.edgeAt(mouse.x, mouse.y))) {
                                editor.markerCanceled()
                                editor.openLabel()
                            }
                        }
                        onWheel: wheel => {
                            if (wheel.angleDelta.y)
                                editor.changeZoom(wheel.angleDelta.y > 0 ? 1.25 : 0.8, wheel.x, wheel.y)
                        }
                    }

                    Canvas {
                        anchors.fill: parent
                        readonly property var points: editor.lassoPoints
                        readonly property string source: editor.connectorSource
                        readonly property point end: editor.connectorPoint
                        onPointsChanged: if (available) requestPaint()
                        onSourceChanged: if (available) requestPaint()
                        onEndChanged: if (available) requestPaint()
                        onAvailableChanged: if (available) requestPaint()
                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            if (source) {
                                ctx.beginPath()
                                ctx.moveTo(connectorHandle.centerX, connectorHandle.centerY)
                                ctx.lineTo(end.x, end.y)
                                ctx.strokeStyle = "#155cc2"
                                ctx.lineWidth = 2 * editor.uiScale
                                ctx.stroke()
                            }
                            if (points.length < 2)
                                return
                            ctx.beginPath()
                            ctx.moveTo(points[0].x, points[0].y)
                            for (let index = 1; index < points.length; ++index)
                                ctx.lineTo(points[index].x, points[index].y)
                            ctx.closePath()
                            ctx.fillStyle = "#18155cc2"
                            ctx.fill()
                            ctx.strokeStyle = "#155cc2"
                            ctx.lineWidth = 2 * editor.uiScale
                            ctx.stroke()
                        }
                    }

                    DiagramItem {
                        readonly property bool inCanvas: editor.draggedShape !== null && editor.insideCanvas(editor.shapeDrop)
                        readonly property point origin: inCanvas ? editor.positionShape(editor.draggedShape, editor.shapeDrop)
                            : Qt.point(0, 0)
                        visible: inCanvas
                        opacity: 0.55
                        anchors.fill: parent
                        viewRect: editor.view
                        diagram: {
                            if (!inCanvas)
                                return {}
                            const size = editor.shapeSize(editor.draggedShape)
                            return {
                                nodes: [{ id: "preview", shape: editor.draggedShape.shape, text: editor.draggedShape.title,
                                    x: origin.x, y: origin.y, width: size.width, height: size.height }]
                            }
                        }
                    }

                    Item {
                        id: connectorHandle
                        objectName: "diagramConnectorHandle"
                        readonly property var node: editor.activeNode
                        readonly property real centerX: node ? editor.view.x + (node.x + node.width) * editor.view.width : 0
                        readonly property real centerY: node ? editor.view.y + (node.y + node.height / 2) * editor.view.height : 0
                        width: 38 * editor.uiScale
                        height: width
                        x: Math.max(0, Math.min(canvas.width - width, centerX - width / 2))
                        y: Math.max(0, Math.min(canvas.height - height, centerY - height / 2))
                        visible: node !== null && editor.nodes.length > 1 && editor.edges.length < 128
                            && !editor.connecting && !editor.lassoEnabled && !editor.panEnabled && !editor.pointerStart
                            && editor.insideCanvas(Qt.point(centerX, centerY))

                        Rectangle {
                            anchors.centerIn: parent
                            width: 22 * editor.uiScale
                            height: width
                            radius: width / 2
                            color: connectorContact.pressed ? "#dbe9ff" : "#ffffff"
                            border.color: "#155cc2"
                            PaperIcon {
                                name: "edge"
                                tint: "#155cc2"
                                anchors.centerIn: parent
                                width: 14 * editor.uiScale
                                height: width
                            }
                        }

                        MouseArea {
                            id: connectorContact
                            anchors.fill: parent
                            preventStealing: true
                            cursorShape: Qt.CrossCursor
                            property point start: Qt.point(0, 0)
                            onPressed: mouse => {
                                start = mapToItem(canvas, mouse.x, mouse.y)
                                editor.connectorPoint = start
                                editor.connectorSource = connectorHandle.node.id
                            }
                            onPositionChanged: mouse => {
                                if (pressed)
                                    editor.connectorPoint = mapToItem(canvas, mouse.x, mouse.y)
                            }
                            onReleased: mouse => {
                                const point = mapToItem(canvas, mouse.x, mouse.y)
                                editor.finishConnector(point, Math.hypot(point.x - start.x, point.y - start.y) > 7 * editor.uiScale)
                            }
                            onCanceled: editor.connectorSource = ""
                        }
                    }
                }

                Repeater {
                    model: editor.gridEnabled ? 19 : 0
                    delegate: Rectangle {
                        required property int index
                        x: canvas.x + editor.view.x + editor.view.width * (index + 1) * editor.gridStep
                        y: canvas.y
                        width: 1
                        height: canvas.height
                        color: "#e8e9ec"
                    }
                }

                Repeater {
                    model: editor.gridEnabled ? 19 : 0
                    delegate: Rectangle {
                        required property int index
                        x: canvas.x
                        y: canvas.y + editor.view.y + editor.view.height * (index + 1) * editor.gridStep
                        width: canvas.width
                        height: 1
                        color: "#e8e9ec"
                    }
                }

                ColumnLayout {
                    z: 2
                    anchors.centerIn: parent
                    width: parent.width - 28 * editor.uiScale
                    visible: editor.nodes.length === 0
                    spacing: 8 * editor.uiScale

                    Action {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Modello di flusso")
                        iconName: "diagram"
                        onClicked: editor.flowTemplate()
                    }
                    Action {
                        Layout.alignment: Qt.AlignHCenter
                        visible: !editor.draftOnly
                        text: qsTr("Importa .drawio")
                        iconName: "folder"
                        onClicked: editor.importRequested()
                    }
                }
            }

            Rectangle {
                id: inspector
                objectName: "diagramInspector"
                Layout.row: editor.sidePanels ? 0 : 2
                Layout.column: editor.sidePanels ? 2 : 0
                Layout.fillWidth: !editor.sidePanels
                Layout.fillHeight: editor.sidePanels
                Layout.preferredWidth: editor.sidePanels ? 176 * editor.uiScale : 0
                implicitHeight: properties.implicitHeight + 12 * editor.uiScale
                color: "#f6f6f8"
                border.color: "#d9dce2"
                radius: 6 * editor.uiScale
                visible: editor.sidePanels || (editor.hasSelection && !editor.connecting)

                ColumnLayout {
                    id: properties
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: 6 * editor.uiScale
                    spacing: 5 * editor.uiScale

                    Label {
                        visible: editor.sidePanels
                        Layout.fillWidth: true
                        text: editor.activeEdge ? qsTr("Connettore")
                            : editor.selectedNodes.length > 1 ? qsTr("%1 forme selezionate").arg(editor.selectedNodes.length)
                            : qsTr("Proprietà")
                        elide: Text.ElideRight
                        font.weight: Font.DemiBold
                        font.pixelSize: 13 * editor.uiScale
                    }

                    Label {
                        visible: editor.sidePanels && !editor.hasSelection
                        Layout.fillWidth: true
                        text: qsTr("Seleziona una forma o un connettore.")
                        wrapMode: Text.Wrap
                        color: "#535862"
                        font.pixelSize: 12 * editor.uiScale
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 5 * editor.uiScale
                        visible: editor.hasSelection && !editor.connecting

                        ComboBox {
                            objectName: "diagramShapeProperty"
                            visible: editor.activeNode !== null
                            width: editor.sidePanels ? inspector.width - 12 * editor.uiScale : 132 * editor.uiScale
                            implicitHeight: 44 * editor.uiScale
                            font.pixelSize: 13 * editor.uiScale
                            textRole: "title"
                            valueRole: "shape"
                            model: editor.palette
                            currentIndex: editor.activeNode
                                ? editor.palette.findIndex(kind => kind.shape === editor.activeNode.shape) : -1
                            Accessible.name: qsTr("Tipo di forma")
                            onActivated: editor.changeShape(currentValue)
                        }
                        Action {
                            objectName: "diagramEditLabel"
                            visible: editor.activeNode !== null || editor.activeEdge !== null
                            text: qsTr("Etichetta")
                            iconName: "rename"
                            iconOnly: editor.compactActions
                            ToolTip.visible: !editor.tabletMode && hovered && iconOnly
                            ToolTip.text: text
                            onClicked: editor.openLabel()
                        }
                        Tool {
                            visible: editor.activeNode !== null
                            text: qsTr("Duplica forma")
                            iconName: "duplicate"
                            enabled: editor.nodes.length < 64
                            onClicked: editor.duplicateShape()
                        }
                        Tool {
                            visible: editor.activeEdge !== null
                            text: qsTr("Inverti freccia")
                            iconName: "reverse"
                            enabled: editor.activeEdge !== null
                                && !editor.edges.some(edge => edge.from === editor.activeEdge.to && edge.to === editor.activeEdge.from)
                            onClicked: editor.reverseEdge()
                        }
                        Action {
                            objectName: "diagramOrthogonalRoute"
                            visible: editor.activeEdge !== null
                            text: qsTr("Ortogonale")
                            iconName: "orthogonal"
                            iconOnly: editor.compactActions
                            width: editor.sidePanels ? Math.min(implicitWidth, inspector.width - 12 * editor.uiScale) : implicitWidth
                            filled: editor.activeEdge !== null && editor.activeEdge.route === "orthogonal"
                            ToolTip.visible: !editor.tabletMode && hovered && iconOnly
                            ToolTip.text: text
                            onClicked: editor.toggleEdgeRoute()
                        }
                        Action {
                            visible: editor.selectedNodes.length > 1
                            text: qsTr("Allinea e distribuisci")
                            iconName: "align"
                            iconOnly: !editor.sidePanels
                            width: editor.sidePanels ? Math.min(implicitWidth, inspector.width - 12 * editor.uiScale) : implicitWidth
                            ToolTip.visible: !editor.tabletMode && hovered && iconOnly
                            ToolTip.text: text
                            onClicked: arrangeDialog.open()
                        }
                        Tool {
                            text: qsTr("Rimuovi")
                            iconName: "close"
                            onClicked: editor.removeSelection()
                        }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: editor.errorText.length > 0 || editor.importNotice.length > 0
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: editor.errorText ? "#8c2d27" : "#535862"
            text: editor.errorText || editor.importNotice
        }

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            elide: Text.ElideRight
            textFormat: Text.PlainText
            color: "#535862"
            text: editor.connecting ? (editor.connectFrom ? qsTr("Tocca la forma di arrivo.") : qsTr("Tocca la forma di partenza."))
                : editor.lassoEnabled ? qsTr("Cerchia le forme da selezionare.")
                : editor.panEnabled ? qsTr("Trascina per spostare la vista.")
                : editor.nodes.length === 0 ? qsTr("Tocca una forma per inserirla o trascinala sul foglio.")
                : !editor.sidePanels && editor.selectedNodes.length > 1 ? qsTr("%1 forme selezionate").arg(editor.selectedNodes.length)
                : connectorHandle.visible ? qsTr("Trascina il punto sul bordo per collegare due forme.")
                : ""
        }
    }

    footer: RowLayout {
        Action {
            id: more
            visible: !editor.draftOnly
            Layout.leftMargin: editor.padding
            Layout.bottomMargin: editor.padding
            text: qsTr("Altre azioni")
            iconName: "more"
            iconOnly: true
            onClicked: overflow.popup(more, 0, 0)
        }
        Item { Layout.fillWidth: true }
        Action {
            Layout.bottomMargin: editor.padding
            text: qsTr("Chiudi")
            onClicked: editor.close()
        }
        Action {
            Layout.rightMargin: editor.padding
            Layout.bottomMargin: editor.padding
            text: qsTr("Salva")
            filled: true
            enabled: editor.nodes.length > 0
            onClicked: editor.saved(editor.payload())
        }
    }

    Menu {
        id: overflow
        dim: !editor.tabletMode && modal
        MenuItem {
            text: qsTr("Esporta .drawio")
            enabled: editor.nodes.length > 0
            onTriggered: editor.exportRequested(editor.payload())
        }
        MenuItem {
            text: qsTr("Salva su una nuova pagina")
            enabled: editor.nodes.length > 0
            onTriggered: {
                const item = editor.payload()
                delete item.id
                editor.saveOnNewPageRequested(item)
            }
        }
        MenuItem {
            text: qsTr("Elimina diagramma")
            enabled: editor.original !== null
            onTriggered: deleteDialog.open()
        }
    }

    Dialog {
        id: arrangeDialog
        anchors.centerIn: Overlay.overlay
        width: Math.min(editor.width, 465 * editor.uiScale)
        title: qsTr("Allinea e distribuisci")
        modal: true
        dim: !editor.tabletMode
        background: Surface { }
        Overlay.modal: Rectangle { color: "#73000000" }
        padding: 12 * editor.uiScale

        contentItem: ColumnLayout {
            spacing: 6 * editor.uiScale

            Flow {
                Layout.fillWidth: true
                spacing: 6 * editor.uiScale
                Repeater {
                    model: [
                        { key: "left", label: qsTr("Allinea a sinistra") },
                        { key: "center", label: qsTr("Allinea al centro") },
                        { key: "right", label: qsTr("Allinea a destra") },
                        { key: "top", label: qsTr("Allinea in alto") },
                        { key: "middle", label: qsTr("Allinea a metà altezza") },
                        { key: "bottom", label: qsTr("Allinea in basso") }
                    ]
                    delegate: Action {
                        required property var modelData
                        text: modelData.label
                        onClicked: editor.arrange("align", modelData.key)
                    }
                }
            }

            Action {
                text: qsTr("Distribuisci in orizzontale")
                enabled: editor.selectedNodes.length >= 3
                onClicked: editor.arrange("distribute", "horizontal")
            }
            Action {
                text: qsTr("Distribuisci in verticale")
                enabled: editor.selectedNodes.length >= 3
                onClicked: editor.arrange("distribute", "vertical")
            }
        }

        footer: RowLayout {
            Item { Layout.fillWidth: true }
            Action {
                Layout.rightMargin: arrangeDialog.padding
                Layout.bottomMargin: arrangeDialog.padding
                text: qsTr("Chiudi")
                onClicked: arrangeDialog.close()
            }
        }
    }

    Dialog {
        id: labelDialog
        anchors.centerIn: Overlay.overlay
        width: Math.min(editor.width, 460 * editor.uiScale)
        padding: 12 * editor.uiScale
        title: qsTr("Modifica etichetta")
        modal: true
        dim: !editor.tabletMode
        background: Surface { }
        Overlay.modal: Rectangle { color: "#73000000" }
        onClosed: labelInput.clear()

        contentItem: ColumnLayout {
            spacing: 8 * editor.uiScale
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 76 * editor.uiScale
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                TextArea {
                    id: labelInput
                    property int textLimit: 200
                    placeholderText: qsTr("Testo dell’etichetta")
                    selectByMouse: !editor.tabletMode
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    onTextChanged: if (length > textLimit) remove(textLimit, length)
                }
            }
            Action {
                visible: editor.showTouchKeyboard
                Layout.alignment: Qt.AlignRight
                focusPolicy: Qt.NoFocus
                text: qsTr("A capo")
                quiet: true
                onClicked: labelKeyboard.typeKey("\n")
            }
            TouchKeyboard {
                id: labelKeyboard
                visible: editor.showTouchKeyboard
                Layout.fillWidth: true
                uiScale: editor.uiScale
                target: labelInput
            }
        }

        footer: RowLayout {
            Item { Layout.fillWidth: true }
            Action {
                Layout.bottomMargin: labelDialog.padding
                text: qsTr("Annulla")
                onClicked: labelDialog.close()
            }
            Action {
                Layout.rightMargin: labelDialog.padding
                Layout.bottomMargin: labelDialog.padding
                text: qsTr("Applica")
                filled: true
                onClicked: {
                    editor.changeLabel(labelInput.text)
                    labelDialog.close()
                }
            }
        }
    }

    Dialog {
        id: deleteDialog
        anchors.centerIn: Overlay.overlay
        width: Math.min(editor.width, 420 * editor.uiScale)
        title: qsTr("Elimina diagramma")
        modal: true
        dim: !editor.tabletMode
        padding: 12 * editor.uiScale
        background: Surface { }
        Overlay.modal: Rectangle { color: "#73000000" }

        contentItem: Label {
            text: qsTr("Eliminare questo diagramma dalla pagina?")
            wrapMode: Text.Wrap
        }
        footer: RowLayout {
            Item { Layout.fillWidth: true }
            Action {
                Layout.bottomMargin: deleteDialog.padding
                text: qsTr("Annulla")
                onClicked: deleteDialog.close()
            }
            Action {
                Layout.rightMargin: deleteDialog.padding
                Layout.bottomMargin: deleteDialog.padding
                text: qsTr("Elimina")
                onClicked: {
                    deleteDialog.close()
                    if (editor.original)
                        editor.deleteRequested(editor.original.id)
                }
            }
        }
    }
}
