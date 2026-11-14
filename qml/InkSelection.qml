pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: editor

    required property real uiScale
    property bool tabletMode: false
    property var selectedStrokes: []
    property var selectedIndexes: []
    property bool pasting: false
    property string errorText: ""
    property var pageSnapshot: ({})
    property point requestedOffset: Qt.point(0, 0)
    property real scaleValue: 1
    property real angleValue: 0

    readonly property real borderWidth: Math.max(1, Math.round(uiScale))
    readonly property color blue: "#155cc2"
    readonly property var previewContent: {
        const content = pageSnapshot
        const excluded = new Set(selectedIndexes)
        return {
            paper: content.paper || "blank",
            background: content.background || "",
            items: content.items || [],
            strokes: (content.strokes || []).filter((stroke, index) => pasting || !ink.selectionSupported || !excluded.has(index))
        }
    }

    signal transformAccepted(real pivotX, real pivotY, real dx, real dy, real scale, real angle)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(590 * uiScale, (parent ? parent.width : 590 * uiScale) - 20 * uiScale)
    height: Math.min(680 * uiScale, (parent ? parent.height : 680 * uiScale) - 20 * uiScale)
    padding: 12 * uiScale
    spacing: 6 * uiScale
    title: pasting ? qsTr("Incolla tratti") : qsTr("Sposta e trasforma")
    modal: true
    dim: !tabletMode
    focus: true
    closePolicy: Popup.CloseOnEscape

    function openFor(strokes: var, paste: bool, pageContent: var, indexes: var) {
        pasting = paste
        selectedStrokes = strokes
        selectedIndexes = indexes || []
        pageSnapshot = pageContent || ({})
        resetTransform()
        open()
    }

    function resetTransform() {
        errorText = ""
        scaleValue = 1
        angleValue = 0
        requestedOffset = pasting ? Qt.point(0.5 - ink.pivot.x, 0.5 - ink.pivot.y) : Qt.point(0, 0)
    }

    function nudge(x: real, y: real) {
        errorText = ""
        requestedOffset = Qt.point(ink.appliedOffset.x + x, ink.appliedOffset.y + y)
    }

    function turn(direction: int) {
        errorText = ""
        const amount = ink.quarterTurnsOnly ? 90 : 15
        angleValue = (angleValue + direction * amount + 360) % 360
    }

    function resizeBy(factor: real) {
        errorText = ""
        scaleValue = Math.max(0.15, Math.min(6, scaleValue * factor))
    }

    function apply() {
        if (ink.valid)
            transformAccepted(ink.pivot.x, ink.pivot.y, ink.appliedOffset.x, ink.appliedOffset.y, scaleValue, angleValue)
    }

    onOpened: placement.forceActiveFocus()
    onClosed: {
        selectedIndexes = []
        selectedStrokes = []
        pageSnapshot = ({})
    }

    background: Rectangle {
        color: "#ffffff"
        radius: 16 * editor.uiScale
        border.color: "#d9dce2"
        border.width: editor.borderWidth
    }

    header: Label {
        text: editor.title
        color: "#24262c"
        font.pixelSize: 21 * editor.uiScale
        font.weight: Font.DemiBold
        topPadding: 13 * editor.uiScale
        leftPadding: 14 * editor.uiScale
        rightPadding: leftPadding
    }

    contentItem: ColumnLayout {
        spacing: 6 * editor.uiScale

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Trascina nell’anteprima per posizionare i tratti.")
            color: "#555b67"
            font.pixelSize: 13 * editor.uiScale
        }

        Item {
            id: pageArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 65 * editor.uiScale

            Rectangle {
                id: paper
                width: Math.min(pageArea.width, pageArea.height * 3 / 4)
                height: width * 4 / 3
                anchors.centerIn: parent
                border.color: "#c6ccd6"
                border.width: editor.borderWidth
                color: "#ffffff"
                clip: true

                PagePreviewItem {
                    anchors.fill: parent
                    content: editor.previewContent
                }

                InkSelectionPreview {
                    id: ink
                    width: paper.width
                    height: paper.height
                    x: drawOffset.x * width
                    y: drawOffset.y * height
                    strokes: editor.selectedStrokes
                    scaleFactor: editor.scaleValue
                    angle: editor.angleValue
                    offset: editor.requestedOffset
                }

                Rectangle {
                    visible: ink.valid
                    x: Math.max(1, ink.bounds.x * paper.width - 3 * editor.uiScale)
                    y: Math.max(1, ink.bounds.y * paper.height - 3 * editor.uiScale)
                    width: Math.min(paper.width - x - 1, Math.max(2 * editor.borderWidth, ink.bounds.width * paper.width + 6 * editor.uiScale))
                    height: Math.min(paper.height - y - 1, Math.max(2 * editor.borderWidth, ink.bounds.height * paper.height + 6 * editor.uiScale))
                    color: "transparent"
                    radius: 3 * editor.uiScale
                    border.color: editor.blue
                    border.width: editor.borderWidth
                }

                MouseArea {
                    id: placement
                    anchors.fill: parent
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    activeFocusOnTab: true
                    property point pressPosition
                    property point pressOffset
                    property bool dragged: false

                    onPressed: mouse => {
                        forceActiveFocus()
                        pressPosition = Qt.point(mouse.x, mouse.y)
                        pressOffset = ink.appliedOffset
                        dragged = false
                    }
                    onPositionChanged: mouse => {
                        const dx = mouse.x - pressPosition.x
                        const dy = mouse.y - pressPosition.y
                        if (pressed && (dragged || Math.hypot(dx, dy) >= 4 * editor.uiScale)) {
                            dragged = true
                            editor.errorText = ""
                            editor.requestedOffset = Qt.point(pressOffset.x + dx / width, pressOffset.y + dy / height)
                        }
                    }
                    onReleased: mouse => {
                        if (editor.pasting && !dragged && ink.valid) {
                            const desired = Qt.point(mouse.x / width, mouse.y / height)
                            editor.nudge(desired.x - (ink.bounds.x + ink.bounds.width / 2),
                                         desired.y - (ink.bounds.y + ink.bounds.height / 2))
                        }
                    }

                    Keys.onLeftPressed: editor.nudge(-0.015, 0)
                    Keys.onRightPressed: editor.nudge(0.015, 0)
                    Keys.onUpPressed: editor.nudge(0, -0.015)
                    Keys.onDownPressed: editor.nudge(0, 0.015)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 3 * editor.uiScale

            PaperButton {
                uiScale: editor.uiScale
                text: "−"
                Accessible.name: qsTr("Riduci")
                leftPadding: 7 * editor.uiScale
                rightPadding: leftPadding
                enabled: editor.scaleValue > 0.15
                ToolTip.text: qsTr("Riduci")
                ToolTip.visible: !editor.tabletMode && hovered
                onClicked: editor.resizeBy(1 / 1.15)
            }

            Label {
                Layout.minimumWidth: 48 * editor.uiScale
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("%1 %").arg(Math.round(editor.scaleValue * 100))
                color: "#24262c"
                font.pixelSize: 13 * editor.uiScale
            }

            PaperButton {
                uiScale: editor.uiScale
                text: qsTr("Ingrandisci")
                iconName: "plus"
                iconOnly: true
                leftPadding: 7 * editor.uiScale
                rightPadding: leftPadding
                enabled: editor.scaleValue < 6
                ToolTip.text: text
                ToolTip.visible: !editor.tabletMode && hovered
                onClicked: editor.resizeBy(1.15)
            }

            Item { Layout.fillWidth: true }

            PaperButton {
                uiScale: editor.uiScale
                text: qsTr("Ruota a sinistra")
                iconName: "undo"
                iconOnly: true
                leftPadding: 7 * editor.uiScale
                rightPadding: leftPadding
                ToolTip.text: text
                ToolTip.visible: !editor.tabletMode && hovered
                onClicked: editor.turn(-1)
            }

            Label {
                Layout.minimumWidth: 38 * editor.uiScale
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("%1°").arg(editor.angleValue)
                color: "#24262c"
                font.pixelSize: 13 * editor.uiScale
            }

            PaperButton {
                uiScale: editor.uiScale
                text: qsTr("Ruota a destra")
                iconName: "redo"
                iconOnly: true
                leftPadding: 7 * editor.uiScale
                rightPadding: leftPadding
                ToolTip.text: text
                ToolTip.visible: !editor.tabletMode && hovered
                onClicked: editor.turn(1)
            }
        }

        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            wrapMode: Text.WordWrap
            font.pixelSize: 13 * editor.uiScale
            color: editor.errorText || !ink.valid ? "#8c2d27" : "#555b67"
            text: editor.errorText || (!ink.selectionSupported ? qsTr("Non è possibile trasformare questa selezione. Prova a selezionare meno tratti.")
                                    : !ink.valid ? qsTr("La selezione non entra nella pagina a questa dimensione. Prova a ridurla.")
                                    : ink.quarterTurnsOnly ? qsTr("Le forme intelligenti ruotano di 90° per conservare il tratto originale.") : "")
        }
    }

    footer: RowLayout {
        spacing: 4 * editor.uiScale

        PaperButton {
            Layout.leftMargin: editor.padding
            Layout.bottomMargin: editor.padding
            uiScale: editor.uiScale
            text: qsTr("Ripristina")
            quiet: true
            onClicked: editor.resetTransform()
        }

        Item { Layout.fillWidth: true }

        PaperButton {
            Layout.bottomMargin: editor.padding
            uiScale: editor.uiScale
            text: qsTr("Annulla")
            quiet: true
            onClicked: editor.close()
        }

        PaperButton {
            Layout.rightMargin: editor.padding
            Layout.bottomMargin: editor.padding
            uiScale: editor.uiScale
            text: editor.pasting ? qsTr("Incolla") : qsTr("Applica")
            enabled: ink.valid
            filled: true
            onClicked: editor.apply()
        }
    }
}
