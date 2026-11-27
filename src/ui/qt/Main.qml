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
    property bool amberTheme: false
    property bool compactLayout: width < 1100 || height < 680
    property int currentKeyIndex: 0
    property int currentModeIndex: 0
    property int currentPatchIndex: 0
    property string activeChordName: "READY"
    property var activeChordRows: ({})
    property int selectedEffectSlot: 0

    readonly property var keyNames: [
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    ]
    readonly property var patchNames: ["PAD", "PLUCK", "KEYS"]

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

    function playChord(row) {
        const button = chordRepeater.itemAt(row)
        if (button) {
            button.chordActive = true
            activeChordRows[row] = true
            activeChordName = button.rootName + " " + button.quality
        }
        engineBridge.playChord(row)
    }

    function releaseChord(row) {
        engineBridge.releaseChord(row)
        const button = chordRepeater.itemAt(row)
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
    }

    Item {
        id: keyboardFocus
        objectName: "keyboardFocus"
        width: 0
        height: 0
        focus: true
        property var heldChords: ({})

        function releaseHeldChords() {
            for (let row = 0; row < 7; ++row) {
                if (heldChords[row]) {
                    delete heldChords[row]
                    window.releaseChord(row)
                }
            }
        }

        onActiveFocusChanged: {
            if (!activeFocus)
                releaseHeldChords()
        }

        Keys.priority: Keys.BeforeItem
        Keys.onPressed: function(event) {
            const row = event.key >= Qt.Key_1 && event.key <= Qt.Key_7
                ? event.key - Qt.Key_1 : -1
            if (row < 0 || window.activeFocusItem !== keyboardFocus)
                return
            event.accepted = true
            if (event.isAutoRepeat || keyboardFocus.heldChords[row])
                return
            keyboardFocus.heldChords[row] = true
            window.playChord(row)
        }

        Keys.onReleased: function(event) {
            const row = event.key >= Qt.Key_1 && event.key <= Qt.Key_7
                ? event.key - Qt.Key_1 : -1
            if (row < 0 || !keyboardFocus.heldChords[row])
                return
            delete keyboardFocus.heldChords[row]
            event.accepted = true
            window.releaseChord(row)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.panel
        border.color: Theme.edge
        border.width: Theme.mainBorderWidth

        Rectangle {
            anchors.fill: parent
            anchors.margins: Theme.mainBorderWidth + Theme.insetFrameGap
            color: Theme.clear
            border.color: Theme.trim
            border.width: Theme.hairlineWidth
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
                    Layout.alignment: Qt.AlignVCenter
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
                    label: window.activeChordName
                    value: window.keyNames[window.currentKeyIndex] +
                           (window.currentModeIndex === 0 ? " MAJOR" : " NAT MIN")
                    unit: window.patchNames[window.currentPatchIndex]
                }

                LcdDisplay {
                    objectName: "engineStatusDisplay"
                    label: "ENGINE / " + engineBridge.activeVoices + "V"
                    value: engineBridge.running ? "ONLINE" : "OFFLINE"
                    unit: engineBridge.xruns + "XR"
                }

                LevelMeter {
                    id: outputMeter
                    objectName: "outputMeter"
                    label: "OUTPUT"
                    value: engineBridge.outputPeak
                    bars: 12
                }

                ToggleButton {
                    id: amberThemeToggle
                    objectName: "amberThemeToggle"
                    Layout.preferredWidth: Theme.themeToggleWidth
                    label: "AMBER"
                    checked: window.amberTheme
                    onToggled: window.amberTheme = checked
                }

                Knob {
                    id: masterKnob
                    objectName: "masterKnob"
                    large: true
                    label: "MASTER"
                    readonly property var controlId:
                        engineBridge.parameterId("master.gain")
                    unit: engineBridge.parameterModel.unitForId(controlId)
                    minimum: engineBridge.parameterModel.minimumForId(controlId)
                    maximum: engineBridge.parameterModel.maximumForId(controlId)
                    curve: engineBridge.parameterModel.curveForId(controlId)
                    value: engineBridge.parameterModel.valueForId(controlId)
                    onValueEdited: engineBridge.setParameter(controlId, nextValue)
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

                    SectionTitle {
                        text: "SYNTHESIZER"
                        inverted: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spacing

                        RowLayout {
                            id: rotaryControls
                            Layout.preferredWidth: Theme.controlSideWidth
                            Layout.alignment: Qt.AlignVCenter
                            spacing: Theme.inlineGap

                            RotarySelector {
                                id: patchSelector
                                objectName: "patchSelector"
                                label: "PATCH"
                                options: window.patchNames
                                currentIndex: window.currentPatchIndex
                                onActivated: {
                                    window.currentPatchIndex = index
                                    engineBridge.selectPatch(index)
                                }
                            }

                            RotarySelector {
                                id: modeSelector
                                objectName: "modeSelector"
                                label: "SCALE"
                                options: ["MAJOR", "NAT MIN"]
                                currentIndex: window.currentModeIndex
                                onActivated: {
                                    window.currentModeIndex = index
                                    engineBridge.setChordScale(
                                        window.currentKeyIndex, index === 1)
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

                    SectionTitle {
                        text: "DIATONIC CHORDS"
                        inverted: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
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
                            }
                        }
                    }
                }
            }

            SectionTitle {
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

            ThemeAmber {
                id: effectParameterPanel
                objectName: "effectParameterPanel"
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
                                if (Math.abs(realValue) >= 1000)
                                    return (realValue / 1000).toFixed(1) + "k"
                                if (Math.abs(realValue) >= 100)
                                    return realValue.toFixed(0)
                                return realValue.toFixed(2)
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
                                    text: parameterControl.formatValue(parameterControl.value) +
                                          (parameterControl.unit.length > 0
                                               ? " " + parameterControl.unit : "")
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
}
