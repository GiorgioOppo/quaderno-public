.pragma library

var tolerance = 1e-9

function bounds(nodes) {
    if (nodes.length === 0)
        return { x: 0, y: 0, width: 0, height: 0 }

    let left = nodes[0].x
    let top = nodes[0].y
    let right = left + nodes[0].width
    let bottom = top + nodes[0].height
    for (const node of nodes) {
        left = Math.min(left, node.x)
        top = Math.min(top, node.y)
        right = Math.max(right, node.x + node.width)
        bottom = Math.max(bottom, node.y + node.height)
    }
    return { x: left, y: top, width: right - left, height: bottom - top }
}

function selected(nodes, ids) {
    const keys = new Set(ids)
    return nodes.filter(node => keys.has(node.id))
}

function clamp(value, lower, upper) {
    return Math.max(lower, Math.min(upper, value))
}

function applyPositions(nodes, positions) {
    let changed = false
    const result = nodes.map(node => {
        const position = positions.get(node.id)
        if (!position)
            return node
        const x = clamp(position.x, 0, 1 - node.width)
        const y = clamp(position.y, 0, 1 - node.height)
        if (Math.abs(x - node.x) <= tolerance && Math.abs(y - node.y) <= tolerance)
            return node
        changed = true
        return Object.assign({}, node, { x: x, y: y })
    })
    return changed ? result : nodes
}

function move(nodes, ids, dx, dy) {
    const group = selected(nodes, ids)
    if (group.length === 0 || !Number.isFinite(dx) || !Number.isFinite(dy))
        return nodes

    const area = bounds(group)
    dx = clamp(dx, -area.x, 1 - area.x - area.width)
    dy = clamp(dy, -area.y, 1 - area.y - area.height)
    const positions = new Map()
    for (const node of group)
        positions.set(node.id, { x: node.x + dx, y: node.y + dy })
    return applyPositions(nodes, positions)
}

function align(nodes, ids, alignment) {
    const group = selected(nodes, ids)
    if (group.length < 2)
        return nodes

    const area = bounds(group)
    const positions = new Map()
    for (const node of group) {
        let x = node.x
        let y = node.y
        switch (alignment) {
        case "left":   x = area.x; break
        case "center": x = area.x + (area.width - node.width) / 2; break
        case "right":  x = area.x + area.width - node.width; break
        case "top":    y = area.y; break
        case "middle": y = area.y + (area.height - node.height) / 2; break
        case "bottom": y = area.y + area.height - node.height; break
        default: return nodes
        }
        positions.set(node.id, { x: x, y: y })
    }
    return applyPositions(nodes, positions)
}

function distribute(nodes, ids, axis) {
    if (axis !== "horizontal" && axis !== "vertical")
        return nodes
    const group = selected(nodes, ids)
    if (group.length < 3)
        return nodes

    const horizontal = axis === "horizontal"
    const positionKey = horizontal ? "x" : "y"
    const sizeKey = horizontal ? "width" : "height"
    const area = bounds(group)
    const occupied = group.reduce((total, node) => total + node[sizeKey], 0)
    const spare = area[sizeKey] - occupied
    if (spare < -tolerance)
        return nodes

    // Only the positions change. Keep the page's draw order in the result.
    group.sort((a, b) => a[positionKey] - b[positionKey])
    const gap = Math.max(0, spare) / (group.length - 1)
    const positions = new Map()
    let start = area[positionKey]
    for (let index = 0; index < group.length; ++index) {
        const node = group[index]
        if (index === group.length - 1)
            start = area[positionKey] + area[sizeKey] - node[sizeKey]
        const position = { x: node.x, y: node.y }
        position[positionKey] = start
        positions.set(node.id, position)
        start += node[sizeKey] + gap
    }
    return applyPositions(nodes, positions)
}
