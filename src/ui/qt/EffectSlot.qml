import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ThemeAmber {
    id: root
    showScrews: false
    property int slot: 0
    property string typeName: "none"
    property bool bypassed: true
    property real mix: 0.5
    property bool selected: false
    signal selectRequested()
    signal typeSelected(int index)
    signal requestBypass(bool bypassed)
    signal requestMix(real mix)

    implicitWidth: Theme.effectSlotWidth
    implicitHeight: Theme.effectSlotHeight
    border.color: root.selected ? Theme.accent : Theme.trim
    border.width: root.selected ? Theme.moduleBorderWidth + 1 : Theme.moduleBorderWidth

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.inlineGap
        spacing: Theme.inlineGap

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.slotLabelHeight

            Rectangle {
                id: statusLed
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.chordLedDiameter
                height: width
                radius: width / 2
                color: root.bypassed ? Theme.ledBypass : Theme.ledActive
                border.color: Theme.edge
                border.width: Theme.hairlineWidth

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 1.9
                    height: width
                    radius: width / 2
                    z: -1
                    color: root.bypassed ? Theme.ledGlowBypass : Theme.ledGlowOn
                    opacity: root.bypassed ? 0.35 : 0.6
                }
            }

            Text {
                anchors.left: statusLed.right
                anchors.leftMargin: Theme.inlineGap
                anchors.verticalCenter: parent.verticalCenter
                text: "SLOT " + (root.slot + 1)
                color: root.selected ? Theme.accentBright : Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: Theme.smallTextSize
                font.bold: true
            }

            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.selected ? "EDIT" : "SELECT"
                color: root.selected ? Theme.accent : Theme.mutedInk
                font.family: Theme.labelFont
                font.pixelSize: Theme.tinySize
                font.letterSpacing: Theme.serigraphyTracking
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.selectRequested()
            }
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
