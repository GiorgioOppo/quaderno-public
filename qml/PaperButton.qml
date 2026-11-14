import QtQuick
import QtQuick.Controls

Button {
    id: control

    required property real uiScale
    property bool filled: false
    property bool quiet: false
    property string iconName: ""
    property bool iconOnly: false
    property real iconSize: 18 * uiScale
    property bool alignLeft: false
    readonly property color labelColor: !enabled ? "#636366" : filled ? "#ffffff" : "#1c1c1e"

    font.pixelSize: 14 * uiScale
    font.weight: Font.DemiBold
    leftPadding: (iconOnly ? 11 : 14) * uiScale
    rightPadding: leftPadding
    topPadding: 8 * uiScale
    bottomPadding: 8 * uiScale
    implicitWidth: Math.max(44 * uiScale, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(44 * uiScale, implicitContentHeight + topPadding + bottomPadding)

    contentItem: Item {
        id: content
        readonly property bool showsGlyph: control.iconName.length > 0
        readonly property bool showsLabel: control.text.length > 0 && !control.iconOnly

        implicitWidth: (showsGlyph ? glyph.width : 0) + (showsLabel ? label.implicitWidth : 0)
                       + (showsGlyph && showsLabel ? row.spacing : 0)
        implicitHeight: Math.max(showsGlyph ? glyph.height : 0, showsLabel ? label.implicitHeight : 0)

        Row {
            id: row
            x: control.alignLeft ? 0 : (parent.width - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8 * control.uiScale

            PaperIcon {
                id: glyph
                anchors.verticalCenter: parent.verticalCenter
                name: control.iconName
                tint: control.labelColor
                visible: content.showsGlyph
                width: control.iconSize
                height: width
            }

            Text {
                id: label
                anchors.verticalCenter: parent.verticalCenter
                visible: content.showsLabel
                width: Math.max(0, Math.min(implicitWidth, control.availableWidth
                           - (content.showsGlyph ? glyph.width + row.spacing : 0)))
                text: control.text
                textFormat: Text.PlainText
                font: control.font
                color: control.labelColor
                elide: Text.ElideRight
            }
        }
    }

    background: Rectangle {
        readonly property bool primary: control.filled && control.enabled
        readonly property bool bare: control.quiet && !control.filled

        color: !control.enabled ? (bare ? "transparent" : "#f2f2f5")
            : primary ? (control.down ? "#00458f" : control.hovered ? "#004fa8" : "#0059bd")
            : control.down ? "#dedee3"
            : control.hovered || control.visualFocus ? "#eaeaf0"
            : bare ? "transparent" : "#f6f6f8"
        border.color: control.visualFocus ? (primary ? "#ffffff" : "#0059bd") : "#aaaab0"
        border.width: control.visualFocus ? Math.max(2, Math.round(2 * control.uiScale))
            : bare || primary ? 0 : Math.max(1, Math.round(control.uiScale))
        radius: 10 * control.uiScale
    }
}
