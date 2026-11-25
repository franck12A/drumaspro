import QtQuick

Rectangle {
    id: root
    property string label: ""
    property string value: ""
    property string unit: ""

    implicitWidth: 180
    implicitHeight: 58
    radius: Theme.corner
    color: Theme.background
    border.color: Theme.edge

    Column {
        anchors.fill: parent
        anchors.margins: Theme.compactPadding
        spacing: Theme.compactPadding / 3

        Text {
            text: root.label
            color: Theme.mutedInk
            font.family: Theme.labelFont
            font.pixelSize: Theme.labelSize
            font.letterSpacing: 1
        }

        Text {
            text: root.value + (root.unit.length > 0 ? " " + root.unit : "")
            color: Theme.accentBright
            font.family: Theme.displayFont
            font.pixelSize: Theme.displaySize
            font.bold: true
        }
    }
}
