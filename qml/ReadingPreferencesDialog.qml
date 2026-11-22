pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: dialog
    objectName: "readingPreferencesDialog"
    required property var notebook
    required property var preferences
    required property real uiScale
    property bool tabletMode: Qt.platform.os !== "osx"
    property string sourceBookId: ""
    property int zoom: 100
    property int draftMargins: 0
    property int orientation: -1
    property string swipe: "pages"
    property bool resetRequested: false
    property string errorText: ""

    parent: Overlay.overlay
    anchors.centerIn: parent
    title: qsTr("Lettura di questo libro")
    width: Math.min(510 * uiScale, (parent ? parent.width : 550 * uiScale) - 24 * uiScale)
    padding: 16 * uiScale
    modal: true
    dim: !tabletMode
    background: Rectangle {
        color: "white"
        radius: 16 * dialog.uiScale
        border.color: "#a4a4ac"
        border.width: Math.max(1, dialog.uiScale)
    }
    header: Label {
        text: dialog.title
        textFormat: Text.PlainText
        font.pixelSize: 22 * dialog.uiScale
        font.weight: Font.DemiBold
        padding: dialog.padding
        bottomPadding: 10 * dialog.uiScale
        wrapMode: Text.Wrap
    }

    function loadDefaults() {
        zoom = preferences.readingZoom
        draftMargins = preferences.readingMargins
        orientation = preferences.readingOrientation
        swipe = preferences.swipeAction
    }
    function begin() {
        if (!notebook.bookId.length) return
        sourceBookId = notebook.bookId
        loadDefaults()
        const saved = notebook.localReadingPreferences
        if (saved.readingZoom !== undefined) zoom = saved.readingZoom
        if (saved.readingMargins !== undefined) draftMargins = saved.readingMargins
        if (saved.readingOrientation !== undefined) orientation = saved.readingOrientation
        if (saved.swipeAction !== undefined) swipe = saved.swipeAction
        errorText = ""
        resetRequested = false
        open()
    }
    function save() {
        if (sourceBookId !== notebook.bookId || !sourceBookId.length) {
            errorText = qsTr("Il libro aperto è cambiato. Riapri le preferenze di lettura.")
            return
        }
        const saved = resetRequested ? notebook.clearReadingPreferences()
            : notebook.setReadingPreferences({ readingZoom: zoom, readingMargins: draftMargins,
                readingOrientation: orientation, swipeAction: swipe })
        if (saved) close()
        else errorText = notebook.error
    }
    component Body: Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        font.pixelSize: 15 * dialog.uiScale
        font.bold: dialog.preferences.boldLabels
        color: dialog.preferences.highContrast ? "black" : "#1c1c1e"
    }
    component Action: PaperButton {
        uiScale: dialog.uiScale
        hoverEnabled: !dialog.tabletMode
    }
    component Choice: ColumnLayout {
        id: choice
        required property string field
        required property string label
        required property var choices
        Layout.fillWidth: true
        spacing: 4 * dialog.uiScale
        Body { text: choice.label }
        ComboBox {
            objectName: "bookReading_" + choice.field
            Layout.fillWidth: true
            Layout.minimumHeight: 44 * dialog.uiScale
            font.pixelSize: 15 * dialog.uiScale
            font.bold: dialog.preferences.boldLabels
            model: choice.choices
            textRole: "label"
            valueRole: "value"
            currentIndex: choice.choices.findIndex(option => option.value === dialog[choice.field])
            Accessible.name: choice.label
            onActivated: { dialog[choice.field] = currentValue; dialog.resetRequested = false }
        }
    }
    contentItem: ScrollView {
        id: scroll
        clip: true
        implicitHeight: Math.min(content.implicitHeight, Math.max(100 * dialog.uiScale,
            (dialog.parent ? dialog.parent.height : 800 * dialog.uiScale) - 180 * dialog.uiScale))
        contentWidth: availableWidth
        contentHeight: content.implicitHeight
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            id: content
            width: scroll.availableWidth
            spacing: 12 * dialog.uiScale
            Body { text: qsTr("Queste preferenze valgono solo per questo libro su questo dispositivo. Non modificano il documento o le annotazioni.") }
            Body { text: qsTr("Zoom iniziale") }
            SpinBox {
                objectName: "bookReading_zoom"
                Layout.fillWidth: true
                Layout.minimumHeight: 44 * dialog.uiScale
                from: 100
                to: 300
                stepSize: 1
                value: dialog.zoom
                editable: false
                font.pixelSize: 16 * dialog.uiScale
                Accessible.name: qsTr("Zoom del libro, da 100 a 300 per cento")
                textFromValue: value => value + "%"
                onValueModified: { dialog.zoom = value; dialog.resetRequested = false }
            }
            Flow {
                Layout.fillWidth: true
                spacing: 5 * dialog.uiScale
                Repeater {
                    model: [100, 125, 150, 200, 300]
                    Action {
                        required property int modelData
                        objectName: "bookZoom_" + modelData
                        text: modelData + "%"
                        filled: dialog.zoom === modelData
                        onClicked: { dialog.zoom = modelData; dialog.resetRequested = false }
                    }
                }
            }
            Choice { field: "draftMargins"; label: qsTr("Margini"); choices: [{ value: 0, label: qsTr("Nessuno") }, { value: 8, label: qsTr("Piccoli") }, { value: 16, label: qsTr("Medi") }, { value: 24, label: qsTr("Grandi") }] }
            Choice { field: "orientation"; label: qsTr("Orientamento"); enabled: dialog.tabletMode; choices: [{ value: -1, label: qsTr("Come l’app") }, { value: 0, label: qsTr("Verticale") }, { value: 90, label: qsTr("Orizzontale a destra") }, { value: 180, label: qsTr("Verticale capovolto") }, { value: 270, label: qsTr("Orizzontale a sinistra") }] }
            Choice { field: "swipe"; label: qsTr("Scorrimento"); choices: [{ value: "pages", label: qsTr("Cambia pagina") }, { value: "scroll", label: qsTr("Scorrimento verticale") }, { value: "none", label: qsTr("Nessuna azione") }] }
            Action {
                objectName: "resetBookReadingPreferences"
                text: qsTr("Usa le impostazioni generali")
                onClicked: { dialog.loadDefaults(); dialog.resetRequested = true; dialog.errorText = "" }
            }
            Body { visible: dialog.resetRequested; text: qsTr("Premi Applica per rimuovere le preferenze di questo libro e usare quelle generali.") }
            Body { visible: dialog.errorText.length > 0; text: dialog.errorText; color: "#942922" }
        }
    }
    footer: RowLayout {
        spacing: 6 * dialog.uiScale
        Item { Layout.fillWidth: true }
        Action { Layout.bottomMargin: dialog.padding; objectName: "cancelBookReadingPreferences"; text: qsTr("Annulla"); onClicked: dialog.close() }
        Action { Layout.bottomMargin: dialog.padding; Layout.rightMargin: dialog.padding; objectName: "saveBookReadingPreferences"; text: qsTr("Applica"); filled: true; onClicked: dialog.save() }
    }
}
