import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "chordEditorDialog"
    modal: true
    title: "CHORD PAD"
    width: Math.min(400, parent ? parent.width - 40 : 400)
    height: 420
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    padding: 0
    header: null
    footer: null

    property int chordRow: 0
    property bool manualMode: false
    property var noteModel: engineBridge.noteNames()
    property var qualityModel: engineBridge.chordQualityNames()

    background: Item {}

    contentItem: ThemeAmber {
        id: panel
        showScrews: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.padding + 4
            spacing: Theme.spacing

        Text {
            Layout.fillWidth: true
            text: "PAD " + (root.chordRow + 1)
            color: Theme.ink
            font.family: Theme.brandFont
            font.pixelSize: Theme.sectionSize + 2
            font.bold: true
            font.letterSpacing: Theme.brandTracking
        }

        ToggleButton {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.toggleHeight
            checkable: false
            label: root.manualMode ? "MODE: MANUAL" : "MODE: DIATONIC AUTO"
            onClicked: root.manualMode = !root.manualMode
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.groupPadding
            rowSpacing: Theme.inlineGap

            Text {
                text: "ROOT"
                color: Theme.mutedInk
                font.family: Theme.labelFont
                font.pixelSize: Theme.smallTextSize
            }
            Selector {
                id: rootSelector
                Layout.fillWidth: true
                model: root.noteModel
            }

            Text {
                text: "QUALITY"
                color: Theme.mutedInk
                font.family: Theme.labelFont
                font.pixelSize: Theme.smallTextSize
            }
            Selector {
                id: qualitySelector
                Layout.fillWidth: true
                model: root.qualityModel
            }

            Text {
                text: "INVERSION"
                color: Theme.mutedInk
                font.family: Theme.labelFont
                font.pixelSize: Theme.smallTextSize
            }
            Selector {
                id: inversionSelector
                Layout.fillWidth: true
                model: ["ROOT", "1ST", "2ND", "3RD"]
            }

            Text {
                text: "OCTAVE"
                color: Theme.mutedInk
                font.family: Theme.labelFont
                font.pixelSize: Theme.smallTextSize
            }
            Selector {
                id: octaveSelector
                Layout.fillWidth: true
                model: ["0", "1", "2", "3", "4", "5", "6", "7", "8"]
            }
        }

        Text {
            Layout.fillWidth: true
            visible: root.manualMode
            text: "Manual pads keep their notes when the scale or sevenths change."
            color: Theme.mutedInk
            font.family: Theme.labelFont
            font.pixelSize: Theme.minimumFontSize
            wrapMode: Text.WordWrap
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.inlineGap

            ToggleButton {
                checkable: false
                Layout.preferredWidth: Theme.toggleWidth
                Layout.preferredHeight: Theme.toggleHeight
                label: "RESET"
                onClicked: {
                    engineBridge.resetChordSlot(root.chordRow)
                    root.close()
                }
            }
            Item { Layout.fillWidth: true }
            ToggleButton {
                checkable: false
                Layout.preferredWidth: Theme.toggleWidth
                Layout.preferredHeight: Theme.toggleHeight
                label: "CANCEL"
                onClicked: root.close()
            }
            ToggleButton {
                checkable: false
                Layout.preferredWidth: Theme.toggleWidth
                Layout.preferredHeight: Theme.toggleHeight
                label: "APPLY"
                onClicked: {
                    if (engineBridge.setChordSlot(
                            root.chordRow, root.manualMode,
                            rootSelector.currentIndex, qualitySelector.currentIndex,
                            inversionSelector.currentIndex,
                            octaveSelector.currentIndex))
                        root.close()
                }
            }
            }
        }
    }

    function openFor(row) {
        chordRow = row
        const slot = engineBridge.chordSlot(row)
        manualMode = slot.manual === true
        rootSelector.currentIndex = slot.root
        qualitySelector.currentIndex = slot.quality
        inversionSelector.currentIndex = slot.inversion
        octaveSelector.currentIndex = slot.octave
        open()
    }
}
