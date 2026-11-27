import QtQuick
import QtQuick.Controls

ComboBox {
    id: root
    implicitHeight: Theme.selectorHeight
    font.family: Theme.bodyFont
    font.pixelSize: Theme.labelSize

    contentItem: Text {
        leftPadding: Theme.groupPadding
        rightPadding: Theme.groupPadding + Theme.selectorIndicatorWidth
        text: root.displayText
        color: Theme.ink
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Canvas {
        x: root.width - width - Theme.groupPadding
        y: root.topPadding + (root.availableHeight - height) / 2
        width: Theme.selectorIndicatorWidth
        height: Theme.selectorIndicatorHeight
        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.fillStyle = Theme.ink
            context.beginPath()
            context.moveTo(0, 0)
            context.lineTo(width, 0)
            context.lineTo(width / 2, height)
            context.closePath()
            context.fill()
        }
    }

    background: Item {
        Rectangle {
            width: parent.width
            height: parent.height
            y: Theme.dropShadowPressedOffset
            radius: Theme.corner
            color: Theme.dropShadow
        }

        Rectangle {
            width: parent.width
            height: parent.height
            radius: Theme.corner
            border.color: root.activeFocus ? Theme.accent : Theme.edge
            border.width: Theme.moduleBorderWidth
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.buttonFaceTop }
                GradientStop { position: 1.0; color: Theme.buttonFaceBottom }
            }

            Rectangle {
                x: Theme.moduleBorderWidth
                y: Theme.moduleBorderWidth
                width: parent.width - 2 * Theme.moduleBorderWidth - Theme.hairlineWidth
                height: Theme.hairlineWidth
                color: Theme.bevelLight
                opacity: 0.7
            }

            Rectangle {
                x: Theme.moduleBorderWidth
                y: parent.height - Theme.moduleBorderWidth - Theme.hairlineWidth
                width: parent.width - 2 * Theme.moduleBorderWidth - Theme.hairlineWidth
                height: Theme.hairlineWidth
                color: Theme.bevelDark
                opacity: 0.5
            }

            Rectangle {
                anchors.fill: parent
                anchors.margins: Theme.moduleBorderWidth
                radius: Math.max(0, Theme.corner - 1)
                color: root.hovered && !root.down ? Theme.buttonHover : Theme.clear
            }
        }
    }

    delegate: ItemDelegate {
        width: root.width
        contentItem: Text {
            text: root.textRole ? model[root.textRole] : modelData
            color: highlighted ? Theme.sectionInk : Theme.ink
            font: root.font
            verticalAlignment: Text.AlignVCenter
        }
        highlighted: root.highlightedIndex === index
        background: Rectangle {
            color: parent.highlighted ? Theme.accent : Theme.panelRaised
        }
    }

    popup: Popup {
        y: root.height
        width: root.width
        implicitHeight: contentItem.implicitHeight
        padding: Theme.groupPadding / 2
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
