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
        anchors.margins: Theme.inlineGap
        spacing: Theme.inlineGap

        Text {
            text: "SLOT " + (root.slot + 1)
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: Theme.smallTextSize
            font.bold: true
            Layout.preferredHeight: Theme.slotLabelHeight
        }

        Selector {
            id: effectSelector
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.selectorHeight
            model: ["reverb", "delay", "chorus", "drive", "filter"]
            currentIndex: Math.max(0, ["reverb", "delay", "chorus", "drive", "filter"]
                                      .indexOf(root.typeName))
            onActivated: root.typeSelected(currentIndex)
        }

        ToggleButton {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.toggleHeight
            label: root.bypassed ? "BYPASSED" : "ACTIVE"
            checked: !root.bypassed
            onToggled: root.requestBypass(!checked)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.sliderHeight
            spacing: Theme.inlineGap

            Text {
                text: "WET"
                color: Theme.mutedInk
                font.family: Theme.labelFont
                font.pixelSize: Theme.smallTextSize
            }

            Slider {
                id: mixControl
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.sliderHeight
                from: 0
                to: 1
                value: root.mix
                onMoved: root.requestMix(value)
                background: Rectangle {
                    x: mixControl.leftPadding
                    y: mixControl.topPadding + mixControl.availableHeight / 2 - height / 2
                    width: mixControl.availableWidth
                    height: Theme.moduleBorderWidth
                    color: Theme.edge
                    radius: Theme.corner
                }
                handle: Rectangle {
                    x: mixControl.leftPadding + mixControl.visualPosition *
                       (mixControl.availableWidth - width)
                    y: mixControl.topPadding + mixControl.availableHeight / 2 - height / 2
                    width: Theme.toggleHeight / 2
                    height: Theme.toggleHeight / 2
                    radius: Theme.corner
                    color: Theme.accent
                    border.color: Theme.edge
                    border.width: Theme.moduleBorderWidth
                }
            }
        }
    }
}
