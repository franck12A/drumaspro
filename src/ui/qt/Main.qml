import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    objectName: "mainWindow"
    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "Drumaspro"
    color: Theme.background
    property bool amberTheme: false
    property bool compactLayout: width < 1050 || height < 680

    Binding {
        target: Theme
        property: "amberMode"
        value: window.amberTheme
    }

    Binding {
        target: Theme
        property: "compactMode"
        value: window.compactLayout
    }

    ScrollView {
        id: page
        objectName: "mainPage"
        anchors.fill: parent
        anchors.margins: Theme.padding
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: page.availableWidth
            spacing: Theme.spacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.compactPadding / 2
                    Text {
                        text: "DRUMASPRO"
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: Theme.titleSize
                        font.bold: true
                        font.letterSpacing: 2
                    }
                    Text {
                        text: "VIRTUAL INSTRUMENT  /  PERFORMANCE SYSTEM"
                        color: Theme.mutedInk
                        font.family: Theme.labelFont
                        font.pixelSize: Theme.labelSize
                        font.letterSpacing: 1
                    }
                }

                LcdDisplay {
                    label: "ENGINE"
                    value: engineBridge.running ? "ONLINE" : "OFFLINE"
                    unit: engineBridge.activeVoices + " VOICES"
                }

                LcdDisplay {
                    label: "XRUNS"
                    value: engineBridge.xruns.toString()
                    unit: "COUNT"
                }

                LevelMeter {
                    label: "OUTPUT"
                    value: engineBridge.outputPeak
                }
            }

            ThemeAmber {
                Layout.fillWidth: true
                implicitHeight: synthContent.implicitHeight + 2 * Theme.padding

                ColumnLayout {
                    id: synthContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.padding
                    spacing: Theme.spacing

                    SectionTitle { text: "SYNTHESIZER" }

                    RowLayout {
                        Layout.fillWidth: true
                        Selector {
                            id: patchSelector
                            objectName: "patchSelector"
                            Layout.preferredWidth: Theme.effectSlotWidth
                            model: engineBridge.patchModel
                            textRole: "name"
                            onActivated: engineBridge.selectPatch(currentIndex)
                        }

                        Selector {
                            id: keySelector
                            Layout.preferredWidth: Theme.effectSlotWidth
                            model: ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
                            onActivated: engineBridge.setChordScale(currentIndex,
                                                                   modeSelector.currentIndex === 1)
                        }

                        Selector {
                            id: modeSelector
                            Layout.preferredWidth: Theme.effectSlotWidth
                            model: ["MAJOR", "NATURAL MINOR"]
                            onActivated: engineBridge.setChordScale(keySelector.currentIndex,
                                                                   currentIndex === 1)
                        }

                        ToggleButton {
                            label: "AMBER"
                            checked: window.amberTheme
                            onToggled: window.amberTheme = checked
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: "HOLD A CHORD TO PLAY"
                            color: Theme.mutedInk
                            font.family: Theme.labelFont
                            font.pixelSize: Theme.labelSize
                            font.letterSpacing: 1
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: Math.max(1, Math.min(8,
                            Math.floor((synthContent.width + Theme.spacing) /
                                      (Theme.knobWidth + Theme.spacing))))
                        rowSpacing: Theme.compactPadding
                        columnSpacing: Theme.spacing

                        Repeater {
                            model: [
                                { label: "MASTER", parameter: "master.gain" },
                                { label: "CUTOFF", parameter: "filter.cutoff" },
                                { label: "RESONANCE", parameter: "filter.resonance" },
                                { label: "ATTACK", parameter: "amp.attack" },
                                { label: "RELEASE", parameter: "amp.release" },
                                { label: "LFO RATE", parameter: "lfo.rate" },
                                { label: "OSC 1", parameter: "osc1.level" },
                                { label: "OSC 2", parameter: "osc2.level" }
                            ]
                            delegate: Knob {
                                required property var modelData
                                readonly property int controlId:
                                    engineBridge.parameterId(modelData.parameter)
                                label: modelData.label
                                unit: engineBridge.parameterModel.unitForId(controlId)
                                minimum: engineBridge.parameterModel.minimumForId(controlId)
                                maximum: engineBridge.parameterModel.maximumForId(controlId)
                                value: engineBridge.parameterModel.valueForId(controlId)
                                onValueEdited: engineBridge.setParameter(controlId, nextValue)
                            }
                        }
                    }

                    SectionTitle { text: "DIATONIC CHORDS" }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Theme.compactPadding
                        Repeater {
                            model: engineBridge.chordModel
                            delegate: ChordButton {
                                required property int index
                                required property string chordRoot
                                required property string chordQuality
                                required property string degreeNumeral
                                required property var chordMidiNotes
                                rootName: chordRoot
                                quality: chordQuality
                                degree: degreeNumeral
                                midiNotes: chordMidiNotes
                                onPressed: engineBridge.playChord(index)
                                onReleased: engineBridge.releaseChord(index)
                            }
                        }
                    }
                }
            }

            SectionTitle { text: "EFFECTS CHAIN" }

            GridLayout {
                Layout.fillWidth: true
                columns: Math.max(1, Math.min(6,
                    Math.floor((page.availableWidth + Theme.compactPadding) /
                              (Theme.effectSlotWidth + Theme.compactPadding))))
                columnSpacing: Theme.compactPadding
                rowSpacing: Theme.compactPadding

                Repeater {
                    model: engineBridge.chainModel
                    delegate: EffectSlot {
                        required property int slot
                        required property string effectName
                        required property bool isBypassed
                        required property real effectMix
                        typeName: effectName
                        bypassed: isBypassed
                        mix: effectMix
                        onTypeSelected: engineBridge.setEffectType(slot,
                                                                   engineBridge.effectTypeId(index))
                        onRequestBypass: engineBridge.setEffectBypass(slot, bypassed)
                        onRequestMix: engineBridge.setEffectMix(slot, mix)
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: engineBridge.error.length > 0
                text: engineBridge.error
                color: Theme.danger
                font.family: Theme.displayFont
                font.pixelSize: Theme.labelSize
                elide: Text.ElideRight
            }
        }
    }
}
