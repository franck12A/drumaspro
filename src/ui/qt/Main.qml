import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: Theme.windowWidth
    height: Theme.windowHeight
    minimumWidth: Theme.windowWidth
    minimumHeight: Theme.windowHeight
    visible: true
    title: "Drumaspro"
    color: Theme.background

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.padding
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
                label: "VOICES"
                value: engineBridge.activeVoices / 32
            }
        }

        ThemeAmber {
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.padding
                spacing: Theme.spacing

                SectionTitle { text: "SYNTHESIZER" }

                RowLayout {
                    Layout.fillWidth: true
                    Selector {
                        id: patchSelector
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
                    columns: 8
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

                RowLayout {
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
                    Item { Layout.fillWidth: true }
                }
            }
        }

        SectionTitle { text: "EFFECTS CHAIN" }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.compactPadding
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
            Item { Layout.fillWidth: true }
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
