import QtQuick

Canvas {
    id: icon

    required property string name
    property color tint: "#1c1c1e"

    implicitWidth: 20
    implicitHeight: 20
    onAvailableChanged: if (available) requestPaint()
    onNameChanged: requestPaint()
    onTintChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        const c = getContext("2d")
        c.reset()
        const size = Math.min(width, height)
        c.translate((width - size) / 2, (height - size) / 2)
        c.scale(size / 24, size / 24)
        c.strokeStyle = tint
        c.lineWidth = 1.8
        c.lineCap = "round"
        c.lineJoin = "round"
        c.beginPath()

        switch (name) {
        case "plus":
            c.moveTo(6, 12); c.lineTo(18, 12)
            c.moveTo(12, 6); c.lineTo(12, 18)
            break
        case "left":
            c.moveTo(14.5, 6.5); c.lineTo(9, 12); c.lineTo(14.5, 17.5)
            break
        case "right":
            c.moveTo(9.5, 6.5); c.lineTo(15, 12); c.lineTo(9.5, 17.5)
            break
        case "undo":
            c.moveTo(9, 5); c.lineTo(4.5, 9.5); c.lineTo(9, 14)
            c.moveTo(5, 9.5); c.lineTo(14, 9.5)
            c.bezierCurveTo(22, 9.5, 22, 18.5, 14.5, 18.5); c.lineTo(11, 18.5)
            break
        case "redo":
            c.moveTo(15, 5); c.lineTo(19.5, 9.5); c.lineTo(15, 14)
            c.moveTo(19, 9.5); c.lineTo(10, 9.5)
            c.bezierCurveTo(2, 9.5, 2, 18.5, 9.5, 18.5); c.lineTo(13, 18.5)
            break
        case "search":
            c.arc(10.5, 10.5, 5.5, 0, Math.PI * 2)
            c.moveTo(14.5, 14.5); c.lineTo(19, 19)
            break
        case "share":
            c.moveTo(8, 9); c.lineTo(7, 9)
            c.quadraticCurveTo(5, 9, 5, 11); c.lineTo(5, 19)
            c.quadraticCurveTo(5, 21, 7, 21); c.lineTo(17, 21)
            c.quadraticCurveTo(19, 21, 19, 19); c.lineTo(19, 11)
            c.quadraticCurveTo(19, 9, 17, 9); c.lineTo(16, 9)
            c.moveTo(12, 14); c.lineTo(12, 3)
            c.moveTo(8.5, 6.5); c.lineTo(12, 3); c.lineTo(15.5, 6.5)
            break
        case "sync":
            c.moveTo(5, 9); c.bezierCurveTo(7, 4, 15, 3, 19, 8)
            c.moveTo(19, 4); c.lineTo(19, 8); c.lineTo(15, 8)
            c.moveTo(19, 15); c.bezierCurveTo(17, 20, 9, 21, 5, 16)
            c.moveTo(5, 20); c.lineTo(5, 16); c.lineTo(9, 16)
            break
        case "continue":
            c.moveTo(10, 7); c.lineTo(10, 4); c.lineTo(20, 4); c.lineTo(20, 20); c.lineTo(10, 20); c.lineTo(10, 17)
            c.moveTo(3, 12); c.lineTo(15.5, 12)
            c.moveTo(12, 8.5); c.lineTo(15.5, 12); c.lineTo(12, 15.5)
            break
        case "download":
            c.moveTo(5, 14); c.lineTo(5, 20); c.lineTo(19, 20); c.lineTo(19, 14)
            c.moveTo(12, 4); c.lineTo(12, 14)
            c.moveTo(8.5, 10.5); c.lineTo(12, 14); c.lineTo(15.5, 10.5)
            break
        case "upload":
            c.moveTo(5, 14); c.lineTo(5, 20); c.lineTo(19, 20); c.lineTo(19, 14)
            c.moveTo(12, 15); c.lineTo(12, 4)
            c.moveTo(8.5, 7.5); c.lineTo(12, 4); c.lineTo(15.5, 7.5)
            break
        case "close":
            c.moveTo(6, 6); c.lineTo(18, 18)
            c.moveTo(18, 6); c.lineTo(6, 18)
            break
        case "expand":
            c.moveTo(4, 9); c.lineTo(4, 4); c.lineTo(9, 4)
            c.moveTo(15, 4); c.lineTo(20, 4); c.lineTo(20, 9)
            c.moveTo(20, 15); c.lineTo(20, 20); c.lineTo(15, 20)
            c.moveTo(9, 20); c.lineTo(4, 20); c.lineTo(4, 15)
            break
        case "collapse":
            c.moveTo(4, 9); c.lineTo(9, 9); c.lineTo(9, 4)
            c.moveTo(15, 4); c.lineTo(15, 9); c.lineTo(20, 9)
            c.moveTo(20, 15); c.lineTo(15, 15); c.lineTo(15, 20)
            c.moveTo(9, 20); c.lineTo(9, 15); c.lineTo(4, 15)
            break
        case "exit":
            c.moveTo(11, 4); c.lineTo(5, 4); c.lineTo(5, 20); c.lineTo(11, 20)
            c.moveTo(9, 12); c.lineTo(20, 12)
            c.moveTo(16, 8); c.lineTo(20, 12); c.lineTo(16, 16)
            break
        case "rename":
            c.moveTo(5, 16); c.lineTo(15.8, 5.2); c.lineTo(18.8, 8.2)
            c.lineTo(8, 19); c.lineTo(4, 20); c.closePath()
            c.moveTo(13.7, 7.3); c.lineTo(16.7, 10.3)
            break
        case "pen":
            c.moveTo(4, 20); c.lineTo(6, 13.5); c.lineTo(16, 3.5)
            c.quadraticCurveTo(17, 2.5, 18, 3.5); c.lineTo(20.5, 6)
            c.quadraticCurveTo(21.5, 7, 20.5, 8); c.lineTo(10.5, 18); c.closePath()
            c.moveTo(14, 5.5); c.lineTo(18.5, 10)
            c.moveTo(6, 13.5); c.lineTo(10.5, 18)
            break
        case "eraser":
            c.moveTo(3.5, 14); c.lineTo(13, 4.5)
            c.quadraticCurveTo(14.5, 3, 16, 4.5); c.lineTo(20, 8.5)
            c.quadraticCurveTo(21.5, 10, 20, 11.5); c.lineTo(11, 20.5)
            c.lineTo(7.5, 20.5); c.lineTo(3.5, 16.5)
            c.quadraticCurveTo(2.25, 15.25, 3.5, 14); c.closePath()
            c.moveTo(8, 9.5); c.lineTo(15, 16.5)
            c.moveTo(11, 20.5); c.lineTo(21, 20.5)
            break
        case "sort":
            c.moveTo(4, 6); c.lineTo(13, 6)
            c.moveTo(4, 12); c.lineTo(11, 12)
            c.moveTo(4, 18); c.lineTo(8, 18)
            c.moveTo(17, 6); c.lineTo(17, 18)
            c.moveTo(13.5, 14.5); c.lineTo(17, 18); c.lineTo(20.5, 14.5)
            break
        case "settings":
            for (let row = 0; row < 3; ++row) {
                const y = 6 + row * 6
                const x = row === 1 ? 15 : 9
                c.moveTo(3.5, y); c.lineTo(x - 2.5, y)
                c.moveTo(x + 2.5, y); c.lineTo(20.5, y)
                c.moveTo(x + 2.5, y); c.arc(x, y, 2.5, 0, Math.PI * 2)
            }
            break
        case "notebook":
            c.moveTo(7, 3); c.lineTo(19, 3); c.lineTo(19, 21); c.lineTo(7, 21)
            c.quadraticCurveTo(4.5, 21, 4.5, 18.5); c.lineTo(4.5, 5.5)
            c.quadraticCurveTo(4.5, 3, 7, 3)
            c.moveTo(8.5, 3); c.lineTo(8.5, 21)
            c.moveTo(12, 8); c.lineTo(16, 8)
            c.moveTo(12, 11); c.lineTo(16, 11)
            break
        case "folder":
            c.moveTo(4, 5); c.lineTo(10, 5); c.lineTo(12, 8); c.lineTo(20, 8)
            c.quadraticCurveTo(21, 8, 21, 9); c.lineTo(21, 18)
            c.quadraticCurveTo(21, 19, 20, 19); c.lineTo(4, 19)
            c.quadraticCurveTo(3, 19, 3, 18); c.lineTo(3, 6)
            c.quadraticCurveTo(3, 5, 4, 5); c.closePath()
            break
        case "more":
            c.fillStyle = tint
            for (const x of [5, 12, 19]) {
                c.moveTo(x + 1.3, 12); c.arc(x, 12, 1.3, 0, Math.PI * 2)
            }
            c.fill()
            return
        case "star":
        case "star-filled":
            c.moveTo(12, 3); c.lineTo(14.7, 8.6); c.lineTo(21, 9.5); c.lineTo(16.5, 13.8)
            c.lineTo(17.6, 20); c.lineTo(12, 17); c.lineTo(6.4, 20); c.lineTo(7.5, 13.8)
            c.lineTo(3, 9.5); c.lineTo(9.3, 8.6); c.closePath()
            if (name === "star-filled") {
                c.fillStyle = tint
                c.fill()
            }
            break
        case "paper":
            c.moveTo(6, 3); c.lineTo(15, 3); c.lineTo(19, 7); c.lineTo(19, 21)
            c.lineTo(6, 21); c.closePath()
            c.moveTo(14.5, 3); c.lineTo(14.5, 8); c.lineTo(19, 8)
            c.moveTo(9, 12); c.lineTo(16, 12)
            c.moveTo(9, 16); c.lineTo(16, 16)
            break
        case "text":
            c.moveTo(4, 6); c.lineTo(20, 6)
            c.moveTo(12, 6); c.lineTo(12, 19)
            c.moveTo(9, 19); c.lineTo(15, 19)
            break
        case "underline":
            c.moveTo(7, 4)
            c.lineTo(7, 11)
            c.bezierCurveTo(7, 17, 17, 17, 17, 11)
            c.lineTo(17, 4)
            c.moveTo(5, 20)
            c.lineTo(19, 20)
            break
        case "image":
            c.rect(3, 4, 18, 16)
            c.moveTo(3, 16)
            c.lineTo(9, 10)
            c.lineTo(13, 14)
            c.lineTo(16, 11)
            c.lineTo(21, 16)
            break
        case "table":
            c.rect(4, 5, 16, 14)
            c.moveTo(4, 10); c.lineTo(20, 10)
            c.moveTo(4, 14.5); c.lineTo(20, 14.5)
            c.moveTo(12, 5); c.lineTo(12, 19)
            break
        case "layers":
            c.moveTo(3, 8); c.lineTo(12, 3.5); c.lineTo(21, 8); c.lineTo(12, 12.5); c.closePath()
            c.moveTo(3, 12); c.lineTo(12, 16.5); c.lineTo(21, 12)
            c.moveTo(3, 16); c.lineTo(12, 20.5); c.lineTo(21, 16)
            break
        case "lasso":
            c.moveTo(6, 15)
            c.bezierCurveTo(0, 10, 7, 4, 13, 4)
            c.bezierCurveTo(20, 4, 22, 8, 20, 12)
            c.bezierCurveTo(18, 16, 9, 19, 6, 15)
            c.bezierCurveTo(3, 12, 4, 20, 9, 20)
            break
        case "chart":
            c.moveTo(4, 4); c.lineTo(4, 20); c.lineTo(21, 20)
            c.moveTo(7, 15); c.lineTo(11, 10); c.lineTo(14, 13); c.lineTo(20, 6)
            break
        case "diagram":
            c.rect(8, 3, 8, 5)
            c.moveTo(12, 8); c.lineTo(12, 12)
            c.moveTo(6, 16); c.lineTo(6, 12); c.lineTo(18, 12); c.lineTo(18, 16)
            c.rect(2.5, 16, 7, 5)
            c.rect(14.5, 16, 7, 5)
            break
        case "duplicate":
            c.rect(8, 8, 12, 12)
            c.moveTo(4, 15); c.lineTo(4, 4); c.lineTo(15, 4)
            break
        case "edge":
            c.moveTo(4, 12); c.lineTo(20, 12)
            c.moveTo(15, 7); c.lineTo(20, 12); c.lineTo(15, 17)
            break
        case "reverse":
            c.moveTo(20, 12); c.lineTo(4, 12)
            c.moveTo(9, 7); c.lineTo(4, 12); c.lineTo(9, 17)
            break
        case "orthogonal":
            c.moveTo(4, 6); c.lineTo(11, 6); c.lineTo(11, 17); c.lineTo(20, 17)
            c.moveTo(16, 13); c.lineTo(20, 17); c.lineTo(16, 21)
            break
        case "align":
            c.moveTo(4, 4); c.lineTo(4, 20)
            c.rect(7, 5, 12, 5); c.rect(7, 14, 8, 5)
            break
        case "pan":
            c.moveTo(4, 12); c.lineTo(20, 12)
            c.moveTo(12, 4); c.lineTo(12, 20)
            c.moveTo(7, 9); c.lineTo(4, 12); c.lineTo(7, 15)
            c.moveTo(17, 9); c.lineTo(20, 12); c.lineTo(17, 15)
            c.moveTo(9, 7); c.lineTo(12, 4); c.lineTo(15, 7)
            c.moveTo(9, 17); c.lineTo(12, 20); c.lineTo(15, 17)
            break
        case "zoom-in":
        case "zoom-out":
            c.arc(10, 10, 6, 0, Math.PI * 2)
            c.moveTo(14.5, 14.5); c.lineTo(20, 20)
            c.moveTo(7, 10); c.lineTo(13, 10)
            if (name === "zoom-in") {
                c.moveTo(10, 7); c.lineTo(10, 13)
            }
            break
        case "paste":
            c.moveTo(8, 6); c.lineTo(5, 6); c.lineTo(5, 21); c.lineTo(19, 21); c.lineTo(19, 6); c.lineTo(16, 6)
            c.rect(8, 3, 8, 6)
            c.moveTo(8, 13); c.lineTo(16, 13)
            c.moveTo(8, 17); c.lineTo(14, 17)
            break
        }
        c.stroke()
    }
}
