import QtQuick
import QtQuick.Layouts

Column {
    id: root
    property real value: 0
    property string label: "LEVEL"
    property int bars: 12
    spacing: Theme.inlineGap

    Text {
        text: root.label
        color: Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: Theme.smallTextSize
        font.letterSpacing: Theme.serigraphyTracking
    }

    Row {
        spacing: Theme.inlineGap
        Repeater {
            model: root.bars
            Rectangle {
                required property int index
                width: Theme.meterBarWidth
                height: Theme.meterBarHeight
                radius: Theme.corner / 2
                color: index < Math.round(Math.max(0, Math.min(1, root.value)) * root.bars)
                       ? Theme.meterOn : Theme.meterOff
                border.color: Theme.edge
                border.width: Theme.hairlineWidth
            }
        }
    }
}
