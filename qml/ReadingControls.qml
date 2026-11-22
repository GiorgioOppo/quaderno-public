import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

GridLayout {
    id: controls
    objectName: "readingControls"

    required property real uiScale
    property bool tabletMode: false
    property bool bookmark: false
    property real zoom: 1
    property bool fitWidth: false

    signal toggleBookmark()
    signal bookmarksRequested()
    signal settingsRequested()
    signal zoomRequested(real zoom, bool fitWidth)

    readonly property bool compact: width < 600 * uiScale
    readonly property bool twoRows: width < 370 * uiScale
    readonly property real minimumZoom: 1
    readonly property real maximumZoom: 3
    readonly property real step: 0.25
    Layout.minimumWidth: 0
    columns: twoRows ? 4 : 8
    columnSpacing: 4 * uiScale
    rowSpacing: 2 * uiScale

    function changeZoom(delta: real) {
        zoomRequested(Math.min(maximumZoom, Math.max(minimumZoom, zoom + delta)), fitWidth)
    }

    component ViewButton: PaperButton {
        uiScale: controls.uiScale
        ToolTip.visible: !controls.tabletMode && hovered && tooltip.length > 0
        ToolTip.text: tooltip
        property string tooltip: text
    }

    ViewButton {
        text: "−"
        tooltip: qsTr("Riduci zoom")
        Accessible.name: tooltip
        enabled: controls.zoom > controls.minimumZoom
        onClicked: controls.changeZoom(-controls.step)
    }

    ViewButton {
        text: qsTr("%1%").arg(Math.round(controls.zoom * 100))
        tooltip: qsTr("Adatta alla pagina")
        Accessible.name: tooltip
        Layout.minimumWidth: 60 * controls.uiScale
        quiet: true
        enabled: controls.zoom !== controls.minimumZoom || controls.fitWidth
        onClicked: controls.zoomRequested(1, false)
    }

    ViewButton {
        text: qsTr("Aumenta zoom")
        iconName: "plus"
        iconOnly: true
        enabled: controls.zoom < controls.maximumZoom
        onClicked: controls.changeZoom(controls.step)
    }

    ViewButton {
        text: qsTr("Adatta alla larghezza")
        iconName: "expand"
        iconOnly: controls.compact
        filled: controls.fitWidth
        onClicked: controls.zoomRequested(1, !controls.fitWidth)
    }

    ViewButton {
        objectName: "readingPreferencesButton"
        text: qsTr("Preferenze di lettura")
        iconName: "settings"
        iconOnly: true
        onClicked: controls.settingsRequested()
    }

    Item { visible: !controls.twoRows; Layout.fillWidth: true }

    ViewButton {
        text: controls.bookmark ? qsTr("Rimuovi segnalibro") : qsTr("Aggiungi segnalibro")
        iconName: controls.bookmark ? "star-filled" : "star"
        iconOnly: true
        filled: controls.bookmark
        onClicked: controls.toggleBookmark()
    }

    ViewButton {
        objectName: "readingBookmarksButton"
        text: qsTr("Segnalibri")
        iconName: "notebook"
        iconOnly: controls.compact
        onClicked: controls.bookmarksRequested()
    }
}
