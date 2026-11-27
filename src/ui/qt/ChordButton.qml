import QtQuick
import QtQuick.Controls

Button {
    id: root
    property string rootName: ""
    property string quality: ""
    property string degree: ""
    property var midiNotes: []
    property bool chordActive: false
    signal editRequested()
    readonly property bool sounding: chordActive || down

    implicitWidth: Theme.chordButtonWidth
    implicitHeight: Theme.chordButtonHeight
    padding: Theme.chordButtonGap

    contentItem: Column {
        spacing: Theme.chordButtonGap

        Text {
            width: parent.width
            text: (root.rootName + " " + root.quality).toUpperCase()
            color: Theme.mutedInk
            font.family: Theme.labelFont
            font.pixelSize: Theme.smallTextSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
        }

        Item {
            width: Theme.chordLedDiameter
            height: width
            anchors.horizontalCenter: parent.horizontalCenter

            // Soft halo instead of a hard ring: two low-opacity discs behind
            // the LED body (a rectangle cannot do a radial gradient cheaply).
            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 2.4
                height: width
                radius: width / 2
                color: Theme.ledGlowOn
                opacity: root.sounding ? 0.35 : 0.0
                visible: opacity > 0
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width * 1.6
                height: width
                radius: width / 2
                color: Theme.ledGlowOn
                opacity: root.sounding ? 0.45 : 0.0
                visible: opacity > 0
            }

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: root.sounding ? Theme.ledOn : Theme.ledOff
                border.color: Theme.edge
                border.width: Theme.hairlineWidth

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: Theme.chordButtonGap / 2
                    radius: width / 2
                    color: root.sounding ? Theme.sectionInk : Theme.ledOff
                    opacity: root.sounding ? 0.8 : 0.35
                }
            }
        }

        Text {
            width: parent.width
            text: root.rootName
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: Theme.titleSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            width: parent.width
            text: root.degree
            color: Theme.accent
            font.family: Theme.labelFont
            font.pixelSize: Theme.labelSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // Right-click opens the manual chord editor for this pad.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: root.editRequested()
    }

    background: Item {
        // The separate shadow shrinks as the cap sinks, so the key reads as a
        // physical cap instead of a flat rectangle.
        Rectangle {
            width: parent.width
            height: parent.height
            y: root.sounding ? Theme.dropShadowPressedOffset : Theme.dropShadowOffset
            radius: Theme.corner
            color: Theme.dropShadow
        }

        Rectangle {
            width: parent.width
            height: parent.height
            y: root.sounding ? Theme.pressedOffset : 0
            radius: Theme.corner
            border.color: Theme.edge
            border.width: Theme.moduleBorderWidth
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: root.sounding ? Theme.buttonFaceDownTop : Theme.buttonFaceTop
                }
                GradientStop {
                    position: 1.0
                    color: root.sounding ? Theme.buttonFaceDownBottom : Theme.buttonFaceBottom
                }
            }

            Rectangle {
                x: Theme.moduleBorderWidth
                y: Theme.moduleBorderWidth
                width: parent.width - 2 * Theme.moduleBorderWidth - Theme.hairlineWidth
                height: Theme.hairlineWidth
                color: Theme.bevelLight
                opacity: root.sounding ? 0.25 : 0.75
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
                color: root.hovered && !root.sounding ? Theme.buttonHover : Theme.clear
            }
        }
    }
}
