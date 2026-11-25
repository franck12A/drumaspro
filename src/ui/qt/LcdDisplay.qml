import QtQuick

Rectangle {
    id: root
    property string label: ""
    property string value: ""
    property string unit: ""
    property bool wide: false

    implicitWidth: wide ? Theme.lcdWideWidth : Theme.lcdWidth
    implicitHeight: wide ? Theme.lcdWideHeight : Theme.lcdHeight
    radius: Theme.corner
    color: Theme.lcdFace
    border.color: Theme.edge
    border.width: Theme.moduleBorderWidth

    Column {
        anchors.fill: parent
        anchors.margins: Theme.groupPadding
        spacing: Theme.inlineGap

        Text {
            width: parent.width
            text: root.label
            color: Theme.lcdMuted
            font.family: Theme.labelFont
            font.pixelSize: Theme.smallTextSize
            font.letterSpacing: Theme.serigraphyTracking
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            text: root.value + (root.unit.length > 0 ? " " + root.unit : "")
            color: Theme.lcdInk
            font.family: Theme.displayFont
            font.pixelSize: Theme.displaySize
            font.bold: true
            elide: Text.ElideRight
        }
    }
}
