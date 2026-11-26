import QtQuick

Item {
    id: root
    property string label: ""
    property string unit: ""
    property real value: 0
    property real minimum: 0
    property real maximum: 1
    // 0 = linear, 1 = logarithmic (core::ParameterCurve).
    property int curve: 0
    property bool large: false
    signal valueEdited(real nextValue)

    readonly property bool logarithmic: root.curve === 1 && root.minimum > 0 &&
                                        root.maximum > root.minimum

    function normalizedFor(number) {
        if (root.maximum <= root.minimum)
            return 0
        if (root.logarithmic) {
            return Math.max(0, Math.min(1, Math.log(number / root.minimum) /
                                                   Math.log(root.maximum / root.minimum)))
        }
        return Math.max(0, Math.min(1, (number - root.minimum) /
                                           (root.maximum - root.minimum)))
    }

    function valueForNormalized(number) {
        const normalized = Math.max(0, Math.min(1, number))
        if (root.logarithmic)
            return root.minimum * Math.pow(root.maximum / root.minimum, normalized)
        return root.minimum + normalized * (root.maximum - root.minimum)
    }

    readonly property int dialDiameter: large
        ? Theme.knobLargeDiameter : Theme.knobSmallDiameter
    implicitWidth: large ? Theme.knobLargeWidth : Theme.knobSmallWidth
    implicitHeight: large ? Theme.knobLargeHeight : Theme.knobSmallHeight

    Column {
        anchors.fill: parent
        spacing: Theme.inlineGap

        Text {
            id: labelText
            width: parent.width
            height: Theme.knobLabelHeight
            text: root.label
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: Theme.labelSize
            font.bold: true
            font.letterSpacing: Theme.knobLabelTracking
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Canvas {
            id: dial
            width: root.dialDiameter
            height: root.dialDiameter
            anchors.horizontalCenter: parent.horizontalCenter

            function shortNumber(number) {
                if (Math.abs(number) >= 1000)
                    return Math.round(number / 1000) + "k"
                if (Math.abs(number) < 10)
                    return number.toFixed(1)
                return Math.round(number).toString()
            }

            onPaint: {
                const context = getContext("2d")
                context.reset()
                const center = width / 2
                const outerRadius = width * 0.48
                const faceRadius = width * 0.36
                const start = Math.PI * 0.75
                const range = Math.PI * 1.5
                const fraction = root.normalizedFor(root.value)
                const tickCount = root.large ? Theme.knobLargeTicks : Theme.knobSmallTicks

                context.fillStyle = Theme.knobShadow
                context.beginPath()
                context.arc(center, center + Theme.knobRingWidth, outerRadius, 0,
                            Math.PI * 2)
                context.fill()

                context.fillStyle = Theme.knobBase
                context.beginPath()
                context.arc(center, center, outerRadius, 0, Math.PI * 2)
                context.fill()

                for (let ridge = 0; ridge < Theme.knobRidgeCount; ++ridge) {
                    const angle = ridge * Math.PI * 2 / Theme.knobRidgeCount
                    const inner = outerRadius * 0.86
                    const outer = outerRadius * 0.96
                    context.strokeStyle = Theme.knobRidge
                    context.lineWidth = Theme.knobRidgeWidth
                    context.beginPath()
                    context.moveTo(center + Math.cos(angle) * inner,
                                   center + Math.sin(angle) * inner)
                    context.lineTo(center + Math.cos(angle) * outer,
                                   center + Math.sin(angle) * outer)
                    context.stroke()
                }

                context.fillStyle = Theme.knobFace
                context.beginPath()
                context.arc(center, center, faceRadius, 0, Math.PI * 2)
                context.fill()
                context.strokeStyle = Theme.knobFaceEdge
                context.lineWidth = Theme.knobRingWidth
                context.stroke()

                for (let tick = 0; tick <= tickCount; ++tick) {
                    const angle = start + range * tick / tickCount
                    const major = tick % 4 === 0
                    const inner = outerRadius * (major ? 0.68 : 0.75)
                    const outer = outerRadius * 0.83
                    context.strokeStyle = major ? Theme.ink : Theme.trim
                    context.lineWidth = major ? Theme.knobRingWidth : Theme.knobRidgeWidth
                    context.beginPath()
                    context.moveTo(center + Math.cos(angle) * inner,
                                   center + Math.sin(angle) * inner)
                    context.lineTo(center + Math.cos(angle) * outer,
                                   center + Math.sin(angle) * outer)
                    context.stroke()
                }

                context.fillStyle = Theme.ink
                context.font = Theme.tinySize + "px " + Theme.displayFont
                context.textAlign = "center"
                context.textBaseline = "middle"
                context.fillText(shortNumber(root.minimum),
                                 center + Math.cos(start) * outerRadius * 0.66,
                                 center + Math.sin(start) * outerRadius * 0.66)
                context.fillText(shortNumber(root.maximum),
                                 center + Math.cos(start + range) * outerRadius * 0.66,
                                 center + Math.sin(start + range) * outerRadius * 0.66)

                const angle = start + range * fraction
                context.strokeStyle = Theme.knobPointer
                context.lineWidth = Theme.knobValueLineWidth
                context.lineCap = "round"
                context.beginPath()
                context.moveTo(center - Math.cos(angle) * faceRadius * 0.12,
                               center - Math.sin(angle) * faceRadius * 0.12)
                context.lineTo(center + Math.cos(angle) * faceRadius * 0.67,
                               center + Math.sin(angle) * faceRadius * 0.67)
                context.stroke()
                context.fillStyle = Theme.knobPointer
                context.beginPath()
                context.arc(center, center, Theme.knobRingWidth, 0, Math.PI * 2)
                context.fill()
            }

            Connections {
                target: root
                function onValueChanged() { dial.requestPaint() }
                function onMinimumChanged() { dial.requestPaint() }
                function onMaximumChanged() { dial.requestPaint() }
                function onCurveChanged() { dial.requestPaint() }
                function onLargeChanged() { dial.requestPaint() }
            }
        }

        Text {
            width: parent.width
            height: Theme.knobValueHeight
            text: (root.value >= 100 ? root.value.toFixed(0) : root.value.toFixed(2)) +
                  (root.unit.length > 0 ? " " + root.unit : "")
            color: Theme.ink
            font.family: Theme.displayFont
            font.pixelSize: Theme.smallTextSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        property real pressedY: 0
        property real pressedFraction: 0
        onPressed: {
            pressedY = mouse.y
            pressedFraction = root.normalizedFor(root.value)
        }
        onPositionChanged: {
            if (!pressed || root.maximum <= root.minimum)
                return
            const delta = (pressedY - mouse.y) / Theme.knobDragDistance
            root.valueEdited(root.valueForNormalized(pressedFraction + delta))
        }
        onWheel: function(wheel) {
            if (root.maximum <= root.minimum)
                return
            const step = wheel.angleDelta.y > 0 ? 0.02 : -0.02
            root.valueEdited(root.valueForNormalized(root.normalizedFor(root.value) + step))
        }
    }
}
