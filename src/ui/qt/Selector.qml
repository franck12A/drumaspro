import QtQuick
import QtQuick.Controls

ComboBox {
    id: root
    implicitHeight: 38
    font.family: Theme.labelFont
    font.pixelSize: Theme.labelSize

    contentItem: Text {
        leftPadding: Theme.compactPadding
        rightPadding: Theme.compactPadding
        text: root.displayText
        color: Theme.ink
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.corner
        color: Theme.raised
        border.color: root.activeFocus ? Theme.accent : Theme.edge
    }

    delegate: ItemDelegate {
        width: root.width
        contentItem: Text {
            text: root.textRole ? model[root.textRole] : modelData
            color: highlighted ? Theme.accentBright : Theme.ink
            font: root.font
            verticalAlignment: Text.AlignVCenter
        }
        highlighted: root.highlightedIndex === index
        background: Rectangle {
            color: parent.highlighted ? Theme.raised : Theme.panel
        }
    }

    popup: Popup {
        y: root.height
        width: root.width
        implicitHeight: contentItem.implicitHeight
        padding: Theme.compactPadding / 2
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            color: Theme.panel
            border.color: Theme.edge
            radius: Theme.corner
        }
    }
}
