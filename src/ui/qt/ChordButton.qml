import QtQuick
import QtQuick.Controls

Button {
    id: root
    property string rootName: ""
    property string quality: ""
    property string degree: ""
    property var midiNotes: []

    implicitWidth: Theme.chordButtonWidth
    implicitHeight: Theme.chordButtonHeight

    contentItem: Column {
        spacing: Theme.compactPadding / 3
        Text {
            text: root.degree
            color: Theme.accent
            font.family: Theme.labelFont
            font.pixelSize: Theme.labelSize
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            text: root.rootName
            color: Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: Theme.titleSize
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            text: root.quality
            color: Theme.mutedInk
            font.family: Theme.labelFont
            font.pixelSize: Theme.labelSize
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    background: Rectangle {
        radius: Theme.corner
        color: root.down ? Theme.accent : Theme.raised
        border.color: root.down ? Theme.accentBright : Theme.trim
        border.width: 1
    }
}
