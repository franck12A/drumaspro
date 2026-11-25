import QtQuick
import QtQuick.Controls

Button {
    id: root
    checkable: true
    property string label: ""
    implicitWidth: 92
    implicitHeight: 34

    contentItem: Text {
        text: root.label
        color: root.checked ? Theme.background : Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: Theme.labelSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: Theme.corner
        color: root.checked ? Theme.accent : Theme.raised
        border.color: root.checked ? Theme.accentBright : Theme.edge
    }
}
