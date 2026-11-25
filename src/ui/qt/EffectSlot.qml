import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ThemeAmber {
    id: root
    property int slot: 0
    property string typeName: "none"
    property bool bypassed: true
    property real mix: 0.5
    signal typeSelected(int index)
    signal requestBypass(bool bypassed)
    signal requestMix(real mix)

    implicitWidth: Theme.effectSlotWidth
    implicitHeight: Theme.effectSlotHeight

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.compactPadding
        spacing: Theme.compactPadding

        Text {
            text: "SLOT " + (root.slot + 1)
            color: Theme.accent
            font.family: Theme.labelFont
            font.pixelSize: Theme.labelSize
            font.bold: true
        }

        Selector {
            id: effectSelector
            Layout.fillWidth: true
            model: ["reverb", "delay", "chorus", "drive", "filter"]
            currentIndex: Math.max(0, ["reverb", "delay", "chorus", "drive", "filter"]
                                      .indexOf(root.typeName))
            onActivated: root.typeSelected(currentIndex)
        }

        ToggleButton {
            Layout.fillWidth: true
            label: root.bypassed ? "BYPASSED" : "ACTIVE"
            checked: !root.bypassed
            onToggled: root.requestBypass(!checked)
        }

        Text {
            text: "WET / DRY"
            color: Theme.mutedInk
            font.family: Theme.labelFont
            font.pixelSize: Theme.labelSize
        }

        Slider {
            id: mixControl
            Layout.fillWidth: true
            from: 0
            to: 1
            value: root.mix
            onMoved: root.requestMix(value)
        }
    }
}
