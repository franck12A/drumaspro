import QtQuick
import QtQuick.Controls

Button {
    id: root
    checkable: true
    property string label: ""
    property bool showLed: false
    implicitWidth: Theme.toggleWidth
    implicitHeight: Theme.toggleHeight

    contentItem: Item {
        y: root.down ? 1 : 0

        Row {
            anchors.centerIn: parent
            spacing: Theme.inlineGap

            Rectangle {
                visible: root.showLed
                width: Theme.chordLedDiameter
                height: width
                radius: width / 2
                anchors.verticalCenter: parent.verticalCenter
                color: root.checked ? Theme.ledActive : Theme.ledOff
                border.color: Theme.edge
                border.width: Theme.hairlineWidth
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.label
                color: root.checked ? Theme.sectionInk : Theme.ink
                font.family: Theme.labelFont
                font.pixelSize: Theme.labelSize
                font.bold: true
            }
        }
    }

    background: Item {
            Rectangle {
                width: parent.width
                height: parent.height
                y: root.down ? Theme.dropShadowPressedOffset : Theme.dropShadowOffset
                radius: Theme.corner
                color: Theme.dropShadow
            }

            Rectangle {
                width: parent.width
                height: parent.height
                y: root.down ? Theme.pressedOffset : 0
                radius: Theme.corner
                border.color: Theme.edge
                border.width: Theme.moduleBorderWidth
                gradient: Gradient {
                    GradientStop {
                        position: 0.0
                        color: root.checked ? Qt.lighter(Theme.toggleOn, 1.25)
                                            : Theme.buttonFaceTop
                    }
                    GradientStop {
                        position: 1.0
                        color: root.checked ? Qt.darker(Theme.toggleOn, 1.2)
                                            : Theme.buttonFaceBottom
                    }
                }

                Rectangle {
                    x: Theme.moduleBorderWidth
                    y: Theme.moduleBorderWidth
                    width: parent.width - 2 * Theme.moduleBorderWidth - Theme.hairlineWidth
                    height: Theme.hairlineWidth
                    color: Theme.bevelLight
                    opacity: 0.7
                }

                Rectangle {
                    x: Theme.moduleBorderWidth
                    y: parent.height - Theme.moduleBorderWidth - Theme.hairlineWidth
                    width: parent.width - 2 * Theme.moduleBorderWidth - Theme.hairlineWidth
                    height: Theme.hairlineWidth
                    color: Theme.bevelDark
                    opacity: 0.5
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: Theme.moduleBorderWidth
                    radius: Math.max(0, Theme.corner - 1)
                    color: root.hovered && !root.down ? Theme.buttonHover : Theme.clear
                }
            }
        }
}
