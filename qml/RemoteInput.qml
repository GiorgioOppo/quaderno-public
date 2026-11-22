import QtQuick
import QtQuick.Templates as Templates

QtObject {
    id: input

    property Item focusedField: null

    function clearFocus() {
        focusedField = null
    }

    function textItem(item: Item): bool {
        return item instanceof TextInput || item instanceof TextEdit
    }

    function writable(item: Item): bool {
        return !!item && item.visible && item.enabled && item.remoteEditable === true
            && textItem(item) && !item.readOnly && !item.inputMethodComposing
            && (item.echoMode === undefined || item.echoMode === TextInput.Normal)
    }

    function hit(item: Item, sceneX: real, sceneY: real): Item {
        if (!item.visible || item.opacity <= 0)
            return null
        const inside = item.contains(item.mapFromItem(null, sceneX, sceneY))
        if (item.clip && !inside)
            return null

        // Equal-z children are painted in their original sibling order.
        const children = Array.from(item.children).sort((first, second) => first.z - second.z)
        let index = children.length - 1
        for (; index >= 0 && children[index].z >= 0; --index) {
            const child = hit(children[index], sceneX, sceneY)
            if (child)
                return child
        }
        // An unapproved control in front of another control still owns its hit area.
        if (inside && (item.remoteClickable === true || item.remoteEditable === true
            || item instanceof Templates.AbstractButton || item instanceof MouseArea || textItem(item))) {
            return item
        }
        for (; index >= 0; --index) {
            const child = hit(children[index], sceneX, sceneY)
            if (child)
                return child
        }
        return null
    }

    function tap(rootItem: Item, sceneX: real, sceneY: real): bool {
        clearFocus()
        if (!rootItem)
            return false
        for (let ancestor = rootItem.parent; ancestor; ancestor = ancestor.parent) {
            if (!ancestor.visible || ancestor.opacity <= 0 || (ancestor.clip
                && !ancestor.contains(ancestor.mapFromItem(null, sceneX, sceneY)))) {
                return false
            }
        }
        const target = hit(rootItem, sceneX, sceneY)
        if (!target || !target.enabled)
            return false
        if (writable(target)) {
            const location = target.mapFromItem(null, sceneX, sceneY)
            target.forceActiveFocus(Qt.MouseFocusReason)
            target.cursorPosition = target instanceof TextEdit
                ? target.positionAt(location.x, location.y) : target.positionAt(location.x)
            if (target.activeFocus)
                focusedField = target
            return !!focusedField
        }
        if (target.remoteClickable === true) {
            if (typeof target.remoteActivateAt === "function") {
                const location = target.mapFromItem(null, sceneX, sceneY)
                return target.remoteActivateAt(location.x, location.y) !== false
            }
            if (typeof target.remoteActivate === "function") {
                target.remoteActivate()
                return true
            }
            if (target instanceof Templates.AbstractButton) {
                target.clicked()
                return true
            }
        }
        return false
    }

    // Qt exposes UTF-16 positions. Keep supplementary characters whole when moving or deleting.
    function previousPosition(text: string, position: int): int {
        if (position <= 0)
            return 0
        const low = text.charCodeAt(position - 1)
        const high = text.charCodeAt(position - 2)
        return position - (low >= 0xdc00 && low <= 0xdfff && high >= 0xd800 && high <= 0xdbff ? 2 : 1)
    }

    function nextPosition(text: string, position: int): int {
        if (position >= text.length)
            return text.length
        const high = text.charCodeAt(position)
        const low = text.charCodeAt(position + 1)
        return position + (high >= 0xd800 && high <= 0xdbff && low >= 0xdc00 && low <= 0xdfff ? 2 : 1)
    }

    function key(action: string, text: string): bool {
        const field = focusedField
        if (!writable(field) || !field.activeFocus) {
            clearFocus()
            return false
        }

        const start = field.selectionStart
        const end = field.selectionEnd
        const selected = start !== end
        const cursor = field.cursorPosition
        switch (action) {
        case "text":
        case "new-line":
            if (action === "new-line") {
                if (!(field instanceof TextEdit))
                    return false
                text = "\n"
            } else if (!text.length || text.length > 256 || /[\u0000-\u001f\u007f-\u009f]/.test(text)) {
                return false
            }
            if (selected)
                field.remove(start, end)
            const position = selected ? start : cursor
            const oldLength = field.text.length
            field.insert(position, text)
            field.cursorPosition = position + field.text.length - oldLength
            return true
        case "select-all":
            field.selectAll()
            return true
        case "backspace":
            field.remove(selected ? start : previousPosition(field.text, cursor), selected ? end : cursor)
            return true
        case "delete":
            field.remove(selected ? start : cursor, selected ? end : nextPosition(field.text, cursor))
            return true
        case "left":
            field.cursorPosition = selected ? start : previousPosition(field.text, cursor)
            return true
        case "right":
            field.cursorPosition = selected ? end : nextPosition(field.text, cursor)
            return true
        case "home":
            field.cursorPosition = 0
            return true
        case "end":
            field.cursorPosition = field.text.length
            return true
        default:
            return false
        }
    }
}
