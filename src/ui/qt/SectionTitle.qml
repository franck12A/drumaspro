import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root
    property string text: ""
    spacing: Theme.spacing

    Text {
        text: root.text
        color: Theme.accent
        font.family: Theme.labelFont
        font.pixelSize: Theme.sectionSize
        font.bold: true
        font.letterSpacing: 1.4
    }

    Rectangle {
        Layout.fillWidth: true
        height: 1
        color: Theme.edge
    }
}
