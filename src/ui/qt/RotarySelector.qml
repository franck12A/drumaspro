import QtQuick

Item {
    id: root
    property string label: ""
    property var options: []
    property int currentIndex: 0
    signal activated(int index)

    readonly property int optionCount: options ? options.length : 0
    readonly property int dialSize: Theme.rotaryDiameter
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
        text: root.label.toUpperCase()
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: Theme.labelSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    Canvas {
        id: dial
        width: root.dialSize
        height: root.dialSize
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -Theme.rotaryLabelHeight / 2

        onPaint: {
            const context = getContext("2d")
            context.reset()
            const center = width / 2
            const radius = width * 0.46
            const count = Math.max(1, root.optionCount)
            const fraction = count > 1
                ? root.currentIndex / (count - 1) : 0

            context.fillStyle = Theme.knobShadow
            context.beginPath()
            context.arc(center, center + Theme.knobRingWidth, radius, 0,
                        Math.PI * 2)
            context.fill()

            context.fillStyle = Theme.knobBase
            context.beginPath()
            context.arc(center, center, radius, 0, Math.PI * 2)
            context.fill()

            for (let ridge = 0; ridge < Theme.rotaryRidgeCount; ++ridge) {
                const angle = ridge * Math.PI * 2 / Theme.rotaryRidgeCount
                context.strokeStyle = Theme.knobRidge
                context.lineWidth = Theme.knobRidgeWidth
                context.beginPath()
                context.moveTo(center + Math.cos(angle) * radius * 0.86,
                               center + Math.sin(angle) * radius * 0.86)
                context.lineTo(center + Math.cos(angle) * radius * 0.96,
                               center + Math.sin(angle) * radius * 0.96)
                context.stroke()
            }

            const faceRadius = radius * 0.78
            context.fillStyle = Theme.knobFace
            context.beginPath()
            context.arc(center, center, faceRadius, 0, Math.PI * 2)
            context.fill()
            context.strokeStyle = Theme.knobFaceEdge
            context.lineWidth = Theme.knobRingWidth
            context.stroke()

            const tickCount = Math.max(
                Theme.rotaryMinimumTickCount, root.optionCount)
            for (let tick = 0; tick < tickCount; ++tick) {
                const angle = Math.PI * 1.25 +
                              Math.PI * 1.5 * tick / Math.max(1, tickCount - 1)
                context.strokeStyle = Theme.ink
                context.lineWidth = Theme.knobRidgeWidth
                context.beginPath()
                context.moveTo(center + Math.cos(angle) * radius * 0.66,
                               center + Math.sin(angle) * radius * 0.66)
                context.lineTo(center + Math.cos(angle) * radius * 0.78,
                               center + Math.sin(angle) * radius * 0.78)
                context.stroke()
            }

            const pointerAngle = Math.PI * 1.25 + Math.PI * 1.5 * fraction
            context.strokeStyle = Theme.knobPointer
            context.lineWidth = Theme.knobValueLineWidth
            context.lineCap = "round"
            context.beginPath()
            context.moveTo(center - Math.cos(pointerAngle) * faceRadius * 0.08,
                           center - Math.sin(pointerAngle) * faceRadius * 0.08)
            context.lineTo(center + Math.cos(pointerAngle) * faceRadius * 0.75,
                           center + Math.sin(pointerAngle) * faceRadius * 0.75)
            context.stroke()
        }
    }

    Repeater {
        model: root.optionCount
        Text {
            required property int index
            readonly property real labelAngle: Math.PI * 1.25 +
                Math.PI * 1.5 * index / Math.max(1, root.optionCount - 1)
            readonly property real labelRadius: root.dialSize * 0.51
            width: Math.min(Theme.rotaryOptionLabelWidth, root.width)
            height: Theme.rotaryLabelHeight
            x: root.width / 2 + Math.cos(labelAngle) * labelRadius - width / 2
            y: root.height / 2 - Theme.rotaryLabelHeight / 2 +
               Math.sin(labelAngle) * labelRadius
            text: root.options[index] === undefined
                ? "" : root.options[index].toString().toUpperCase()
            color: index === root.currentIndex ? Theme.accent : Theme.mutedInk
            font.family: Theme.labelFont
            font.pixelSize: Theme.rotaryLabelSize
            font.bold: index === root.currentIndex
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
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
        target: root
        function onCurrentIndexChanged() { dial.requestPaint() }
        function onOptionCountChanged() { dial.requestPaint() }
    }
}
