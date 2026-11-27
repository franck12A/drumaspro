import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    objectName: "mainWindow"
    width: Theme.windowWidth
    height: Theme.windowHeight
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "DRUMASPRO"
    color: Theme.background
    property bool amberTheme: engineBridge.themeName === "amber"
    property bool nightTheme: engineBridge.themeName === "night"
    property bool compactLayout: width < 1100 || height < 680
    property int currentKeyIndex: engineBridge.initialKey
    property int currentModeIndex: engineBridge.initialScale
    property int currentPatchIndex: engineBridge.initialPatch
    property string activeChordName: "READY"
    property var activeChordRows: ({})
    property int selectedEffectSlot: 0
    // 0 = chord pads, 1 = on-screen piano in the same strip.
    property int bottomView: 0
    property var soundingChordNotes: []
    property var pianoHeldNotes: []

    readonly property var keyNames: [
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    ]
    readonly property var patchNames: {
        const names = []
        for (let index = 0; index < engineBridge.patchCount(); ++index)
            names.push(engineBridge.patchInfo(index).name)
        return names
    }
    readonly property var patchCategories: {
        const categories = []
        for (let index = 0; index < engineBridge.patchCount(); ++index)
            categories.push(engineBridge.patchInfo(index).category)
        return categories
    }

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

    Binding {
        target: Theme
        property: "nightMode"
        value: window.nightTheme
    }

    Binding {
        target: Theme
        property: "accentPreset"
        value: engineBridge.accentPreset
    }

    Binding {
        target: Theme
        property: "uiScale"
        value: engineBridge.uiScale / 100.0
    }

    function applyTheme(name) {
        window.amberTheme = name === "amber"
        window.nightTheme = name === "night"
        engineBridge.themeName = name
    }

    function clearChordHighlights() {
        for (let row = 0; row < 7; ++row) {
            const button = chordRepeater.itemAt(row)
            if (button)
                button.chordActive = false
        }
        activeChordRows = ({})
        activeChordName = "READY"
        refreshSoundingChordNotes()
    }

    function refreshSoundingChordNotes() {
        const notes = []
        for (let row = 0; row < 7; ++row) {
            if (!activeChordRows[row])
                continue
            const button = chordRepeater.itemAt(row)
            if (!button)
                continue
            for (let index = 0; index < button.midiNotes.length; ++index) {
                const note = button.midiNotes[index]
                if (notes.indexOf(note) < 0)
                    notes.push(note)
            }
        }
        soundingChordNotes = notes
    }

    function pressPianoNote(note, velocity) {
        if (engineBridge.noteOn(note, velocity) && pianoHeldNotes.indexOf(note) < 0)
            pianoHeldNotes = pianoHeldNotes.concat([note])
    }

    function releasePianoNote(note) {
        engineBridge.noteOff(note)
        const index = pianoHeldNotes.indexOf(note)
        if (index >= 0) {
            const next = pianoHeldNotes.slice()
            next.splice(index, 1)
            pianoHeldNotes = next
        }
    }

    function playChord(row) {
        const button = chordRepeater.itemAt(row)
        if (button) {
            button.chordActive = true
            activeChordRows[row] = true
            activeChordName = button.rootName + " " + button.quality
        }
        engineBridge.playChord(row)
        if (engineBridge.latchEnabled) {
            for (let other = 0; other < 7; ++other) {
                if (other === row)
                    continue
                const otherButton = chordRepeater.itemAt(other)
                if (otherButton)
                    otherButton.chordActive = false
            }
            activeChordRows = ({})
            activeChordRows[row] = true
        }
        refreshSoundingChordNotes()
    }

    function releaseChord(row) {
        engineBridge.releaseChord(row)
        const button = chordRepeater.itemAt(row)
        // With latch on, the bridge keeps the chord sounding; leave the LED lit.
        if (engineBridge.latchEnabled && engineBridge.latchedRow === row)
            return
        if (button)
            button.chordActive = false
        delete activeChordRows[row]
        activeChordName = "READY"
        for (let activeRow = 0; activeRow < 7; ++activeRow) {
            if (activeChordRows[activeRow]) {
                const button = chordRepeater.itemAt(activeRow)
                if (button)
                    activeChordName = button.rootName + " " + button.quality
            }
        }
        refreshSoundingChordNotes()
    }

    Item {
        id: keyboardFocus
        objectName: "keyboardFocus"
        width: 0
        height: 0
        focus: true
        property var heldChords: ({})
        // Qt key code -> MIDI note for the computer keyboard piano.
        property var heldPianoNotes: ({})

        function releaseHeldChords() {
            for (let row = 0; row < 7; ++row) {
                if (heldChords[row]) {
                    delete heldChords[row]
                    window.releaseChord(row)
                }
            }
        }

        function releasePianoKeys() {
            for (const code in heldPianoNotes) {
                const note = heldPianoNotes[code]
                delete heldPianoNotes[code]
                window.releasePianoNote(note)
            }
        }

        function releaseAllKeys() {
            releaseHeldChords()
            releasePianoKeys()
        }

        onActiveFocusChanged: {
            if (!activeFocus)
                releaseAllKeys()
        }

        Keys.priority: Keys.BeforeItem
        Keys.onPressed: function(event) {
            if (window.activeFocusItem !== keyboardFocus)
                return

            // 1-7 keep playing the diatonic chords regardless of the view.
            const row = event.key >= Qt.Key_1 && event.key <= Qt.Key_7
                ? event.key - Qt.Key_1 : -1
            if (row >= 0) {
                event.accepted = true
                if (event.isAutoRepeat || keyboardFocus.heldChords[row])
                    return
                keyboardFocus.heldChords[row] = true
                window.playChord(row)
                return
            }

            // Z/X transpose the computer keyboard by one octave.
            if (event.key === Qt.Key_Z || event.key === Qt.Key_X) {
                event.accepted = true
                if (event.isAutoRepeat)
                    return
                keyboardFocus.releasePianoKeys()
                engineBridge.shiftPianoOctave(event.key === Qt.Key_Z ? -1 : 1)
                return
            }

            const semitone = engineBridge.semitoneForKey(event.key)
            if (semitone < 0)
                return
            event.accepted = true
            if (event.isAutoRepeat ||
                    keyboardFocus.heldPianoNotes[event.key] !== undefined)
                return
            const note = engineBridge.midiNoteForSemitone(semitone)
            if (note < 0 || note > 127)
                return
            keyboardFocus.heldPianoNotes[event.key] = note
            window.pressPianoNote(note, 0.8)
        }

        Keys.onReleased: function(event) {
            const row = event.key >= Qt.Key_1 && event.key <= Qt.Key_7
                ? event.key - Qt.Key_1 : -1
            if (row >= 0) {
                if (!keyboardFocus.heldChords[row])
                    return
                delete keyboardFocus.heldChords[row]
                event.accepted = true
                window.releaseChord(row)
                return
            }
            if (keyboardFocus.heldPianoNotes[event.key] === undefined)
                return
            const note = keyboardFocus.heldPianoNotes[event.key]
            delete keyboardFocus.heldPianoNotes[event.key]
            event.accepted = true
            window.releasePianoNote(note)
        }
    }

    Rectangle {
        id: chassis
        anchors.fill: parent
        border.color: Theme.edge
        border.width: Theme.mainBorderWidth
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.windowTop }
            GradientStop { position: 0.55; color: Theme.panel }
            GradientStop { position: 1.0; color: Theme.windowBottom }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: Theme.mainBorderWidth + Theme.insetFrameGap
            color: Theme.clear
            border.color: Theme.trim
            border.width: Theme.hairlineWidth
        }

        Screw {
            x: Theme.mainBorderWidth + Theme.insetFrameGap
            y: x
            diameter: Theme.screwDiameter + 2
            slotAngle: 20
        }

        Screw {
            x: parent.width - width - Theme.mainBorderWidth - Theme.insetFrameGap
            y: Theme.mainBorderWidth + Theme.insetFrameGap
            diameter: Theme.screwDiameter + 2
            slotAngle: 110
        }

        Screw {
            x: Theme.mainBorderWidth + Theme.insetFrameGap
            y: parent.height - height - Theme.mainBorderWidth - Theme.insetFrameGap
            diameter: Theme.screwDiameter + 2
            slotAngle: 65
        }

        Screw {
            x: parent.width - width - Theme.mainBorderWidth - Theme.insetFrameGap
            y: parent.height - height - Theme.mainBorderWidth - Theme.insetFrameGap
            diameter: Theme.screwDiameter + 2
            slotAngle: 150
        }
    }

    ScrollView {
        id: page
        objectName: "mainPage"
        anchors.fill: parent
        anchors.margins: Theme.frameInset
        clip: true
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        ScrollBar.vertical.policy: ScrollBar.AsNeeded
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        background: Rectangle {
            color: Theme.clear
        }

        ColumnLayout {
            id: contentColumn
            width: page.width
            height: implicitHeight
            spacing: Theme.spacing

            RowLayout {
                id: header
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.masterHeaderHeight
                spacing: Theme.spacing

                ColumnLayout {
                    objectName: "brandHeader"
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignBottom
                    spacing: Theme.inlineGap

                    TapHandler {
                        onTapped: keyboardFocus.forceActiveFocus()
                    }

                    Text {
                        text: "DRUMASPRO"
                        color: Theme.ink
                        font.family: Theme.brandFont
                        font.pixelSize: Theme.brandSize
                        font.bold: true
                        font.letterSpacing: Theme.brandTracking
                    }

                    Text {
                        text: "POLYPHONIC PERFORMANCE INSTRUMENT"
                        color: Theme.mutedInk
                        font.family: Theme.labelFont
                        font.pixelSize: Theme.smallTextSize
                        font.letterSpacing: Theme.serigraphyTracking
                        elide: Text.ElideRight
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignBottom
                    spacing: Theme.inlineGap
                    Text {
                        text: "TONIC"
                        color: Theme.ink
                        font.family: Theme.labelFont
                        font.pixelSize: Theme.smallTextSize
                        font.bold: true
                    }
                    Selector {
                        id: keySelector
                        objectName: "keySelector"
                        Layout.preferredWidth: Theme.selectorWidth
                        model: window.keyNames
                        currentIndex: window.currentKeyIndex
                        onActivated: {
                            window.currentKeyIndex = currentIndex
                            engineBridge.setChordScale(
                                currentIndex, window.currentModeIndex === 1)
                        }
                    }
                }

                LcdDisplay {
                    id: chordDisplay
                    objectName: "activeChordDisplay"
                    wide: true
                    Layout.alignment: Qt.AlignBottom
                    label: window.activeChordName
                    value: window.keyNames[window.currentKeyIndex] +
                           (window.currentModeIndex === 0 ? " MAJOR" : " NAT MIN")
                    unit: engineBridge.userPatchActive
                          ? "USER / " + engineBridge.currentPatchCategory
                          : (window.currentPatchIndex >= 0 &&
                             window.currentPatchIndex < window.patchNames.length
                             ? window.patchNames[window.currentPatchIndex] +
                               " / " + window.patchCategories[window.currentPatchIndex]
                             : "")
                }

                LcdDisplay {
                    objectName: "engineStatusDisplay"
                    wide: true
                    // Wide enough for "VOICES 4 / XRUNS 0" at both window sizes.
                    Layout.preferredWidth: Theme.lcdWideWidth + 46
                    Layout.alignment: Qt.AlignBottom
                    label: "ENGINE"
                    value: engineBridge.running ? "ONLINE" : "OFFLINE"
                    unit: "VOICES " + engineBridge.activeVoices +
                          " / XRUNS " + engineBridge.xruns
                }

                LevelMeter {
                    id: outputMeter
                    objectName: "outputMeter"
                    Layout.alignment: Qt.AlignBottom
                    label: "OUTPUT"
                    value: engineBridge.outputPeak
                    bars: 12
                }

                LcdDisplay {
                    objectName: "octaveDisplay"
                    Layout.alignment: Qt.AlignBottom
                    label: "OCTAVE"
                    value: "C" + engineBridge.pianoOctave
                    unit: "Z / X"
                }

                ToggleButton {
                    id: amberThemeToggle
                    objectName: "amberThemeToggle"
                    Layout.alignment: Qt.AlignBottom
                    Layout.preferredWidth: Theme.themeToggleWidth
                    label: "AMBER"
                    checked: window.amberTheme
                    onToggled: window.applyTheme(checked ? "amber" : "default")
                }
            }

            ThemeAmber {
                id: synthPanel
                objectName: "synthPanel"
                Layout.fillWidth: true
                implicitHeight: synthContent.implicitHeight + 2 * Theme.padding

                ColumnLayout {
                    id: synthContent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.padding
                    spacing: Theme.spacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.inlineGap

                        SectionTitle {
                            Layout.fillWidth: true
                            text: "SYNTHESIZER"
                            inverted: true
                        }

                        Selector {
                            id: modeSelector
                            objectName: "modeSelector"
                            Layout.preferredWidth: Theme.selectorWidth
                            Layout.preferredHeight: Theme.sectionHeight
                            model: ["MAJOR", "NAT MIN"]
                            currentIndex: window.currentModeIndex
                            onActivated: {
                                window.currentModeIndex = currentIndex
                                engineBridge.setChordScale(
                                    window.currentKeyIndex, currentIndex === 1)
                            }
                        }

                        ToggleButton {
                            objectName: "latchToggle"
                            Layout.preferredWidth: Theme.toggleWidth
                            Layout.preferredHeight: Theme.sectionHeight
                            label: "LATCH"
                            showLed: true
                            checked: engineBridge.latchEnabled
                            onToggled: {
                                engineBridge.latchEnabled = checked
                                if (!checked)
                                    window.clearChordHighlights()
                            }
                        }

                        ToggleButton {
                            objectName: "setupButton"
                            checkable: false
                            Layout.preferredWidth: Theme.toggleWidth
                            Layout.preferredHeight: Theme.sectionHeight
                            label: "SETUP"
                            onClicked: settingsDialog.open()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing

                        RowLayout {
                            id: rotaryControls
                            Layout.preferredWidth: Theme.controlSideWidth
                            Layout.alignment: Qt.AlignVCenter
                            spacing: Theme.inlineGap

                            PatchBrowser {
                                id: patchBrowser
                                Layout.fillWidth: true
                                names: window.patchNames
                                categories: window.patchCategories
                                currentIndex: window.currentPatchIndex
                                userActive: engineBridge.userPatchActive
                                userName: engineBridge.currentPatchName
                                onActivated: function(index) {
                                    window.currentPatchIndex = index
                                    engineBridge.selectPatch(index)
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillHeight: true
                            Layout.preferredWidth: Theme.moduleBorderWidth
                            color: Theme.edge
                        }

                        RowLayout {
                            id: parameterGroups
                            Layout.fillWidth: true
                            spacing: Theme.inlineGap

                            Repeater {
                                model: [
                                    {
                                        label: "OSC",
                                        controls: [
                                            { label: "OSC 1", parameter: "osc1.level" },
                                            { label: "OSC 2", parameter: "osc2.level" }
                                        ]
                                    },
                                    {
                                        label: "FILTER",
                                        controls: [
                                            { label: "CUTOFF", parameter: "filter.cutoff" },
                                            { label: "RESONANCE", parameter: "filter.resonance" }
                                        ]
                                    },
                                    {
                                        label: "ENV",
                                        controls: [
                                            { label: "ATTACK", parameter: "amp.attack" },
                                            { label: "RELEASE", parameter: "amp.release" }
                                        ]
                                    },
                                    {
                                        label: "LFO",
                                        controls: [
                                            { label: "RATE", parameter: "lfo.rate" }
                                        ]
                                    },
                                    {
                                        label: "MASTER",
                                        controls: [
                                            { label: "LEVEL", parameter: "master.gain" }
                                        ]
                                    }
                                ]

                                delegate: Rectangle {
                                    required property var modelData
                                    color: Theme.panelInset
                                    radius: Theme.corner
                                    border.color: Theme.edge
                                    border.width: Theme.moduleBorderWidth
                                    implicitWidth: parameterRow.implicitWidth +
                                                   2 * Theme.parameterGroupPadding
                                    implicitHeight: groupContent.implicitHeight +
                                                    2 * Theme.parameterGroupPadding

                                    ColumnLayout {
                                        id: groupContent
                                        anchors.fill: parent
                                        anchors.margins: Theme.parameterGroupPadding
                                        spacing: Theme.inlineGap

                                        SectionTitle {
                                            Layout.fillWidth: true
                                            text: modelData.label
                                            bracketed: true
                                        }

                                        RowLayout {
                                            id: parameterRow
                                            spacing: Theme.inlineGap

                                            Repeater {
                                                model: modelData.controls
                                                delegate: Knob {
                                                    required property var modelData
                                                    readonly property var controlId:
                                                        engineBridge.parameterId(
                                                            modelData.parameter)
                                                    parameterId: controlId
                                                    label: modelData.label
                                                    unit: engineBridge.parameterModel
                                                        .unitForId(controlId)
                                                    minimum: engineBridge.parameterModel
                                                        .minimumForId(controlId)
                                                    maximum: engineBridge.parameterModel
                                                        .maximumForId(controlId)
                                                    curve: engineBridge.parameterModel
                                                        .curveForId(controlId)
                                                    value: engineBridge.parameterModel
                                                        .valueForId(controlId)
                                                    onValueEdited:
                                                        engineBridge.setParameter(
                                                            controlId, nextValue)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.inlineGap

                        SectionTitle {
                            Layout.fillWidth: true
                            text: "CHORDS / PIANO KEYBOARD"
                            inverted: true
                        }

                        ToggleButton {
                            objectName: "keysViewToggle"
                            Layout.preferredWidth: Theme.toggleWidth
                            Layout.preferredHeight: Theme.sectionHeight
                            label: "KEYS"
                            checked: window.bottomView === 1
                            onToggled: window.bottomView = checked ? 1 : 0
                        }
                    }

                    RowLayout {
                        id: chordRow
                        Layout.fillWidth: true
                        visible: window.bottomView === 0
                        spacing: Theme.inlineGap

                        Repeater {
                            id: chordRepeater
                            model: engineBridge.chordModel
                            delegate: ChordButton {
                                required property int index
                                required property string chordRoot
                                required property string chordQuality
                                required property string degreeNumeral
                                required property var chordMidiNotes
                                objectName: "chordButton_" + index
                                Layout.fillWidth: true
                                rootName: chordRoot
                                quality: chordQuality
                                degree: degreeNumeral
                                midiNotes: chordMidiNotes
                                onPressed: window.playChord(index)
                                onReleased: window.releaseChord(index)
                                onCanceled: window.releaseChord(index)
                                onEditRequested: chordEditor.openFor(index)
                            }
                        }
                    }

                    PianoKeyboard {
                        id: pianoKeyboard
                        Layout.fillWidth: true
                        Layout.preferredHeight: Theme.pianoKeyHeight
                        visible: window.bottomView === 1
                        lowestNote: 48
                        octaveCount: window.width >= Theme.pianoOctaveThreshold ? 3 : 2
                        soundingNotes: window.soundingChordNotes.concat(
                                           window.pianoHeldNotes)
                        onNoteOn: function(note, velocity) {
                            window.pressPianoNote(
                                note, engineBridge.applyVelocityCurve(velocity))
                        }
                        onNoteOff: function(note) {
                            window.releasePianoNote(note)
                        }
                    }
                }
            }

            ThemeAmber {
                id: effectsFrame
                objectName: "effectsFrame"
                Layout.fillWidth: true
                implicitHeight: effectsContent.implicitHeight + 2 * Theme.padding

                ColumnLayout {
                    id: effectsContent
                    anchors.fill: parent
                    anchors.margins: Theme.padding
                    spacing: Theme.spacing

                    SectionTitle {
                        Layout.fillWidth: true
                        text: "EFFECTS CHAIN"
                        inverted: true
                    }

                    GridLayout {
                        id: effectGrid
                        Layout.fillWidth: true
                        columns: 6
                        columnSpacing: Theme.inlineGap
                        rowSpacing: Theme.inlineGap

                        Repeater {
                            model: engineBridge.chainModel
                            delegate: EffectSlot {
                                required property int index
                                required property string effectName
                                required property bool isBypassed
                                required property real effectMix
                                objectName: "effectSlot_" + index
                                Layout.fillWidth: true
                                typeName: effectName
                                slot: index
                                bypassed: isBypassed
                                mix: effectMix
                                selected: window.selectedEffectSlot === index
                                onSelectRequested: {
                                    window.selectedEffectSlot = index
                                    engineBridge.selectEffectSlot(index)
                                }
                                onTypeSelected: {
                                    window.selectedEffectSlot = index
                                    engineBridge.selectEffectSlot(index)
                                    engineBridge.setEffectType(
                                        slot, engineBridge.effectTypeId(index))
                                }
                                onRequestBypass: engineBridge.setEffectBypass(slot, bypassed)
                                onRequestMix: engineBridge.setEffectMix(slot, mix)
                            }
                        }
                    }
                }
            }

            ThemeAmber {
                id: effectParameterPanel
                objectName: "effectParameterPanel"
                showScrews: false
                Layout.fillWidth: true
                implicitHeight: effectParameterContent.implicitHeight + 2 * Theme.inlineGap

                RowLayout {
                    id: effectParameterContent
                    anchors.fill: parent
                    anchors.margins: Theme.inlineGap
                    spacing: Theme.spacing

                    Rectangle {
                        Layout.preferredWidth: slotChip.implicitWidth + 2 * Theme.groupPadding
                        Layout.preferredHeight: Theme.sectionHeight
                        color: Theme.sectionFill
                        radius: Theme.corner / 2

                        Text {
                            id: slotChip
                            anchors.centerIn: parent
                            text: "SLOT " + (window.selectedEffectSlot + 1) + " / " +
                                  engineBridge.selectedEffectTypeName.toUpperCase()
                            color: Theme.sectionInk
                            font.family: Theme.labelFont
                            font.pixelSize: Theme.sectionSize
                            font.bold: true
                            font.letterSpacing: Theme.serigraphyTracking
                        }
                    }

                    Repeater {
                        model: engineBridge.effectParameterModel
                        delegate: ColumnLayout {
                            id: parameterControl
                            required property var parameterId
                            required property string name
                            required property real value
                            required property real minimum
                            required property real maximum
                            required property string unit
                            required property int curve
                            Layout.fillWidth: true
                            Layout.preferredWidth: 120
                            spacing: 0

                            readonly property bool logarithmic: curve === 1 && minimum > 0

                            function normalizedFor(realValue) {
                                if (maximum <= minimum)
                                    return 0
                                if (logarithmic) {
                                    return Math.max(0, Math.min(1, Math.log(realValue / minimum) /
                                                                           Math.log(maximum / minimum)))
                                }
                                return Math.max(0, Math.min(1,
                                                           (realValue - minimum) /
                                                           (maximum - minimum)))
                            }

                            function valueForNormalized(normalized) {
                                const clamped = Math.max(0, Math.min(1, normalized))
                                if (logarithmic)
                                    return minimum * Math.pow(maximum / minimum, clamped)
                                return minimum + clamped * (maximum - minimum)
                            }

                            function formatValue(realValue) {
                                return engineBridge.formatUnitValue(
                                    parameterControl.unit, realValue)
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.inlineGap

                                Text {
                                    Layout.fillWidth: true
                                    text: parameterControl.name.split(".").pop().toUpperCase()
                                    color: Theme.ink
                                    font.family: Theme.labelFont
                                    font.pixelSize: Theme.tinySize
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: parameterControl.formatValue(parameterControl.value)
                                    color: Theme.mutedInk
                                    font.family: Theme.displayFont
                                    font.pixelSize: Theme.tinySize
                                }
                            }

                            Item {
                                id: parameterTrack
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.sliderHeight
                                readonly property real normalized:
                                    parameterControl.normalizedFor(parameterControl.value)

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width
                                    height: Theme.moduleBorderWidth
                                    color: Theme.edge
                                }

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width * parameterTrack.normalized
                                    height: Theme.moduleBorderWidth
                                    color: Theme.accent
                                }

                                Rectangle {
                                    width: Theme.toggleHeight / 2
                                    height: width
                                    radius: width / 2
                                    color: Theme.accent
                                    border.color: Theme.edge
                                    border.width: Theme.hairlineWidth
                                    x: parameterTrack.normalized *
                                       (parameterTrack.width - width)
                                    y: (parameterTrack.height - height) / 2
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    function setFromX(x) {
                                        const normalized = Math.max(
                                            0, Math.min(1, x / width))
                                        engineBridge.setEffectParameter(
                                            window.selectedEffectSlot,
                                            parameterControl.parameterId,
                                            parameterControl.valueForNormalized(normalized))
                                    }
                                    onPressed: setFromX(mouse.x)
                                    onPositionChanged: {
                                        if (pressed)
                                            setFromX(mouse.x)
                                    }
                                    onWheel: function(wheel) {
                                        const step = wheel.angleDelta.y > 0 ? 0.02 : -0.02
                                        const normalized = Math.max(0, Math.min(
                                            1, parameterControl.normalizedFor(
                                                   parameterControl.value) + step))
                                        engineBridge.setEffectParameter(
                                            window.selectedEffectSlot,
                                            parameterControl.parameterId,
                                            parameterControl.valueForNormalized(normalized))
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        visible: engineBridge.effectParameterModel.count === 0
                        text: "NO EDITABLE PARAMETERS"
                        color: Theme.mutedInk
                        font.family: Theme.labelFont
                        font.pixelSize: Theme.smallTextSize
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                visible: engineBridge.error.length > 0
                text: engineBridge.error
                color: Theme.ledOn
                font.family: Theme.displayFont
                font.pixelSize: Theme.smallTextSize
                elide: Text.ElideRight
            }
        }
    }

    SettingsDialog {
        id: settingsDialog
        onThemeSelected: function(name) { window.applyTheme(name) }
        onEditChordRequested: function(row) { chordEditor.openFor(row) }
    }

    ChordEditorDialog {
        id: chordEditor
    }
}
