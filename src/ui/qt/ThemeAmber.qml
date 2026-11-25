import QtQuick

Rectangle {
    radius: Theme.corner
    color: Theme.panel
    border.color: Theme.trim
    border.width: Theme.moduleBorderWidth

    Rectangle {
        anchors.fill: parent
        anchors.margins: Theme.moduleBorderWidth + Theme.insetFrameGap
        radius: Theme.corner / 2
        color: Theme.clear
        border.color: Theme.panelShadow
        border.width: Theme.hairlineWidth
    }
}
