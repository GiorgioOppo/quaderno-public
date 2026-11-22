import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: viewer
    objectName: "screenStreamWindow"

    property var stream: null
    signal connectionRequested()

    readonly property bool showingFrame: visible && Boolean(stream?.active)
                                         && frame.status === Image.Ready
    readonly property bool navigationAllowed: Boolean(stream?.active && stream.remoteNavigationAllowed)
    readonly property string confirmedSection: navigationAllowed ? stream.remoteSection : ""
    readonly property bool canTapFrame: showingFrame && confirmedSection.length > 0
    readonly property int frameOrientation: stream?.frameOrientation ?? 0
    readonly property bool sideways: frameOrientation === 90 || frameOrientation === 270
    readonly property real frameAspect: {
        const size = frame.sourceSize
        if (size.width <= 0 || size.height <= 0)
            return 0
        return sideways ? size.height / size.width : size.width / size.height
    }
    property real fittedAspect: 0

    title: qsTr("Schermo del Paper Pro")
    transientParent: null
    flags: Qt.Window
    visible: false
    width: 480
    height: 688
    minimumWidth: 240
    minimumHeight: 228
    color: "#202024"

    onVisibleChanged: {
        if (stream) stream.setViewerOpen(visible)
        if (!visible) fittedAspect = 0
        else Qt.callLater(viewer.fitNewFrame)
    }
    onConfirmedSectionChanged: frame.focus = false
    onShowingFrameChanged: Qt.callLater(viewer.fitNewFrame)
    onFrameAspectChanged: Qt.callLater(viewer.fitNewFrame)
    onScreenChanged: Qt.callLater(viewer.fitOnScreenChange)

    Connections {
        target: viewer.stream
        ignoreUnknownSignals: true
        function onStateChanged() {
            if (!viewer.stream?.active) viewer.fittedAspect = 0
        }
    }

    function open() {
        const opening = !visible
        show()
        if (opening) Qt.callLater(viewer.fitOnScreenChange)
        raise()
        requestActivate()
    }

    function workArea(): rect {
        if (typeof stream?.viewerAvailableArea === "function") {
            const area = stream.viewerAvailableArea(viewer)
            if (area.width > 0 && area.height > 0) return area
        }
        const display = viewer.screen
        return Qt.rect((display?.virtualX ?? 0) + 8, (display?.virtualY ?? 0) + 38,
            Math.max(1, Math.min(display?.width || 640, display?.desktopAvailableWidth || 640) - 16),
            Math.max(1, Math.min(display?.height || 820, display?.desktopAvailableHeight || 820) - 54))
    }

    function fitWindow(aspect: real, area: rect) {
        const previewHeight = Math.max(1, Math.floor(Math.min(area.height - footer.implicitHeight, area.width / aspect)))
        const nextWidth = Math.max(minimumWidth, Math.round(previewHeight * aspect))
        const nextHeight = Math.max(minimumHeight, previewHeight + footer.implicitHeight)
        const centerX = x + width / 2
        const centerY = y + height / 2
        width = nextWidth
        height = nextHeight
        x = Math.round(Math.max(area.x, Math.min(area.x + area.width - nextWidth, centerX - nextWidth / 2)))
        y = Math.round(Math.max(area.y, Math.min(area.y + area.height - nextHeight, centerY - nextHeight / 2)))
    }

    function fitNewFrame() {
        if (!showingFrame || visibility !== Window.Windowed || frameAspect <= 0
            || Math.abs(fittedAspect - frameAspect) < 0.002)
            return
        fittedAspect = frameAspect
        fitWindow(frameAspect, workArea())
    }

    function fitOnScreenChange() {
        if (!visible || visibility !== Window.Windowed) return
        const area = workArea()
        if (width > area.width || height > area.height)
            fitWindow(frameAspect || 0.75, area)
    }

    function keyboardAction(event): string {
        if (event.matches(StandardKey.SelectAll))
            return "select-all"
        if (event.modifiers & (Qt.ControlModifier | Qt.MetaModifier))
            return ""

        if (!(event.modifiers & (Qt.ShiftModifier | Qt.AltModifier))) {
            switch (event.key) {
            case Qt.Key_Backspace: return "backspace"
            case Qt.Key_Delete: return "delete"
            case Qt.Key_Left: return "left"
            case Qt.Key_Right: return "right"
            case Qt.Key_Home: return "home"
            case Qt.Key_End: return "end"
            case Qt.Key_Return:
            case Qt.Key_Enter: return "new-line"
            }
        }
        return validTypedText(event.text) ? "text" : ""
    }

    function validTypedText(text: string): bool {
        return text.length > 0 && text.length <= 256 && !/[\u0000-\u001f\u007f-\u009f]/.test(text)
    }

    TextEdit {
        id: pasteBuffer
        visible: false
        textFormat: TextEdit.PlainText
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: preview
            objectName: "screenStreamViewport"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Image {
                id: frame
                objectName: "screenStreamImage"
                anchors.centerIn: parent
                width: viewer.sideways ? preview.height : preview.width
                height: viewer.sideways ? preview.width : preview.height
                rotation: -viewer.frameOrientation
                source: viewer.visible && viewer.stream?.active ? viewer.stream.frameSource : ""
                visible: viewer.showingFrame
                fillMode: Image.PreserveAspectFit
                horizontalAlignment: Image.AlignHCenter
                verticalAlignment: Image.AlignVCenter
                cache: false
                Accessible.name: qsTr("Schermo del Paper Pro in diretta")
                Keys.onPressed: event => {
                    if (!viewer.navigationAllowed || !viewer.confirmedSection.length)
                        return
                    if (event.matches(StandardKey.Paste)) {
                        pasteBuffer.paste()
                        const text = pasteBuffer.text
                        pasteBuffer.clear()
                        if (viewer.validTypedText(text))
                            viewer.stream.requestKey("text", text)
                        event.accepted = true
                        return
                    }
                    const action = viewer.keyboardAction(event)
                    if (action) {
                        viewer.stream.requestKey(action, action === "text" ? event.text : "")
                        event.accepted = true
                    }
                }

                MouseArea {
                    objectName: "screenStreamTapArea"
                    width: frame.paintedWidth
                    height: frame.paintedHeight
                    x: (frame.width - width) / 2
                    y: (frame.height - height) / 2
                    enabled: viewer.canTapFrame
                    acceptedButtons: Qt.LeftButton
                    onClicked: mouse => {
                        if (width > 0 && height > 0 && contains(Qt.point(mouse.x, mouse.y))) {
                            frame.forceActiveFocus(Qt.MouseFocusReason)
                            viewer.stream.requestTap(mouse.x / width, mouse.y / height)
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, 390)
                visible: !viewer.showingFrame
                text: viewer.stream?.active ? qsTr("In attesa della prima immagine…")
                    : viewer.stream?.available ? qsTr("Sul Paper Pro, premi Condividi schermo via Wi-Fi.")
                    : viewer.stream?.status || qsTr("Collega il Paper Pro e il Mac alla stessa rete usando Sincronizzazione wireless.")
                color: "#f2f2f5"
                font.pixelSize: 17
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Rectangle {
            id: footer
            Layout.fillWidth: true
            implicitHeight: 48
            color: "#f5f5f7"

            RowLayout {
                id: viewerActions
                anchors.fill: parent
                anchors.margins: 6
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: viewer.stream?.active && !viewer.navigationAllowed ? qsTr("Sola visualizzazione")
                        : viewer.navigationAllowed && !viewer.confirmedSection.length ? qsTr("Comandi in pausa sul Paper Pro.")
                        : viewer.canTapFrame ? qsTr("Fai clic sui comandi o su un campo per digitare.") : ""
                    color: "#3c3c43"
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                Button {
                    objectName: "screenStreamViewerConnectionButton"
                    visible: viewer.stream !== null && !viewer.stream.available && !viewer.stream.active
                    text: qsTr("Collegamento Wi-Fi…")
                    onClicked: viewer.connectionRequested()
                }

                Button {
                    objectName: "screenStreamViewerStopButton"
                    text: viewer.stream?.active ? qsTr("Interrompi") : qsTr("Chiudi")
                    onClicked: viewer.close()
                }
            }
        }
    }
}
