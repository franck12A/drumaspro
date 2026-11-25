import QtQuick
import QtQuick.Layouts

Column {
    id: root
    property real value: 0
    property string label: "LEVEL"
    property int bars: 12
    spacing: Theme.compactPadding / 2

    Text {
        text: root.label
        color: Theme.mutedInk
        font.family: Theme.labelFont
        font.pixelSize: Theme.labelSize
        font.letterSpacing: 1
    }

    Row {
        spacing: Theme.compactPadding / 3
        Repeater {
            model: root.bars
            Rectangle {
                required property int index
                width: Theme.compactPadding
                height: Theme.displaySize
                radius: Theme.corner / 2
                color: index < Math.round(Math.max(0, Math.min(1, root.value)) * root.bars)
                       ? Theme.meterOn : Theme.meterOff
            }
        }
    }
}
