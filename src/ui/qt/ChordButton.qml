import QtQuick
import QtQuick.Controls

Button {
    id: root
    property string rootName: ""
    property string quality: ""
    property string degree: ""
    property var midiNotes: []
    property bool chordActive: false
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

        Rectangle {
            width: Theme.chordLedDiameter
            height: width
            radius: width / 2
            anchors.horizontalCenter: parent.horizontalCenter
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

    background: Rectangle {
        y: root.sounding ? Theme.pressedOffset : 0
        radius: Theme.corner
        color: root.sounding ? Theme.panelInset : Theme.panelRaised
        border.color: Theme.edge
        border.width: Theme.moduleBorderWidth
    }
}
