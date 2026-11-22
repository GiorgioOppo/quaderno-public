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
    property int nextId: 0

    signal saved(var item)
    signal saveOnNewPageRequested(var item)
    signal deleteRequested(string id)
    signal exportRequested(var item)
    signal importRequested()

    title: importTitle.length ? qsTr("Diagramma · %1").arg(importTitle) : qsTr("Diagramma")
    anchors.centerIn: Overlay.overlay
    width: Math.min(710 * uiScale, parent ? parent.width - 16 * uiScale : 710 * uiScale)
    height: Math.min(800 * uiScale, parent ? parent.height - 16 * uiScale : 800 * uiScale)
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
    readonly property bool compactActions: availableWidth < 680 * uiScale
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

    function addShape(kind) {
        if (nodes.length >= 64)
            return
        const shapeWidth = kind.shape === "decision" ? 0.29 : 0.25
        const shapeHeight = kind.shape === "decision" ? 0.18 : 0.13
        let position = null
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
    }

    function updateNode(updated) {
        nodes = nodes.map(node => node.id === updated.id ? updated : node)
    }

    function startConnect() {
        connecting = !connecting
        lassoEnabled = false
        panEnabled = false
        markerCanceled()
        connectFrom = ""
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
            updateNode({ id: node.id, shape: node.shape, text: value, x: node.x, y: node.y,
                width: node.width, height: node.height })
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

    contentItem: ColumnLayout {
        spacing: 6 * editor.uiScale

        Flow {
            Layout.fillWidth: true
            spacing: 5 * editor.uiScale
            Repeater {
                model: editor.palette
                delegate: Action {
                    required property var modelData
                    text: modelData.title
                    implicitWidth: Math.max(74 * editor.uiScale,
                        implicitContentWidth + leftPadding + rightPadding + 4 * editor.uiScale)
                    enabled: editor.nodes.length < 64
                    onClicked: editor.addShape(modelData)
                }
            }
            Action {
                text: qsTr("Griglia")
                checkable: true
                checked: editor.gridEnabled
                filled: checked
                onToggled: editor.gridEnabled = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 5 * editor.uiScale

            Action {
                text: qsTr("Lazo")
                iconName: "lasso"
                filled: editor.lassoEnabled
                enabled: editor.nodes.length > 0
                onClicked: editor.toggleLasso()
            }
            Action {
                text: qsTr("Allinea e distribuisci")
                iconName: "align"
                iconOnly: editor.compactActions
                ToolTip.visible: !editor.tabletMode && hovered && iconOnly
                ToolTip.text: text
                enabled: editor.selectedNodes.length >= 2
                onClicked: arrangeDialog.open()
            }
            Item { Layout.fillWidth: true }
            Action {
                text: qsTr("Sposta vista")
                iconName: "pan"
                iconOnly: true
                ToolTip.visible: !editor.tabletMode && hovered
                ToolTip.text: text
                enabled: editor.zoom > 1
                filled: editor.panEnabled
                onClicked: editor.togglePan()
            }
            Action {
                text: qsTr("Riduci vista")
                iconName: "zoom-out"
                iconOnly: true
                ToolTip.visible: !editor.tabletMode && hovered
                ToolTip.text: text
                enabled: editor.zoom > 1
                onClicked: editor.changeZoom(0.8, canvas.width / 2, canvas.height / 2)
            }
            Action {
                text: qsTr("Ingrandisci vista")
                iconName: "zoom-in"
                iconOnly: true
                ToolTip.visible: !editor.tabletMode && hovered
                ToolTip.text: text
                enabled: editor.zoom < 3
                onClicked: editor.changeZoom(1.25, canvas.width / 2, canvas.height / 2)
            }
            Action {
                text: qsTr("Adatta alla vista")
                iconName: "collapse"
                iconOnly: true
                ToolTip.visible: !editor.tabletMode && hovered
                ToolTip.text: text
                enabled: editor.zoom > 1
                onClicked: editor.fitView()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 80 * editor.uiScale
            color: "white"
            border.color: "#d9dce2"
            radius: 6 * editor.uiScale
            clip: true

            DiagramItem {
                id: canvas
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
                    onWheel: wheel => {
                        if (wheel.angleDelta.y)
                            editor.changeZoom(wheel.angleDelta.y > 0 ? 1.25 : 0.8, wheel.x, wheel.y)
                    }
                }

                Canvas {
                    anchors.fill: parent
                    readonly property var points: editor.lassoPoints
                    onPointsChanged: if (available) requestPaint()
                    onAvailableChanged: if (available) requestPaint()
                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
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
                    text: qsTr("Importa .drawio")
                    iconName: "folder"
                    onClicked: editor.importRequested()
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: 5 * editor.uiScale
            Action {
                text: qsTr("Collega")
                iconName: editor.compactActions ? "edge" : ""
                iconOnly: editor.compactActions
                ToolTip.visible: !editor.tabletMode && hovered && iconOnly
                ToolTip.text: text
                enabled: editor.nodes.length > 1 && editor.edges.length < 128
                filled: editor.connecting
                onClicked: editor.startConnect()
            }
            Action {
                text: qsTr("Etichetta")
                iconName: editor.compactActions ? "rename" : ""
                iconOnly: editor.compactActions
                ToolTip.visible: !editor.tabletMode && hovered && iconOnly
                ToolTip.text: text
                enabled: (editor.activeNode !== null || editor.activeEdge !== null) && !editor.connecting
                onClicked: editor.openLabel()
            }
            Action {
                text: qsTr("Duplica forma")
                iconName: editor.compactActions ? "duplicate" : ""
                iconOnly: editor.compactActions
                ToolTip.visible: !editor.tabletMode && hovered && iconOnly
                ToolTip.text: text
                visible: editor.activeNode !== null && !editor.connecting
                enabled: editor.nodes.length < 64
                onClicked: editor.duplicateShape()
            }
            Action {
                text: qsTr("Inverti freccia")
                iconName: editor.compactActions ? "reverse" : ""
                iconOnly: editor.compactActions
                ToolTip.visible: !editor.tabletMode && hovered && iconOnly
                ToolTip.text: text
                visible: editor.activeEdge !== null && !editor.connecting
                enabled: editor.activeEdge !== null && !editor.edges.some(edge => edge.from === editor.activeEdge.to && edge.to === editor.activeEdge.from)
                onClicked: editor.reverseEdge()
            }
            Action {
                text: qsTr("Freccia ortogonale")
                iconName: editor.compactActions ? "orthogonal" : ""
                iconOnly: editor.compactActions
                ToolTip.visible: !editor.tabletMode && hovered && iconOnly
                ToolTip.text: text
                visible: editor.activeEdge !== null && !editor.connecting
                filled: editor.activeEdge !== null && editor.activeEdge.route === "orthogonal"
                onClicked: editor.toggleEdgeRoute()
            }
            Action {
                text: qsTr("Rimuovi")
                iconName: editor.compactActions ? "close" : ""
                iconOnly: editor.compactActions
                ToolTip.visible: !editor.tabletMode && hovered && iconOnly
                ToolTip.text: text
                enabled: editor.hasSelection
                onClicked: editor.removeSelection()
            }
            Action {
                text: qsTr("Annulla ultima modifica")
                iconName: "undo"
                iconOnly: true
                ToolTip.visible: !editor.tabletMode && hovered
                ToolTip.text: text
                enabled: editor.history.length > 0
                onClicked: editor.undo()
            }
            Action {
                text: qsTr("Ripristina ultima modifica")
                iconName: "redo"
                iconOnly: true
                ToolTip.visible: !editor.tabletMode && hovered
                ToolTip.text: text
                enabled: editor.future.length > 0
                onClicked: editor.redo()
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
            Layout.minimumHeight: 18 * editor.uiScale
            elide: Text.ElideRight
            textFormat: Text.PlainText
            color: "#535862"
            text: editor.connecting ? (editor.connectFrom ? qsTr("Tocca la forma di arrivo.") : qsTr("Tocca la forma di partenza."))
                : editor.lassoEnabled ? qsTr("Cerchia le forme da selezionare.")
                : editor.panEnabled ? qsTr("Trascina per spostare la vista.") : editor.selectedNodes.length > 1
                ? qsTr("%1 forme selezionate").arg(editor.selectedNodes.length) : ""
        }
    }

    footer: RowLayout {
        Action {
            id: more
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
