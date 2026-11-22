import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: editor
    objectName: "mixedPageDialog"
    required property Notebook notebook
    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    property string recognitionLanguage: "it"
    property var source: ({})
    property var regions: ({diagram: [], table: [], text: []})
    property string part: "diagram"
    property bool reviewing: false
    property bool showingOriginal: false
    property var diagramDraft: ({})
    property string errorText: ""
    property var recognitionQueue: []
    property int queueIndex: -1
    property int missedLines: 0
    readonly property bool readingInk: queueIndex >= 0
    readonly property bool canAnalyze: regions.diagram.length > 0 && regions.table.length > 0
        && regions.text.length > 0 && !analyzer.busy && !sourcePage.busy
    readonly property bool canCreate: reviewing && !readingInk && titleField.text.trim().length > 0
        && Boolean(diagramDraft.nodes?.length) && (tableField.text.trim().length > 0 || tableField.text.includes("\t"))
        && textField.text.trim().length > 0
    readonly property var parts: [
        {key: "diagram", label: qsTr("Diagramma"), tint: "#315f98"},
        {key: "table", label: qsTr("Tabella"), tint: "#3c6a47"},
        {key: "text", label: qsTr("Testo"), tint: "#8a552e"}
    ]
    readonly property var visibleRegions: {
        const result = []
        for (const group of parts)
            for (const contour of regions[group.key])
                result.push({points: contour.points, tint: group.tint})
        return result
    }

    signal created()

    title: qsTr("Riconosci pagina mista")
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(780 * uiScale, parent.width - 16 * uiScale)
    height: Math.min(1050 * uiScale, parent.height - 16 * uiScale)
    padding: 12 * uiScale
    modal: true
    dim: !tabletMode
    closePolicy: Popup.CloseOnEscape

    function openForPage() {
        source = notebook.mixedPageSource()
        if (!source.bookId) return
        part = "diagram"
        regions = {diagram: [], table: [], text: []}
        diagramDraft = ({})
        errorText = ""
        reviewing = false
        showingOriginal = false
        missedLines = 0
        titleField.text = qsTr("%1 - Convertito").arg(source.title.slice(0, 56).trim())
        textField.text = ""
        tableField.text = ""
        open()
    }

    function addRegion(key: string, points: var) {
        if (regions[key].length >= 6 || points.length < 3) return
        let left = 1, right = 0, top = 1, bottom = 0, twiceArea = 0
        let previous = points[points.length - 1]
        for (const point of points) {
            left = Math.min(left, point.x)
            right = Math.max(right, point.x)
            top = Math.min(top, point.y)
            bottom = Math.max(bottom, point.y)
            twiceArea += previous.x * point.y - point.x * previous.y
            previous = point
        }
        if (right - left < .02 || bottom - top < .02 || Math.abs(twiceArea) < .0007) return
        const updated = Object.assign({}, regions)
        updated[key] = regions[key].concat([{points: points}])
        regions = updated
        errorText = ""
    }

    function removeRegion() {
        const updated = Object.assign({}, regions)
        updated[part] = regions[part].slice(0, -1)
        regions = updated
    }

    function stopRecognition() {
        queueIndex = -1
        recognitionQueue = []
        recognizer.cancel()
    }

    function dismiss() {
        analyzer.cancel()
        stopRecognition()
        diagramEditor.close()
        close()
    }

    function beginReview(draft: var) {
        diagramDraft = draft.diagram || ({})
        textField.text = draft.text || ""
        tableField.text = draft.tableText || ""
        reviewing = true
        showingOriginal = false
        missedLines = 0
        if (!recognizer.available) return
        recognitionQueue = (draft.tableInk || []).map(row => ({part: "table", strokes: row.strokes}))
            .concat((draft.tableCellInk || []).map(cell => ({part: "table-cell", row: cell.row, column: cell.column,
                strokes: cell.strokes})))
            .concat((draft.textInk || []).map(row => ({part: "text", strokes: row.strokes})))
            .concat((draft.diagramInk || []).map(label => ({part: "diagram", strokes: label.strokes,
                nodeId: label.nodeId, orientation: label.stacked ? "stacked" : label.vertical ? "clockwise" : "auto"})))
        if (recognitionQueue.length) {
            queueIndex = 0
            Qt.callLater(recognizeNext)
        }
    }

    function recognizeNext() {
        if (!visible || queueIndex < 0) return
        if (queueIndex >= recognitionQueue.length) {
            stopRecognition()
            return
        }
        const languages = recognizer.availableLanguages
        const language = languages.indexOf(recognitionLanguage) >= 0 ? recognitionLanguage : languages[0]
        const job = recognitionQueue[queueIndex]
        if (job.part === "diagram")
            recognizer.recognizeDiagramLabel(job.strokes, language, job.orientation)
        else if (job.part === "text")
            recognizer.recognizeProse(job.strokes, language)
        else
            recognizer.recognize(job.strokes, language)
    }

    function applyRecognition(value: string) {
        const job = recognitionQueue[queueIndex]
        if (!job) return
        const text = value.trim()
        if (!text) {
            ++missedLines
        } else if (job.part === "text") {
            textField.text += (textField.text ? "\n" : "") + text
        } else if (job.part === "table") {
            tableField.text += (tableField.text ? "\n" : "") + text.replace(/\s*\n\s*/g, " ")
        } else if (job.part === "table-cell") {
            const rows = tableField.text.split("\n")
            const cells = rows[job.row].split("\t")
            cells[job.column] = text.replace(/[\t\r\n]+/g, " ")
            rows[job.row] = cells.join("\t")
            tableField.text = rows.join("\n")
        } else if (diagramDraft.nodes) {
            const nodes = diagramDraft.nodes.map(node => node.id === job.nodeId && !(node.text || "").trim()
                ? Object.assign({}, node, {text: text.slice(0, 200)})
                : node)
            diagramDraft = Object.assign({}, diagramDraft, {nodes})
        }
        ++queueIndex
        Qt.callLater(recognizeNext)
    }

    function makeTable(): var {
        const lines = tableField.text.split(/\r?\n/)
        const blank = line => !line.includes("\t") && line.trim().length === 0
        while (lines.length > 1 && blank(lines[0])) lines.shift()
        while (lines.length > 1 && blank(lines[lines.length - 1])) lines.pop()
        const rows = lines.map(line => line.split(line.includes("\t") ? "\t" : "|").map(cell => cell.trim()))
        const columns = rows.reduce((largest, row) => Math.max(largest, row.length), 0)
        if (rows.length > 12 || columns > 8 || rows.some(row => row.some(cell => cell.length > 500))) {
            errorText = qsTr("Usa al massimo 12 righe, 8 colonne e 500 caratteri per cella.")
            return null
        }
        const cells = []
        for (const row of rows)
            for (let column = 0; column < columns; ++column)
                cells.push(row[column] || "")
        return {type: "table", rows: rows.length, cols: columns, cells: cells}
    }

    function saveDraft() {
        if (!canCreate) return
        const table = makeTable()
        if (!table) return
        if (notebook.createMixedDocument(source.bookId, source.pageId, source.revision,
                titleField.text, textField.text, diagramDraft, table)) {
            close()
            created()
        } else {
            errorText = notebook.error
        }
    }

    onClosed: {
        analyzer.cancel()
        stopRecognition()
        diagramEditor.close()
        source = ({})
    }

    MixedPageAnalyzer {
        id: analyzer
        onFinished: (draft, error) => {
            if (!editor.visible) return
            if (error) editor.errorText = error
            else editor.beginReview(draft)
        }
    }

    Handwriting {
        id: recognizer
        onFinished: success => editor.applyRecognition(success ? result : "")
    }

    DiagramEditor {
        id: diagramEditor
        uiScale: editor.uiScale
        tabletMode: editor.tabletMode
        showTouchKeyboard: editor.showTouchKeyboard
        draftOnly: true
        onSaved: draft => {
            editor.diagramDraft = draft
            diagramEditor.close()
        }
    }

    contentItem: ColumnLayout {
        spacing: 6 * editor.uiScale

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 14 * editor.uiScale
            text: editor.reviewing
                ? qsTr("Rivedi le tre bozze. Il testo collegherà tabella e diagramma nello stesso quaderno.")
                : qsTr("Scegli una parte e disegna un contorno attorno al contenuto. Puoi disegnare più contorni per una parte.")
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 4 * editor.uiScale
            Repeater {
                model: editor.parts
                PaperButton {
                    required property var modelData
                    objectName: "mixedPagePart_" + modelData.key
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    uiScale: editor.uiScale
                    text: modelData.label
                    filled: editor.part === modelData.key
                    enabled: !analyzer.busy
                    onClicked: editor.part = modelData.key
                }
            }
            PaperButton {
                uiScale: editor.uiScale
                text: editor.reviewing ? qsTr("Pagina origine") : qsTr("Rimuovi ultimo contorno")
                iconName: editor.reviewing ? "paper" : "undo"
                iconOnly: true
                quiet: true
                filled: editor.reviewing && editor.showingOriginal
                enabled: editor.reviewing || (editor.regions[editor.part].length > 0 && !analyzer.busy)
                onClicked: if (editor.reviewing) editor.showingOriginal = !editor.showingOriginal; else editor.removeRegion()
            }
        }

        TextField {
            id: titleField
            objectName: "mixedPageTitle"
            visible: editor.reviewing && !editor.showingOriginal
            Layout.fillWidth: true
            placeholderText: qsTr("Titolo del quaderno")
            maximumLength: 70
        }

        Rectangle {
            visible: !editor.reviewing || editor.showingOriginal
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#e8eaed"
            clip: true
            Item {
                id: previewCanvas
                width: Math.min(parent.width, parent.height * .75)
                height: width * 4 / 3
                anchors.centerIn: parent
                PagePreviewItem {
                    id: sourcePage
                    anchors.fill: parent
                    thumbnail: false
                    content: editor.visible ? (editor.source.content || ({})) : ({})
                }
                Canvas {
                    id: contours
                    anchors.fill: parent
                    readonly property var outlines: editor.visibleRegions
                    readonly property real outlineWidth: Math.max(2, 2 * editor.uiScale)
                    onOutlinesChanged: requestPaint()
                    onOutlineWidthChanged: requestPaint()
                    onWidthChanged: requestPaint()
                    onHeightChanged: requestPaint()
                    onAvailableChanged: if (available) requestPaint()
                    onVisibleChanged: if (visible) requestPaint()

                    function paintOutline(context: var, points: var, tint: string, closed: bool) {
                        if (points.length < 2) return
                        context.strokeStyle = tint
                        context.beginPath()
                        context.moveTo(points[0].x * width, points[0].y * height)
                        for (let i = 1; i < points.length; ++i)
                            context.lineTo(points[i].x * width, points[i].y * height)
                        if (closed) context.closePath()
                        context.stroke()
                    }

                    onPaint: region => {
                        const context = getContext("2d")
                        context.save()
                        context.beginPath()
                        context.rect(region.x, region.y, region.width, region.height)
                        context.clip()
                        context.clearRect(region.x, region.y, region.width, region.height)
                        context.lineWidth = outlineWidth
                        context.lineCap = "round"
                        context.lineJoin = "round"
                        for (const outline of outlines)
                            paintOutline(context, outline.points, outline.tint, true)
                        paintOutline(context, regionPicker.livePoints, regionPicker.tint, false)
                        context.restore()
                    }
                }
                // The Paper Pro Marker sends its complete contact as mouse events in dialogs.
                MouseArea {
                    id: regionPicker
                    objectName: "mixedPageRegionPicker"
                    anchors.fill: parent
                    enabled: !editor.reviewing && !analyzer.busy && !sourcePage.busy
                        && editor.regions[editor.part].length < 6
                    property string activePart: ""
                    property var livePoints: []
                    readonly property string tint: editor.parts.find(group => group.key === activePart)?.tint || "#315f98"
                    readonly property real sampleDistance: Math.max(1.5, editor.uiScale * 1.5)

                    function sample(x: real, y: real) {
                        if (width <= 0 || height <= 0) return
                        const point = Qt.point(Math.max(0, Math.min(1, x / width)), Math.max(0, Math.min(1, y / height)))
                        const previous = livePoints[livePoints.length - 1]
                        const dx = previous ? (point.x - previous.x) * width : 0
                        const dy = previous ? (point.y - previous.y) * height : 0
                        if (previous && dx * dx + dy * dy < sampleDistance * sampleDistance) return

                        if (livePoints.length >= 256) {
                            livePoints = livePoints.filter((_, index) => index % 2 === 0)
                            contours.requestPaint()
                        } else if (previous) {
                            // Refresh only the changed segment while following the tip.
                            const padding = contours.outlineWidth + 2
                            contours.markDirty(Qt.rect(Math.min(point.x, previous.x) * width - padding,
                                Math.min(point.y, previous.y) * height - padding,
                                Math.abs(dx) + 2 * padding, Math.abs(dy) + 2 * padding))
                        }
                        livePoints.push(point)
                    }

                    function clearContour() {
                        livePoints = []
                        activePart = ""
                        contours.requestPaint()
                    }

                    onPressed: mouse => {
                        activePart = editor.part
                        livePoints = []
                        sample(mouse.x, mouse.y)
                    }
                    onPositionChanged: mouse => { if (pressed && activePart) sample(mouse.x, mouse.y) }
                    onReleased: mouse => {
                        if (activePart) {
                            sample(mouse.x, mouse.y)
                            editor.addRegion(activePart, livePoints)
                        }
                        clearContour()
                    }
                    onCanceled: clearContour()
                    onEnabledChanged: if (!enabled && activePart) clearContour()
                }
            }
        }

        ColumnLayout {
            visible: editor.reviewing && !editor.showingOriginal && editor.part === "diagram"
            Layout.fillHeight: true
            Layout.fillWidth: true
            DiagramItem {
                Layout.fillHeight: true
                Layout.fillWidth: true
                diagram: editor.diagramDraft
            }
            PaperButton {
                Layout.alignment: Qt.AlignRight
                uiScale: editor.uiScale
                text: editor.diagramDraft.nodes?.length ? qsTr("Rivedi forme ed etichette") : qsTr("Disegna il diagramma")
                enabled: !editor.readingInk
                onClicked: diagramEditor.openFor(editor.diagramDraft.nodes ? editor.diagramDraft : null)
            }
        }
        ScrollView {
            visible: editor.reviewing && !editor.showingOriginal && editor.part === "table"
            Layout.fillHeight: true
            Layout.fillWidth: true
            TextArea {
                id: tableField
                objectName: "mixedPageTable"
                readOnly: editor.readingInk
                font.pixelSize: 15 * editor.uiScale
                placeholderText: qsTr("Una linea per riga. Separa le colonne con Tab o |.")
                wrapMode: TextEdit.Wrap
            }
        }
        ScrollView {
            visible: editor.reviewing && !editor.showingOriginal && editor.part === "text"
            Layout.fillHeight: true
            Layout.fillWidth: true
            TextArea {
                id: textField
                objectName: "mixedPageText"
                readOnly: editor.readingInk
                font.pixelSize: 15 * editor.uiScale
                placeholderText: qsTr("Rivedi o scrivi il testo della pagina.")
                wrapMode: TextEdit.Wrap
            }
        }
        TouchKeyboard {
            Layout.fillWidth: true
            visible: editor.showTouchKeyboard && editor.reviewing && !editor.showingOriginal && !editor.readingInk
                && (titleField.activeFocus || tableField.activeFocus || textField.activeFocus)
            uiScale: editor.uiScale
            target: titleField.activeFocus ? titleField : editor.part === "table" ? tableField : textField
        }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            wrapMode: Text.WordWrap
            font.pixelSize: 13 * editor.uiScale
            text: editor.errorText || (analyzer.busy ? qsTr("Analisi delle aree…") : editor.readingInk
                ? qsTr("Riconoscimento locale · %1 / %2").arg(editor.queueIndex + 1).arg(editor.recognitionQueue.length)
                : editor.reviewing && !recognizer.available ? qsTr("Il motore di scrittura non è disponibile. Puoi completare le bozze a mano.")
                : editor.missedLines > 0 ? qsTr("Alcune righe o celle richiedono una trascrizione manuale.") : sourcePage.error)
            color: editor.errorText ? "#8d3030" : "#535862"
        }
    }

    footer: RowLayout {
        spacing: 4 * editor.uiScale
        PaperButton {
            uiScale: editor.uiScale
            text: editor.readingInk ? qsTr("Ferma riconoscimento") : editor.reviewing ? qsTr("Indietro") : qsTr("Annulla")
            quiet: true
            onClicked: {
                if (editor.readingInk) editor.stopRecognition()
                else if (!editor.reviewing) editor.dismiss()
                else {
                    editor.showingOriginal = false
                    editor.reviewing = false
                    editor.errorText = ""
                }
            }
        }
        Item { Layout.fillWidth: true }
        PaperButton {
            objectName: "mixedPageContinue"
            uiScale: editor.uiScale
            text: editor.reviewing ? qsTr("Crea quaderno") : qsTr("Riconosci")
            filled: true
            enabled: editor.reviewing ? editor.canCreate : editor.canAnalyze
            onClicked: {
                if (editor.reviewing) editor.saveDraft()
                else {
                    editor.errorText = ""
                    analyzer.analyze(editor.source.content, editor.regions)
                }
            }
        }
    }
}
