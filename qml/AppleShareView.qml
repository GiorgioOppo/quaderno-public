import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quaderno

Dialog {
    id: share
    objectName: "appleShareView"

    required property Notebook notebook
    required property AppleShare appleShare
    required property MacDocumentExchange documentExchange
    required property real uiScale
    required property bool tabletMode
    property int deliveryFilter: 0
    property string preferredDestination: "notes"
    readonly property var visibleDeliveries: appleShare.deliveries.filter(entry => {
        const finished = entry.state === "completed" || entry.state === "cancelled"
        return deliveryFilter === 1 ? !finished : deliveryFilter === 2 ? finished : true
    })

    function destinationName(destination) {
        const names = {notes: qsTr("Note"), books: qsTr("Libri"), numbers: "Numbers", keynote: "Keynote",
                       journal: qsTr("Diario"), files: qsTr("File"), share: qsTr("Condivisione macOS")}
        return names[destination] || destination
    }

    function deliveryDate(created) {
        const date = new Date(created || "")
        return isNaN(date.getTime()) ? "" : Qt.formatDateTime(date, "yyyy-MM-dd hh:mm")
    }

    readonly property color ink: "#1e2024"
    readonly property color muted: "#535862"
    readonly property color blue: "#155cc2"
    readonly property color border: "#dedfe4"
    readonly property color surface: "#f6f7f9"

    title: qsTr("Condividi quaderno")
    anchors.centerIn: Overlay.overlay
    width: Math.min(460 * uiScale, (parent ? parent.width : 460 * uiScale) - 32 * uiScale)
    padding: 18 * uiScale
    spacing: 14 * uiScale
    modal: true
    dim: !tabletMode
    focus: true

    background: Rectangle {
        color: "white"
        border.color: share.border
        radius: 16 * share.uiScale
    }

    header: Label {
        text: share.title
        textFormat: Text.PlainText
        color: share.ink
        font.pixelSize: 22 * share.uiScale
        font.weight: Font.DemiBold
        leftPadding: share.leftPadding
        rightPadding: share.rightPadding
        topPadding: share.topPadding
    }

    component Detail: Label {
        Layout.fillWidth: true
        color: share.muted
        textFormat: Text.PlainText
        font.pixelSize: 14 * share.uiScale
        wrapMode: Text.Wrap
    }

    component Destination: ItemDelegate {
        id: destination

        required property string description
        required property bool last

        Layout.fillWidth: true
        implicitHeight: implicitContentHeight + topPadding + bottomPadding
        leftPadding: 12 * share.uiScale
        rightPadding: leftPadding
        topPadding: 10 * share.uiScale
        bottomPadding: topPadding
        enabled: share.appleShare.available && !share.appleShare.busy && share.notebook.pageCount > 0
        Accessible.name: qsTr("%1. %2").arg(text).arg(description)

        background: Rectangle {
            color: destination.down ? "#e6eefb" : destination.hovered && destination.enabled ? "#eef3fc" : "transparent"
            border.color: share.blue
            border.width: destination.visualFocus ? 2 : 0
            radius: 8 * share.uiScale

            Rectangle {
                visible: !destination.last
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 12 * share.uiScale
                anchors.rightMargin: anchors.leftMargin
                height: Math.max(1, share.uiScale)
                color: share.border
            }
        }

        contentItem: RowLayout {
            spacing: 8 * share.uiScale

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3 * share.uiScale

                Detail {
                    text: destination.text
                    color: destination.enabled ? share.blue : share.muted
                    font.pixelSize: 16 * share.uiScale
                    font.weight: Font.DemiBold
                }

                Detail { text: destination.description }
            }

            PaperIcon {
                name: "right"
                tint: destination.enabled ? share.blue : share.muted
                Layout.preferredWidth: 18 * share.uiScale
                Layout.preferredHeight: Layout.preferredWidth
                Accessible.ignored: true
            }
        }
    }

    contentItem: ScrollView {
        id: shareScroll
        clip: true
        implicitHeight: Math.min(shareBody.implicitHeight, Math.max(120, (share.parent ? share.parent.height : 800) - 120 * share.uiScale))
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: shareBody
            width: shareScroll.availableWidth
            spacing: 12 * share.uiScale

            RowLayout {
                Layout.fillWidth: true
                spacing: 8 * share.uiScale

                Detail {
                    text: share.notebook.title
                    color: share.ink
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                }

                Detail {
                    Layout.fillWidth: false
                    text: share.notebook.pageCount === 1 ? qsTr("1 pagina") : qsTr("%1 pagine").arg(share.notebook.pageCount)
                }
            }

            Control {
                Layout.fillWidth: true
                visible: !share.tabletMode && share.appleShare.available
                padding: 2 * share.uiScale

                background: Rectangle {
                    color: share.surface
                    border.color: share.border
                    radius: 10 * share.uiScale
                }

                contentItem: ColumnLayout {
                    spacing: 0

                    Destination {
                        text: qsTr("Destinazione preferita: %1").arg(share.destinationName(share.preferredDestination))
                        description: qsTr("Prepara una copia da controllare prima dell’invio.")
                        last: false
                        onClicked: {
                            if (share.preferredDestination === "files") share.appleShare.savePdf(share.notebook)
                            else share.appleShare.prepareDelivery(share.notebook, share.preferredDestination)
                        }
                    }

                    Destination {
                        text: qsTr("Condividi PDF")
                        description: qsTr("Scegli Note o un’altra destinazione nel menu di macOS.")
                        last: false
                        onClicked: share.appleShare.sharePdf(share.notebook)
                    }

                    Destination {
                        text: qsTr("Salva PDF")
                        description: qsTr("Salva tutte le pagine. In Libri, scegli File → Importa per aggiungere il PDF.")
                        last: true
                        onClicked: share.appleShare.savePdf(share.notebook)
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Repeater {
                            model: [ {name: qsTr("Note"), id: "notes"}, {name: qsTr("Libri"), id: "books"},
                                {name: "Numbers", id: "numbers"}, {name: "Keynote", id: "keynote"},
                                {name: qsTr("Diario"), id: "journal"} ]
                            Button {
                                required property var modelData
                                text: modelData.name
                                enabled: !share.appleShare.busy && share.notebook.pageCount > 0
                                onClicked: share.appleShare.prepareDelivery(share.notebook, modelData.id)
                            }
                        }
                    }
                }
            }

            Control {
                visible: share.tabletMode || !share.appleShare.available
                Layout.fillWidth: true
                padding: 13 * share.uiScale

                background: Rectangle {
                    color: share.surface
                    border.color: share.border
                    radius: 10 * share.uiScale
                }

                contentItem: ColumnLayout {
                    spacing: 6 * share.uiScale

                    Detail {
                        text: qsTr("Continua sul Mac")
                        color: share.ink
                        font.weight: Font.DemiBold
                    }

                    Detail {
                        text: share.tabletMode
                              ? qsTr("Sincronizza il quaderno con Quaderno sul Mac tramite Wi‑Fi o Bluetooth, poi aprilo sul Mac e scegli la destinazione Apple.")
                              : qsTr("Apri il quaderno in Quaderno per macOS per condividerlo con Note e Libri o esportarlo in PDF.")
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: share.documentExchange.available && share.notebook.pageCount > 0
                spacing: 6 * share.uiScale

                Detail { text: qsTr("Trascina un PDF nel Finder") }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8 * share.uiScale

                    PdfDragHandle {
                        Layout.fillWidth: true
                        exchange: share.documentExchange
                        currentPage: true
                        uiScale: share.uiScale
                        text: qsTr("Pagina corrente")
                    }

                    PdfDragHandle {
                        Layout.fillWidth: true
                        exchange: share.documentExchange
                        currentPage: false
                        uiScale: share.uiScale
                        text: qsTr("Intero quaderno")
                    }
                }
            }

            Detail {
                visible: !share.tabletMode && text.length > 0
                text: share.appleShare.status
            }

            Flow {
                visible: share.appleShare.available && share.appleShare.deliveries.length > 0
                Layout.fillWidth: true
                spacing: 5 * share.uiScale
                Repeater {
                    model: [qsTr("Tutte"), qsTr("Da completare"), qsTr("Concluse")]
                    PaperButton {
                        required property string modelData
                        required property int index
                        uiScale: share.uiScale
                        text: modelData
                        filled: share.deliveryFilter === index
                        checked: share.deliveryFilter === index
                        checkable: true
                        onClicked: share.deliveryFilter = index
                    }
                }
            }
            Detail {
                visible: share.appleShare.available && share.appleShare.deliveries.length > 0
                text: share.visibleDeliveries.length ? qsTr("Esportazioni mostrate: %1 / %2").arg(share.visibleDeliveries.length).arg(share.appleShare.deliveries.length)
                                                    : qsTr("Nessuna esportazione in questa vista.")
            }
            ScrollView {
                id: deliveriesScroll
                Layout.fillWidth: true
                Layout.preferredHeight: 160 * share.uiScale
                visible: share.appleShare.available && share.appleShare.deliveries.length > 0
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: deliveriesScroll.availableWidth
                    Repeater {
                        model: share.visibleDeliveries
                        ColumnLayout {
                            id: deliveryRow
                            required property var modelData
                            Layout.fillWidth: true
                            Detail { text: modelData.title + " · " + share.destinationName(modelData.destination) }
                            Detail {
                                text: qsTr("File: %1 · %2").arg(modelData.file || "").arg((modelData.file || "").split(".").pop().toUpperCase())
                            }
                            Detail {
                                visible: text.length > 0
                                text: share.deliveryDate(modelData.created).length > 0
                                      ? qsTr("Creato il %1").arg(share.deliveryDate(modelData.created)) : ""
                            }
                            Detail {
                                text: modelData.state === "completed" ? qsTr("Importazione confermata")
                                    : modelData.state === "cancelled" ? qsTr("Annullata; file conservato")
                                    : modelData.state === "failed" ? modelData.error
                                    : qsTr("Da completare nell’app Apple")
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 5 * share.uiScale
                                Button {
                                    text: qsTr("Mostra nel Finder")
                                    enabled: !share.appleShare.busy
                                    onClicked: share.appleShare.revealDelivery(deliveryRow.modelData.id)
                                }
                                Button {
                                    visible: deliveryRow.modelData.state !== "completed" && deliveryRow.modelData.state !== "cancelled"
                                    text: qsTr("Ho importato il file")
                                    enabled: modelData.state === "awaitingConfirmation" && !share.appleShare.busy
                                    onClicked: share.appleShare.confirmDelivery(modelData.id)
                                }
                                Button {
                                    visible: deliveryRow.modelData.state !== "completed" && deliveryRow.modelData.state !== "cancelled"
                                    text: qsTr("Riapri")
                                    enabled: !share.appleShare.busy
                                    onClicked: share.appleShare.retryDelivery(modelData.id)
                                }
                                Button {
                                    visible: deliveryRow.modelData.state !== "completed" && deliveryRow.modelData.state !== "cancelled"
                                    text: qsTr("Annulla")
                                    enabled: !share.appleShare.busy
                                    onClicked: share.appleShare.cancelDelivery(modelData.id)
                                }
                            }
                        }
                    }
                }
            }

            Detail {
                visible: share.documentExchange.available && text.length > 0
                text: share.documentExchange.status
            }

            Button {
                visible: share.appleShare.available && share.appleShare.deliveries.some(entry =>
                    entry.state === "completed" || entry.state === "cancelled")
                text: qsTr("Archivia completate")
                enabled: !share.appleShare.busy
                onClicked: share.appleShare.archiveCompletedDeliveries()
            }

            Button {
                id: done

                Layout.alignment: Qt.AlignRight
                text: qsTr("Chiudi")
                implicitHeight: 42 * share.uiScale
                leftPadding: 13 * share.uiScale
                rightPadding: leftPadding
                font.pixelSize: 15 * share.uiScale
                font.weight: Font.DemiBold
                onClicked: share.close()

                contentItem: Text {
                    text: done.text
                    color: share.blue
                    font: done.font
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: done.down ? "#dfeaff" : "#edf3ff"
                    border.color: share.blue
                    border.width: done.visualFocus ? 2 : 0
                    radius: 7 * share.uiScale
                }
            }
        }
    }
}
