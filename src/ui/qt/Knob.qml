import QtQuick

Item {
    id: root
    property string label: ""
    property string unit: ""
    property real value: 0
    property real minimum: 0
    property real maximum: 1
    signal valueEdited(real nextValue)

    implicitWidth: Theme.knobWidth
    implicitHeight: Theme.knobHeight

    Column {
        anchors.fill: parent
        spacing: Theme.compactPadding / 2

        Canvas {
            id: dial
            width: Theme.knobDiameter
            height: Theme.knobDiameter
            anchors.horizontalCenter: parent.horizontalCenter

            onPaint: {
                const context = getContext("2d")
                context.reset()
                const center = width / 2
                const radius = width * 0.38
                const start = Math.PI * 0.75
                const range = Math.PI * 1.5
                const fraction = root.maximum > root.minimum
                    ? Math.max(0, Math.min(1, (root.value - root.minimum) /
                                                   (root.maximum - root.minimum)))
                    : 0
                context.lineWidth = Theme.arcWidth
                context.lineCap = "round"
                context.strokeStyle = Theme.edge
                context.beginPath()
                context.arc(center, center, radius, start, start + range)
                context.stroke()
                context.strokeStyle = Theme.accent
                context.beginPath()
                context.arc(center, center, radius, start, start + range * fraction)
                context.stroke()
                const angle = start + range * fraction
                context.fillStyle = Theme.accentBright
                context.beginPath()
                context.arc(center + Math.cos(angle) * radius,
                            center + Math.sin(angle) * radius, Theme.arcWidth, 0,
                            Math.PI * 2)
                context.fill()
            }
            Connections {
                target: root
                function onValueChanged() { dial.requestPaint() }
            }
        }

        Text {
            width: parent.width
            text: root.label
            color: Theme.mutedInk
            font.family: Theme.labelFont
            font.pixelSize: Theme.labelSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            text: (root.value >= 100 ? root.value.toFixed(0) : root.value.toFixed(2)) +
                  (root.unit.length > 0 ? " " + root.unit : "")
            color: Theme.ink
            font.family: Theme.displayFont
            font.pixelSize: Theme.labelSize
            horizontalAlignment: Text.AlignHCenter
        }
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        property real pressedY: 0
        property real pressedValue: 0
        onPressed: {
            pressedY = mouse.y
            pressedValue = root.value
        }
        onPositionChanged: {
            if (!pressed || root.maximum <= root.minimum)
                return
            const delta = (pressedY - mouse.y) / Theme.knobDragDistance *
                          (root.maximum - root.minimum)
            root.valueEdited(Math.max(root.minimum,
                                      Math.min(root.maximum, pressedValue + delta)))
        }
    }
}
