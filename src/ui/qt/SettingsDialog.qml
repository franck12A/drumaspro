import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "settingsDialog"
    modal: true
    title: "SETUP"
    width: Math.min(700, parent ? parent.width - 40 : 700)
    height: Math.min(560, parent ? parent.height - 40 : 560)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0
    padding: 0
    header: null
    footer: null

    signal themeSelected(string name)
    signal editChordRequested(int row)

    readonly property var skinNames: ["default", "amber", "night"]
    readonly property var skinLabels: ["DEFAULT", "AMBER", "NIGHT"]
    readonly property var scaleValues: [90, 100, 110, 125]
    readonly property var sensitivityValues: [75, 100, 130]
    readonly property var velocityLabels: ["LINEAR", "SOFT", "HARD"]

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
            text: "SETUP"
            color: Theme.ink
            font.family: Theme.brandFont
            font.pixelSize: Theme.sectionSize + 4
            font.bold: true
            font.letterSpacing: Theme.brandTracking
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.inlineGap

                Text {
                    text: "SKIN"
                    color: Theme.mutedInk
                    font.family: Theme.labelFont
                    font.pixelSize: Theme.smallTextSize
                    font.bold: true
                }

                RowLayout {
                    spacing: Theme.inlineGap
                    Repeater {
                        model: root.skinLabels
                        delegate: ToggleButton {
                            required property int index
                            required property string modelData
                            Layout.preferredWidth: Theme.toggleWidth
                            Layout.preferredHeight: Theme.sectionHeight
                            label: modelData
                            checked: engineBridge.themeName === root.skinNames[index]
                            onToggled: {
                                if (checked)
                                    root.themeSelected(root.skinNames[index])
                            }
                        }
                    }
                }

                Text {
                    Layout.topMargin: Theme.inlineGap
                    text: "ACCENT"
                    color: Theme.mutedInk
                    font.family: Theme.labelFont
                    font.pixelSize: Theme.smallTextSize
                    font.bold: true
                }

                RowLayout {
                    spacing: Theme.inlineGap
                    Repeater {
                        model: 6
                        delegate: Rectangle {
                            required property int index
                            width: Theme.toggleHeight
                            height: width
                            radius: Theme.corner
                            border.color: engineBridge.accentPreset === index
                                ? Theme.ink : Theme.edge
                            border.width: engineBridge.accentPreset === index
                                ? Theme.moduleBorderWidth : Theme.hairlineWidth
                            color: index === 0 ? Theme.defaultAccent
                                               : Theme.accentBases[index - 1]
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: engineBridge.accentPreset = index
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.inlineGap
                    spacing: Theme.groupPadding
                    Text {
                        Layout.preferredWidth: Theme.futureLabelWidth * 0.62
                        text: "UI SCALE"
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: Theme.smallTextSize
                    }
                    Selector {
                        Layout.fillWidth: true
                        model: ["90 %", "100 %", "110 %", "125 %"]
                        currentIndex: Math.max(0, root.scaleValues.indexOf(
                                                  engineBridge.uiScale))
                        onActivated: engineBridge.uiScale =
                                     root.scaleValues[currentIndex]
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.groupPadding
                    Text {
                        Layout.preferredWidth: Theme.futureLabelWidth * 0.62
                        text: "KNOB SENS"
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: Theme.smallTextSize
                    }
                    Selector {
                        Layout.fillWidth: true
                        model: ["LOW", "NORMAL", "HIGH"]
                        currentIndex: {
                            let nearest = 0
                            let distance = 1000
                            for (let index = 0; index < root.sensitivityValues.length;
                                 ++index) {
                                const candidate =
                                    Math.abs(root.sensitivityValues[index] -
                                             engineBridge.knobSensitivity)
                                if (candidate < distance) {
                                    distance = candidate
                                    nearest = index
                                }
                            }
                            return nearest
                        }
                        onActivated: engineBridge.knobSensitivity =
                                     root.sensitivityValues[currentIndex]
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.groupPadding
                    Text {
                        Layout.preferredWidth: Theme.futureLabelWidth * 0.62
                        text: "VELOCITY"
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: Theme.smallTextSize
                    }
                    Selector {
                        Layout.fillWidth: true
                        model: root.velocityLabels
                        currentIndex: engineBridge.velocityCurve
                        onActivated: engineBridge.velocityCurve = currentIndex
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.groupPadding
                    Text {
                        Layout.preferredWidth: Theme.futureLabelWidth * 0.62
                        text: "OCTAVE"
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: Theme.smallTextSize
                    }
                    Selector {
                        Layout.fillWidth: true
                        model: ["0", "1", "2", "3", "4", "5", "6", "7", "8"]
                        currentIndex: engineBridge.defaultOctave
                        onActivated: engineBridge.defaultOctave = currentIndex
                    }
                }

                ToggleButton {
                    Layout.topMargin: Theme.inlineGap
                    Layout.preferredWidth: Theme.toggleWidth
                    Layout.preferredHeight: Theme.sectionHeight
                    label: "SEVENTHS"
                    checked: engineBridge.sevenths
                    onToggled: engineBridge.sevenths = checked
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Theme.inlineGap

                Text {
                    text: "CHORD PADS"
                    color: Theme.mutedInk
                    font.family: Theme.labelFont
                    font.pixelSize: Theme.smallTextSize
                    font.bold: true
                }

                Repeater {
                    model: 7
                    delegate: RowLayout {
                        required property int index
                        spacing: Theme.inlineGap
                        Text {
                            Layout.fillWidth: true
                            text: "PAD " + (index + 1) + (root.chordManual(index)
                                                          ? "  MANUAL" : "  DIATONIC")
                            color: Theme.ink
                            font.family: Theme.labelFont
                            font.pixelSize: Theme.smallTextSize
                        }
                        ToggleButton {
                            checkable: false
                            Layout.preferredWidth: Theme.toggleWidth * 0.7
                            Layout.preferredHeight: Theme.sectionHeight
                            label: "EDIT"
                            onClicked: root.editChordRequested(index)
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.inlineGap
                    text: "AUDIO DEVICE / BUFFER SIZE: pending until the engine\nexposes a safe selection API."
                    color: Theme.mutedInk
                    font.family: Theme.labelFont
                    font.pixelSize: Theme.minimumFontSize
                    wrapMode: Text.WordWrap
                }
            }
        }

        Item { Layout.fillHeight: true }

        Text {
            Layout.fillWidth: true
            text: engineBridge.settingsPath()
            color: Theme.mutedInk
            font.family: Theme.displayFont
            font.pixelSize: Theme.minimumFontSize
            elide: Text.ElideMiddle
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                visible: engineBridge.error.length > 0
                text: engineBridge.error
                color: Theme.ledOn
                font.family: Theme.displayFont
                font.pixelSize: Theme.minimumFontSize
                elide: Text.ElideRight
            }
            ToggleButton {
                checkable: false
                Layout.preferredWidth: Theme.toggleWidth
                Layout.preferredHeight: Theme.toggleHeight
                label: "CLOSE"
                onClicked: root.close()
            }
            }
        }
    }

    function chordManual(row) {
        const slot = engineBridge.chordSlot(row)
        return slot.manual === true
    }
}
