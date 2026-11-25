import QtQuick
import QtQuick.Controls

Button {
    id: root
    checkable: true
    property string label: ""
    implicitWidth: Theme.toggleWidth
    implicitHeight: Theme.toggleHeight

    contentItem: Text {
        text: root.label
        color: root.checked ? Theme.sectionInk : Theme.ink
        font.family: Theme.labelFont
        font.pixelSize: Theme.labelSize
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        y: root.down ? Theme.pressedOffset : 0
        radius: Theme.corner
        color: root.checked ? Theme.toggleOn : Theme.toggleOff
        border.color: Theme.edge
        border.width: Theme.moduleBorderWidth
    }
}
