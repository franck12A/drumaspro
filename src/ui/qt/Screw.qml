import QtQuick

Item {
    id: root
    property int diameter: Theme.screwDiameter
    property real slotAngle: 35

    implicitWidth: diameter
    implicitHeight: diameter

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        border.color: Theme.screwEdge
        border.width: Theme.hairlineWidth
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.screwLight }
            GradientStop { position: 1.0; color: Theme.screwDark }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.66
            height: 1
            color: Theme.screwSlot
            rotation: root.slotAngle
        }
    }
}
