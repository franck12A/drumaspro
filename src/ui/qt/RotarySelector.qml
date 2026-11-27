import QtQuick

Item {
    id: root
    property string label: ""
    property var options: []
    // Optional 3-4 character spellings printed around the dial.
    property var shortOptions: []
    property int currentIndex: 0
    signal activated(int index)

    readonly property int optionCount: options ? options.length : 0
    // Positions are printed around the dial only when they fit; with more
    // options the caption under the dial remains the source of truth.
    readonly property bool showPositionLabels: optionCount >= 2 && optionCount <= 4
    implicitWidth: Theme.rotaryWidth
    implicitHeight: Theme.rotaryHeight
    focus: true

    function choose(index) {
        if (optionCount < 1)
            return
        const next = Math.max(0, Math.min(optionCount - 1, index))
        if (next === currentIndex)
            return
        currentIndex = next
        activated(next)
        dial.requestPaint()
    }

    Text {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width
        height: Theme.rotaryLabelHeight
        text: root.label.toUpperCase() +
              (root.optionCount > 0 && root.currentIndex >= 0 &&
               root.currentIndex < root.optionCount
                   ? "  /  " + root.options[root.currentIndex].toString().toUpperCase()
                   : "")
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: Theme.rotaryLabelSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    Canvas {
        id: dial
        width: root.width
        height: root.height - Theme.rotaryLabelHeight
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top

        onPaint: {
            const context = getContext("2d")
            context.reset()
            const cx = width / 2
            const cy = height * 0.46
            const radius = Math.min(width * 0.30, height * 0.36)
            const count = Math.max(1, root.optionCount)
            const fraction = count > 1
                ? root.currentIndex / (count - 1) : 0
            const startAngle = Math.PI * 1.25
            const sweep = Math.PI * 1.5
            const angleAt = function(f) { return startAngle + sweep * f }

            context.save()
            context.shadowColor = "rgba(0, 0, 0, 0.45)"
            context.shadowBlur = Theme.knobShadowBlur
            context.shadowOffsetY = Theme.knobShadowOffset
            context.fillStyle = Theme.knobShadow
            context.beginPath()
            context.arc(cx, cy, radius, 0, Math.PI * 2)
            context.fill()
            context.restore()

            const baseGradient = context.createLinearGradient(
                cx - radius, cy - radius, cx + radius, cy + radius)
            baseGradient.addColorStop(0.0, Theme.knobBaseLight)
            baseGradient.addColorStop(0.55, Theme.knobBase)
            baseGradient.addColorStop(1.0, Theme.knobBaseDark)
            context.fillStyle = baseGradient
            context.beginPath()
            context.arc(cx, cy, radius, 0, Math.PI * 2)
            context.fill()

            for (let ridge = 0; ridge < Theme.rotaryRidgeCount; ++ridge) {
                const angle = ridge * Math.PI * 2 / Theme.rotaryRidgeCount
                const lit = ridge % 2 === 0
                context.strokeStyle = lit ? Theme.knobRidgeLight : Theme.knobRidge
                context.lineWidth = Theme.knobRidgeWidth
                context.beginPath()
                context.moveTo(cx + Math.cos(angle) * radius * (lit ? 0.84 : 0.86),
                               cy + Math.sin(angle) * radius * (lit ? 0.84 : 0.86))
                context.lineTo(cx + Math.cos(angle) * radius * (lit ? 0.97 : 0.95),
                               cy + Math.sin(angle) * radius * (lit ? 0.97 : 0.95))
                context.stroke()
            }

            const faceRadius = radius * 0.78
            context.fillStyle = Theme.knobFace
            context.beginPath()
            context.arc(cx, cy, faceRadius, 0, Math.PI * 2)
            context.fill()

            const sheen = context.createRadialGradient(
                cx - faceRadius * 0.35, cy - faceRadius * 0.4,
                faceRadius * 0.05, cx, cy, faceRadius)
            sheen.addColorStop(0.0, Theme.knobSpecular)
            sheen.addColorStop(0.45, "rgba(255, 255, 255, 0.04)")
            sheen.addColorStop(1.0, "rgba(255, 255, 255, 0)")
            context.fillStyle = sheen
            context.beginPath()
            context.arc(cx, cy, faceRadius, 0, Math.PI * 2)
            context.fill()

            context.strokeStyle = Theme.knobFaceEdge
            context.lineWidth = Theme.knobRingWidth
            context.stroke()

            const tickCount = Math.max(
                Theme.rotaryMinimumTickCount, root.optionCount)
            for (let tick = 0; tick < tickCount; ++tick) {
                const angle = angleAt(tick / Math.max(1, tickCount - 1))
                context.strokeStyle = Theme.ink
                context.lineWidth = Theme.knobRidgeWidth
                context.beginPath()
                context.moveTo(cx + Math.cos(angle) * radius * 0.66,
                               cy + Math.sin(angle) * radius * 0.66)
                context.lineTo(cx + Math.cos(angle) * radius * 0.78,
                               cy + Math.sin(angle) * radius * 0.78)
                context.stroke()
            }

            const pointerAngle = angleAt(fraction)
            context.strokeStyle = Theme.knobPointer
            context.lineWidth = Theme.knobValueLineWidth
            context.lineCap = "round"
            context.beginPath()
            context.moveTo(cx - Math.cos(pointerAngle) * faceRadius * 0.08,
                           cy - Math.sin(pointerAngle) * faceRadius * 0.08)
            context.lineTo(cx + Math.cos(pointerAngle) * faceRadius * 0.75,
                           cy + Math.sin(pointerAngle) * faceRadius * 0.75)
            context.stroke()

            if (root.showPositionLabels) {
                context.font = "bold " + Theme.rotaryPositionLabelSize +
                               "px " + Theme.labelFont
                context.textAlign = "center"
                context.textBaseline = "middle"
                const labelDistance = radius + Theme.serigraphyTracking * 2 +
                                      Theme.rotaryPositionLabelSize * 0.9
                for (let option = 0; option < count; ++option) {
                    const optionAngle = angleAt(option / Math.max(1, count - 1))
                    const positionLabel = root.shortOptions.length > option
                        ? String(root.shortOptions[option])
                        : String(root.options[option])
                    context.fillStyle = option === root.currentIndex
                        ? Theme.ink : Theme.mutedInk
                    context.fillText(positionLabel.substring(0, 4).toUpperCase(),
                                     cx + Math.cos(optionAngle) * labelDistance,
                                     cy + Math.sin(optionAngle) * labelDistance)
                }
            }
        }
    }

    MouseArea {
        id: rotorInput
        anchors.fill: parent
        property real startX: 0
        property real startY: 0
        property int startIndex: 0
        property bool dragged: false
        onPressed: {
            startX = mouse.x
            startY = mouse.y
            startIndex = root.currentIndex
            dragged = false
            root.forceActiveFocus()
        }
        onPositionChanged: {
            if (!pressed || root.optionCount < 2)
                return
            const delta = startX - mouse.x + startY - mouse.y
            const steps = Math.round(delta / Theme.rotaryDragStep)
            if (steps !== 0) {
                dragged = true
                root.choose(startIndex + steps)
            }
        }
        onClicked: {
            if (!dragged)
                root.choose(root.currentIndex + 1 >= root.optionCount
                            ? 0 : root.currentIndex + 1)
        }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Down) {
            root.choose(root.currentIndex - 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Up) {
            root.choose(root.currentIndex + 1)
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter ||
                   event.key === Qt.Key_Space) {
            root.activated(root.currentIndex)
            event.accepted = true
        }
    }

    Connections {
        target: Theme
        function onAmberModeChanged() { dial.requestPaint() }
        function onCompactModeChanged() { dial.requestPaint() }
    }

    Connections {
        target: root
        function onCurrentIndexChanged() { dial.requestPaint() }
        function onOptionCountChanged() { dial.requestPaint() }
        function onLabelChanged() { dial.requestPaint() }
        function onShortOptionsChanged() { dial.requestPaint() }
        function onWidthChanged() { dial.requestPaint() }
        function onHeightChanged() { dial.requestPaint() }
    }
}
