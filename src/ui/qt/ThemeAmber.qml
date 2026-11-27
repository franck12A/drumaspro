import QtQuick

Rectangle {
    id: root
    property bool showScrews: true

    radius: Theme.corner
    border.color: Theme.trim
    border.width: Theme.moduleBorderWidth
    gradient: Gradient {
        GradientStop { position: 0.0; color: Theme.panelLight }
        GradientStop { position: 0.18; color: Theme.panel }
        GradientStop { position: 1.0; color: Theme.panelInset }
    }

    // Raised frame: a light line on the top and left, shadow on the bottom and
    // right. The inset frame inside keeps the same language in reverse.
    Rectangle {
        x: Theme.moduleBorderWidth
        y: Theme.moduleBorderWidth
        width: parent.width - 2 * Theme.moduleBorderWidth - Theme.hairlineWidth
        height: Theme.hairlineWidth
        color: Theme.bevelLight
        opacity: 0.6
    }

    Rectangle {
        x: Theme.moduleBorderWidth
        y: Theme.moduleBorderWidth
        width: Theme.hairlineWidth
        height: parent.height - 2 * Theme.moduleBorderWidth - Theme.hairlineWidth
        color: Theme.bevelLight
        opacity: 0.6
    }

    Rectangle {
        x: Theme.moduleBorderWidth
        y: parent.height - Theme.moduleBorderWidth - Theme.hairlineWidth
        width: parent.width - 2 * Theme.moduleBorderWidth - Theme.hairlineWidth
        height: Theme.hairlineWidth
        color: Theme.bevelDark
        opacity: 0.4
    }

    Rectangle {
        x: parent.width - Theme.moduleBorderWidth - Theme.hairlineWidth
        y: Theme.moduleBorderWidth
        width: Theme.hairlineWidth
        height: parent.height - 2 * Theme.moduleBorderWidth - Theme.hairlineWidth
        color: Theme.bevelDark
        opacity: 0.4
    }

    Screw {
        visible: root.showScrews
        x: Theme.moduleBorderWidth + Theme.insetFrameGap / 2
        y: x
    }

    Screw {
        visible: root.showScrews
        x: parent.width - width - Theme.moduleBorderWidth - Theme.insetFrameGap / 2
        y: Theme.moduleBorderWidth + Theme.insetFrameGap / 2
        slotAngle: 125
    }

    Screw {
        visible: root.showScrews
        x: Theme.moduleBorderWidth + Theme.insetFrameGap / 2
        y: parent.height - height - Theme.moduleBorderWidth - Theme.insetFrameGap / 2
        slotAngle: 80
    }

    Screw {
        visible: root.showScrews
        x: parent.width - width - Theme.moduleBorderWidth - Theme.insetFrameGap / 2
        y: parent.height - height - Theme.moduleBorderWidth - Theme.insetFrameGap / 2
        slotAngle: 15
    }
}
