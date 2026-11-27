import QtQuick

Item {
    id: root
    objectName: "pianoKeyboard"
    property int lowestNote: 48
    property int octaveCount: 2
    // MIDI notes to highlight: notes held by chords and by this keyboard.
    property var soundingNotes: []
    // Note currently under the mouse, drawn sunk into the key bed.
    property int pressedNote: -1

    signal noteOn(int note, real velocity)
    signal noteOff(int note)

    readonly property int whitePerOctave: 7
    readonly property int whiteCount: octaveCount * whitePerOctave
    readonly property real whiteWidth: width / whiteCount
    readonly property real blackWidth: whiteWidth * 0.62
    readonly property real blackHeight: height * 0.62
    readonly property var whiteSemitones: [0, 2, 4, 5, 7, 9, 11]
    readonly property var blackOffsets: [
        { afterWhite: 0, semitone: 1 },
        { afterWhite: 1, semitone: 3 },
        { afterWhite: 3, semitone: 6 },
        { afterWhite: 4, semitone: 8 },
        { afterWhite: 5, semitone: 10 }
    ]
    readonly property var blackKeys: {
        const keys = []
        for (let octave = 0; octave < root.octaveCount; ++octave) {
            for (let entry = 0; entry < root.blackOffsets.length; ++entry) {
                const spec = root.blackOffsets[entry]
                keys.push({
                    note: root.lowestNote + octave * 12 + spec.semitone,
                    whiteIndex: octave * 7 + spec.afterWhite
                })
            }
        }
        return keys
    }

    function whiteNoteAt(index) {
        if (index < 0 || index >= whiteCount)
            return -1
        const octave = Math.floor(index / whitePerOctave)
        return lowestNote + octave * 12 + whiteSemitones[index % whitePerOctave]
    }

    function isSounding(note) {
        return soundingNotes.indexOf(note) >= 0
    }

    function blackX(whiteIndex) {
        return (whiteIndex + 1) * whiteWidth - blackWidth / 2
    }

    // White keys.
    Repeater {
        model: root.whiteCount
        delegate: Rectangle {
            id: whiteKey
            required property int index
            readonly property int note: root.whiteNoteAt(index)
            readonly property bool sounding: root.isSounding(note)
            readonly property bool down: root.pressedNote === note

            x: index * root.whiteWidth + 1
            y: down ? 2 : 0
            width: root.whiteWidth - 2
            height: root.height - (down ? 2 : 0)
            radius: Theme.pianoKeyRadius
            border.color: Theme.edge
            border.width: Theme.hairlineWidth
            z: 1
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: down ? Theme.keyWhiteDownTop : Theme.keyWhiteTop
                }
                GradientStop {
                    position: 1.0
                    color: sounding ? Theme.keySounding
                                    : (down ? Theme.keyWhiteDownBottom
                                            : Theme.keyWhiteBottom)
                }
            }

            Rectangle {
                x: 1
                y: 1
                width: parent.width - 2
                height: Theme.hairlineWidth
                color: Theme.bevelLight
                opacity: 0.8
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: Theme.hairlineWidth
                color: Theme.bevelDark
                opacity: 0.5
            }

            Text {
                visible: index % root.whitePerOctave === 0
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Theme.inlineGap
                text: "C" + (Math.floor(note / 12) - 1)
                color: Theme.keyInk
                font.family: Theme.labelFont
                font.pixelSize: Theme.minimumFontSize
                font.bold: true
            }
        }
    }

    // Black keys on top of the white key boundaries.
    Repeater {
        model: root.blackKeys
        delegate: Rectangle {
            required property var modelData
            readonly property int note: modelData.note
            readonly property bool sounding: root.isSounding(note)
            readonly property bool down: root.pressedNote === note

            x: root.blackX(modelData.whiteIndex)
            y: down ? 2 : 0
            width: root.blackWidth
            height: root.blackHeight - (down ? 2 : 0)
            radius: Theme.pianoKeyRadius
            border.color: Theme.edge
            border.width: Theme.hairlineWidth
            z: 2
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: down ? Theme.keyBlackDownTop : Theme.keyBlackTop
                }
                GradientStop {
                    position: 1.0
                    color: sounding ? Theme.keySounding
                                    : (down ? Theme.keyBlackDownBottom
                                            : Theme.keyBlackBottom)
                }
            }

            Rectangle {
                x: 1
                y: 1
                width: parent.width - 2
                height: Theme.hairlineWidth
                color: Theme.bevelLight
                opacity: 0.35
            }
        }
    }

    MouseArea {
        id: input
        anchors.fill: parent
        z: 3
        property int currentNote: -1

        function noteAt(px, py) {
            if (py < root.blackHeight) {
                for (let entry = 0; entry < root.blackKeys.length; ++entry) {
                    const key = root.blackKeys[entry]
                    const left = root.blackX(key.whiteIndex)
                    if (px >= left && px <= left + root.blackWidth)
                        return key.note
                }
            }
            let whiteIndex = Math.floor(px / root.whiteWidth)
            whiteIndex = Math.max(0, Math.min(root.whiteCount - 1, whiteIndex))
            return root.whiteNoteAt(whiteIndex)
        }

        // Clicking lower on the key plays harder, like leaning into a key bed.
        function velocityAt(py) {
            return 0.45 + 0.55 * Math.max(0, Math.min(1, py / root.height))
        }

        onPressed: function(mouse) {
            currentNote = noteAt(mouse.x, mouse.y)
            root.pressedNote = currentNote
            root.noteOn(currentNote, velocityAt(mouse.y))
        }
        onPositionChanged: function(mouse) {
            if (!pressed)
                return
            const next = noteAt(mouse.x, mouse.y)
            if (next === currentNote)
                return
            if (currentNote >= 0)
                root.noteOff(currentNote)
            currentNote = next
            root.pressedNote = next
            root.noteOn(next, velocityAt(mouse.y))
        }
        onReleased: {
            if (currentNote >= 0)
                root.noteOff(currentNote)
            currentNote = -1
            root.pressedNote = -1
        }
        onCanceled: {
            if (currentNote >= 0)
                root.noteOff(currentNote)
            currentNote = -1
            root.pressedNote = -1
        }
    }
}
