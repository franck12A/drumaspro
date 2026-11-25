import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root
    property string text: ""
    property bool inverted: false
    property bool bracketed: false
    spacing: Theme.groupPadding
    implicitHeight: Theme.sectionHeight

    Rectangle {
        implicitWidth: sectionText.implicitWidth + 2 * Theme.groupPadding
        Layout.preferredHeight: Theme.sectionHeight
        radius: Theme.corner / 2
        color: root.inverted ? Theme.sectionFill : Theme.panelRaised
        border.color: Theme.edge
        border.width: root.inverted ? 0 : Theme.hairlineWidth

        Text {
            id: sectionText
            anchors.centerIn: parent
            text: root.bracketed ? "[ " + root.text + " ]" : root.text
            color: root.inverted ? Theme.sectionInk : Theme.ink
            font.family: Theme.labelFont
            font.pixelSize: Theme.sectionSize
            font.bold: true
            font.letterSpacing: Theme.serigraphyTracking
        }
    }

    Rectangle {
        Layout.fillWidth: true
        height: Theme.sectionRuleHeight
        color: root.inverted ? Theme.sectionFill : Theme.edge
    }
}
