pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: editor
    objectName: "chartEditor"

    required property real uiScale
    required property bool tabletMode
    property bool showTouchKeyboard: tabletMode
    property string errorText: ""

    signal saveRequested(var item)
    signal saveOnNewPageRequested(var item)
    signal exportCsvRequested(string csv)
    signal deleteRequested(string id)

    property var originalItem: null
    property var points: []
    property string chartKind: "line"
    property int pointIndex: 0
    property string recognitionError: ""
    property string notice: ""
    property string pointError: ""
    property bool editingPoints: false
    property bool confirmingDelete: false
    property bool fromLibrary: false
    property Item keyboardTarget: null

    readonly property bool existing: originalItem !== null && Boolean(originalItem.id)
    readonly property color ink: "#24262c"
    readonly property color muted: "#555b67"
    readonly property color border: "#d9dce2"
    readonly property color blue: "#155cc2"
    readonly property real maximumScale: 1000000000000
    readonly property var chart: {
        const item = {
            type: "chart", chartKind: chartKind, points: points,
            title: chartTitle.text.trim(), xLabel: labelX.text.trim(), yLabel: labelY.text.trim()
        }
        if (calibrated.checked && validScale) {
            item.xMax = parseNumber(maximumX.text)
            item.yMax = parseNumber(maximumY.text)
            const lowX = parseNumber(minimumX.text)
            const lowY = parseNumber(minimumY.text)
            if (lowX < 0 || lowY < 0) item.minimum = {x: lowX, y: lowY}
        }
        if (existing) {
            item.id = originalItem.id
            item.x = originalItem.x
            item.y = originalItem.y
            item.width = originalItem.width
            item.height = originalItem.height
        }
        return item
    }
    readonly property bool validScale: parseNumber(maximumX.text) > 0 && parseNumber(maximumX.text) <= maximumScale
        && parseNumber(maximumY.text) > 0 && parseNumber(maximumY.text) <= maximumScale
        && parseNumber(minimumX.text) <= 0 && parseNumber(minimumX.text) >= -maximumScale
        && parseNumber(minimumY.text) <= 0 && parseNumber(minimumY.text) >= -maximumScale
    readonly property bool keyboardOpen: showTouchKeyboard && keyboardTarget !== null

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(550 * uiScale, (parent ? parent.width : 550 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    spacing: 8 * uiScale
    modal: true
    dim: !tabletMode
    focus: true
    closePolicy: Popup.CloseOnEscape
    title: existing ? qsTr("Modifica grafico") : fromLibrary ? qsTr("Nuovo grafico") : qsTr("Converti in grafico")

    ChartRecognizer { id: recognizer }

    function openFor(strokes: var) {
        const result = recognizer.recognize(strokes)
        loadItem(result.item || null, result.error || "")
    }

    function openItem(item: var) {
        loadItem(item, "")
    }

    function openDraft(title: string) {
        loadItem({ chartKind: "line", title: title, points: [{ x: 0.25, y: 0.25 }, { x: 0.75, y: 0.75 }] }, "")
        fromLibrary = true
        editingPoints = true
    }

    function openExisting(item: var) {
        openItem(item)
    }

    function loadItem(item: var, failure: string) {
        originalItem = item && item.id ? item : null
        fromLibrary = false
        chartKind = item ? item.chartKind : "line"
        points = item ? Array.from(item.points) : []
        chartTitle.text = item ? item.title || "" : ""
        labelX.text = item ? item.xLabel || "" : ""
        labelY.text = item ? item.yLabel || "" : ""
        calibrated.checked = Boolean(item && item.xMax > 0 && item.yMax > 0)
        maximumX.text = calibrated.checked ? String(item.xMax) : ""
        maximumY.text = calibrated.checked ? String(item.yMax) : ""
        minimumX.text = String(item && item.minimum ? item.minimum.x : 0)
        minimumY.text = String(item && item.minimum ? item.minimum.y : 0)
        errorText = ""
        recognitionError = failure
        notice = ""
        editingPoints = false
        confirmingDelete = false
        newPage.checked = false
        keyboardTarget = null
        selectPoint(0)
        if (scroll.contentItem)
            scroll.contentItem.contentY = 0
        open()
    }

    onClosed: {
        keyboardTarget = null
        points = []
        originalItem = null
        fromLibrary = false
        chartTitle.clear()
        labelX.clear()
        labelY.clear()
        maximumX.clear()
        maximumY.clear()
    }

    function parseNumber(value: string): real {
        const number = value.trim().replace(",", ".")
        return /^[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:e[+-]?\d+)?$/i.test(number) ? Number(number) : NaN
    }

    function formatPercent(value: real): string {
        return String(Math.round(value * 10000) / 100)
    }

    function selectPoint(index: int) {
        pointIndex = Math.max(0, Math.min(index, points.length - 1))
        pointX.text = points.length > 0 ? formatPercent(points[pointIndex].x) : ""
        pointY.text = points.length > 0 ? formatPercent(points[pointIndex].y) : ""
        pointError = ""
    }

    function applyPoint(): bool {
        if (points.length === 0)
            return false
        if (pointX.text === formatPercent(points[pointIndex].x) && pointY.text === formatPercent(points[pointIndex].y))
            return true
        const x = parseNumber(pointX.text) / 100
        const y = parseNumber(pointY.text) / 100
        if (!(x >= 0 && x <= 1 && y >= 0 && y <= 1)) {
            pointError = qsTr("Inserisci valori tra 0 e 100.")
            return false
        }
        const minX = pointIndex > 0 ? points[pointIndex - 1].x : 0
        const maxX = pointIndex + 1 < points.length ? points[pointIndex + 1].x : 1
        if (x < minX || x > maxX || (chartKind === "bars" &&
                ((pointIndex > 0 && x - minX < 0.005) || (pointIndex + 1 < points.length && maxX - x < 0.005)))) {
            pointError = qsTr("Mantieni il punto tra i suoi vicini lungo l’asse X.")
            return false
        }
        const edited = points.slice()
        edited[pointIndex] = {x: x, y: y}
        points = edited
        selectPoint(pointIndex)
        return true
    }

    function navigatePoints(delta: int) {
        if (applyPoint())
            selectPoint(pointIndex + delta)
    }

    function addPoint() {
        if (!applyPoint() || points.length >= 128)
            return
        let slot = 0
        let largestGap = -1
        for (let index = 0; index <= points.length; ++index) {
            const left = index > 0 ? points[index - 1].x : 0
            const right = index < points.length ? points[index].x : 1
            if (right - left > largestGap) {
                largestGap = right - left
                slot = index
            }
        }
        if (chartKind === "bars" && largestGap < 0.01) {
            pointError = qsTr("Sposta un punto per fare spazio sul grafico.")
            return
        }
        const left = slot > 0 ? points[slot - 1] : {x: 0, y: points[0].y}
        const right = slot < points.length ? points[slot] : {x: 1, y: points[points.length - 1].y}
        const expanded = points.slice()
        expanded.splice(slot, 0, {x: (left.x + right.x) / 2, y: (left.y + right.y) / 2})
        points = expanded
        selectPoint(slot)
    }

    function removePoint() {
        if (points.length <= (chartKind === "bars" ? 1 : 2))
            return
        const remaining = points.slice()
        remaining.splice(pointIndex, 1)
        points = remaining
        selectPoint(pointIndex)
    }

    function hideKeyboard() {
        keyboardTarget = null
        contentItem.forceActiveFocus()
    }

    function copyCsv() {
        if (editingPoints && !applyPoint())
            return
        notice = recognizer.copyCsv(chart) ? qsTr("Dati CSV copiati.")
                                          : qsTr("Appunti non disponibili. Usa Salva CSV.")
    }

    function exportCsv() {
        if (editingPoints && !applyPoint())
            return
        const data = recognizer.csv(chart)
        if (data.length > 0)
            exportCsvRequested(data)
    }

    component ChartButton: PaperButton {
        uiScale: editor.uiScale
    }

    component ChartLabel: Label {
        color: editor.ink
        textFormat: Text.PlainText
        font.pixelSize: 14 * editor.uiScale
        wrapMode: Text.WordWrap
    }

    component ChartField: TextField {
        id: field

        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredHeight: 44 * editor.uiScale
        color: editor.ink
        placeholderTextColor: editor.muted
        font.pixelSize: 15 * editor.uiScale
        maximumLength: 40
        leftPadding: 9 * editor.uiScale
        rightPadding: 9 * editor.uiScale
        selectByMouse: true
        Accessible.name: placeholderText
        onActiveFocusChanged: if (activeFocus && editor.tabletMode) editor.keyboardTarget = field

        background: Rectangle {
            color: "#f4f5f7"
            border.color: field.activeFocus ? editor.blue : editor.border
            border.width: field.activeFocus ? 2 : 1
            radius: 8 * editor.uiScale
        }
    }

    background: Rectangle {
        color: "white"
        border.color: editor.border
        radius: 14 * editor.uiScale
    }

    header: ChartLabel {
        text: editor.title
        padding: editor.padding
        bottomPadding: 0
        font.pixelSize: 22 * editor.uiScale
        font.weight: Font.DemiBold
    }

    contentItem: ScrollView {
        id: scroll

        clip: true
        contentWidth: availableWidth
        contentHeight: form.implicitHeight
        implicitHeight: Math.min(form.implicitHeight, Math.max(100 * editor.uiScale,
            (editor.parent ? editor.parent.height : 580) - 145 * editor.uiScale
            - (editor.keyboardOpen ? keyboardPanel.implicitHeight : 0)))
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            id: form

            width: scroll.availableWidth
            spacing: 8 * editor.uiScale

            ChartLabel {
                visible: editor.recognitionError.length > 0
                Layout.fillWidth: true
                text: editor.recognitionError
                color: "#87332c"
            }

            ColumnLayout {
                visible: editor.points.length > 0
                Layout.fillWidth: true
                spacing: 8 * editor.uiScale

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 170 * editor.uiScale
                    color: "white"
                    border.color: editor.border
                    radius: 8 * editor.uiScale

                    ChartItem {
                        anchors.fill: parent
                        anchors.margins: 5 * editor.uiScale
                        chart: editor.chart
                    }
                }

                ChartField {
                    id: chartTitle
                    maximumLength: 80
                    placeholderText: qsTr("Titolo del grafico")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6 * editor.uiScale

                    ChartField {
                        id: labelX
                        placeholderText: qsTr("Etichetta asse X")
                    }

                    ChartField {
                        id: labelY
                        placeholderText: qsTr("Etichetta asse Y")
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 5 * editor.uiScale

                    ComboBox {
                        visible: editor.fromLibrary || editor.existing
                        model: [qsTr("Linee"), qsTr("Barre")]
                        Accessible.name: qsTr("Tipo di grafico")
                        currentIndex: editor.chartKind === "bars" ? 1 : 0
                        onActivated: {
                            const kind = index === 1 ? "bars" : "line"
                            if (kind === "bars" && editor.points.some((point, i) => i > 0 && point.x === editor.points[i - 1].x)) {
                                editor.editingPoints = true
                                editor.pointError = qsTr("Usa valori X distinti per le barre.")
                                currentIndex = editor.chartKind === "bars" ? 1 : 0
                                return
                            }
                            editor.chartKind = kind
                        }
                    }

                    ChartButton {
                        text: editor.editingPoints ? qsTr("Chiudi punti") : qsTr("Modifica punti")
                        onClicked: {
                            if (editor.editingPoints && !editor.applyPoint())
                                return
                            editor.editingPoints = !editor.editingPoints
                            if (!editor.editingPoints)
                                editor.hideKeyboard()
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Switch {
                        id: calibrated
                        text: qsTr("Scala numerica")
                        font.pixelSize: 14 * editor.uiScale
                        onToggled: if (!checked) editor.hideKeyboard()
                    }
                }

                ColumnLayout {
                    visible: editor.editingPoints
                    Layout.fillWidth: true
                    spacing: 5 * editor.uiScale

                    ChartLabel {
                        Layout.fillWidth: true
                        color: editor.muted
                        text: qsTr("Punto %1 di %2 · coordinate in %").arg(editor.pointIndex + 1).arg(editor.points.length)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5 * editor.uiScale

                        ChartButton {
                            text: qsTr("Punto precedente")
                            iconName: "left"
                            iconOnly: true
                            enabled: editor.pointIndex > 0
                            onClicked: editor.navigatePoints(-1)
                        }

                        ChartField {
                            id: pointX
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            placeholderText: qsTr("X %")
                            onAccepted: editor.applyPoint()
                        }

                        ChartField {
                            id: pointY
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            placeholderText: qsTr("Y %")
                            onAccepted: editor.applyPoint()
                        }

                        ChartButton {
                            text: qsTr("Punto successivo")
                            iconName: "right"
                            iconOnly: true
                            enabled: editor.pointIndex + 1 < editor.points.length
                            onClicked: editor.navigatePoints(1)
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 5 * editor.uiScale

                        ChartButton {
                            text: qsTr("Applica punto")
                            onClicked: editor.applyPoint()
                        }
                        ChartButton {
                            text: qsTr("Aggiungi punto")
                            enabled: editor.points.length < 128
                            onClicked: editor.addPoint()
                        }
                        ChartButton {
                            text: qsTr("Rimuovi punto")
                            enabled: editor.points.length > (editor.chartKind === "bars" ? 1 : 2)
                            onClicked: editor.removePoint()
                        }
                    }

                    ChartLabel {
                        Layout.fillWidth: true
                        visible: editor.pointError.length > 0
                        text: editor.pointError
                        color: "#87332c"
                    }
                }

                ColumnLayout {
                    visible: calibrated.checked
                    Layout.fillWidth: true
                    spacing: 5 * editor.uiScale

                    ChartLabel {
                        Layout.fillWidth: true
                        text: qsTr("Imposta il minimo e il massimo degli assi. Il minimo può essere negativo o zero; il massimo deve essere positivo.")
                        color: editor.muted
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6 * editor.uiScale

                        ChartField {
                            id: minimumX
                            objectName: "chartMinimumX"
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            placeholderText: qsTr("Minimo X")
                            Accessible.name: placeholderText
                        }

                        ChartField {
                            id: maximumX
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            placeholderText: qsTr("Massimo X")
                        }

                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6 * editor.uiScale
                        ChartField {
                            id: minimumY
                            objectName: "chartMinimumY"
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            placeholderText: qsTr("Minimo Y")
                            Accessible.name: placeholderText
                        }

                        ChartField {
                            id: maximumY
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            placeholderText: qsTr("Massimo Y")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6 * editor.uiScale

                        Item { Layout.fillWidth: true }

                        ChartButton {
                            text: qsTr("Copia CSV")
                            enabled: editor.validScale
                            onClicked: editor.copyCsv()
                        }

                        ChartButton {
                            text: qsTr("Salva CSV")
                            enabled: editor.validScale
                            onClicked: editor.exportCsv()
                        }
                    }
                }

            }

            ChartLabel {
                Layout.fillWidth: true
                visible: editor.notice.length > 0
                text: editor.notice
                color: editor.blue
            }

            ChartLabel {
                Layout.fillWidth: true
                visible: editor.errorText.length > 0
                text: editor.errorText
                color: "#87332c"
            }
        }
    }

    footer: ColumnLayout {
        spacing: 5 * editor.uiScale

        CheckBox {
            id: newPage
            Layout.leftMargin: editor.padding
            Layout.rightMargin: editor.padding
            visible: !editor.existing && !editor.fromLibrary && editor.points.length > 0 && !editor.keyboardOpen
            text: qsTr("Salva su una nuova pagina")
            font.pixelSize: 14 * editor.uiScale
        }

        ColumnLayout {
            id: keyboardPanel

            Layout.fillWidth: true
            Layout.leftMargin: editor.padding
            Layout.rightMargin: editor.padding
            visible: editor.keyboardOpen
            spacing: 3 * editor.uiScale

            RowLayout {
                Layout.fillWidth: true

                ChartLabel {
                    Layout.fillWidth: true
                    text: editor.keyboardTarget ? editor.keyboardTarget.text || editor.keyboardTarget.placeholderText : ""
                    wrapMode: Text.NoWrap
                    elide: Text.ElideRight
                }

                ChartButton {
                    text: qsTr("Fine")
                    quiet: true
                    focusPolicy: Qt.NoFocus
                    onClicked: editor.hideKeyboard()
                }
            }

            TouchKeyboard {
                Layout.fillWidth: true
                uiScale: Math.max(0.68, editor.uiScale * 0.74)
                target: editor.keyboardTarget
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: editor.padding
            Layout.rightMargin: editor.padding
            Layout.bottomMargin: editor.padding
            spacing: 6 * editor.uiScale

            ChartButton {
                visible: editor.existing
                text: editor.confirmingDelete ? qsTr("Conferma") : qsTr("Elimina")
                onClicked: {
                    if (editor.confirmingDelete)
                        editor.deleteRequested(editor.originalItem.id)
                    else
                        editor.confirmingDelete = true
                }
            }

            Item { Layout.fillWidth: true }

            ChartButton {
                text: editor.points.length > 0 ? qsTr("Annulla") : qsTr("Chiudi")
                quiet: true
                onClicked: {
                    if (editor.confirmingDelete)
                        editor.confirmingDelete = false
                    else
                        editor.close()
                }
            }

            ChartButton {
                visible: editor.points.length > 0
                text: qsTr("Salva")
                filled: true
                enabled: !editor.confirmingDelete && (!calibrated.checked || editor.validScale)
                onClicked: {
                    if (editor.editingPoints && !editor.applyPoint())
                        return
                    if (!editor.existing && newPage.checked)
                        editor.saveOnNewPageRequested(editor.chart)
                    else
                        editor.saveRequested(editor.chart)
                }
            }
        }
    }
}
