pragma Singleton
import QtQuick

QtObject {
    property bool amberMode: false
    property bool nightMode: false
    property bool compactMode: false
    // 0 = the skin's own accent, 1..5 = fixed presets.
    property int accentPreset: 0
    // Typography multiplier from the settings (0.9, 1.0, 1.1, 1.25). Control
    // geometry stays fixed so 960x600 keeps fitting.
    property real uiScale: 1.0

    function pick(amber, night, day) {
        return amberMode ? amber : (nightMode ? night : day)
    }

    function scaled(size) {
        return Math.max(1, Math.round(size * uiScale))
    }

    readonly property color clear: "transparent"
    readonly property color background: pick("#11100d", "#0a0f0c", "#bdb7a9")
    readonly property color panel: pick("#211d17", "#121a14", "#cfc9b8")
    readonly property color panelInset: pick("#15130f", "#0b110d", "#b8b1a1")
    readonly property color panelRaised: pick("#30291e", "#1d271f", "#ded8c9")
    readonly property color panelLight: pick("#403728", "#2b3a2d", "#ebe5d7")
    readonly property color panelShadow: pick("#080807", "#040705", "#918a7c")
    readonly property color ink: pick("#eee5d3", "#dfeee0", "#151612")
    readonly property color mutedInk: pick("#a99c83", "#93a894", "#4d4b43")
    readonly property color edge: pick("#080807", "#050806", "#151612")
    readonly property color trim: pick("#514330", "#33463a", "#5c5a51")

    readonly property color defaultAccent: pick("#e5aa49", "#57d47a", "#405746")
    readonly property color defaultAccentBright: pick("#ffd27a", "#8cf5a5", "#6f8b70")
    readonly property var accentBases: ["#4f8a5c", "#c08a33", "#4a7fa8",
                                        "#7a5aa6", "#b0603f"]
    readonly property var accentBrights: ["#7fb38c", "#e8b45c", "#78aed6",
                                          "#a586cf", "#d98a63"]
    readonly property color accent: accentPreset <= 0
        ? defaultAccent
        : accentBases[Math.min(accentPreset, 5) - 1]
    readonly property color accentBright: accentPreset <= 0
        ? defaultAccentBright
        : accentBrights[Math.min(accentPreset, 5) - 1]

    readonly property color sectionFill: pick("#090908", "#060a07", "#171815")
    readonly property color sectionInk: "#f0e9dc"
    readonly property color lcdFace: pick("#0a0c08", "#071208", "#20271f")
    readonly property color lcdInk: pick("#ffd27a", "#8dffab", "#cee0b5")
    readonly property color lcdMuted: pick("#a48b61", "#5c8a68", "#9ca98b")
    readonly property color ledOff: pick("#4d2823", "#3a1f1c", "#783b35")
    readonly property color ledOn: "#ed4338"
    readonly property color ledActive: pick("#7adb74", "#69e08a", "#4fbf6a")
    readonly property color ledBypass: "#d84438"
    readonly property color ledGlowOn: pick("#338a32", "#2f8a4a", "#2f7a3c")
    readonly property color ledGlowBypass: "#8c1f18"
    readonly property color meterOn: accent
    readonly property color meterOff: pick("#493b29", "#2c3a2f", "#77766b")
    readonly property color knobFace: pick("#25231d", "#232923", "#343632")
    readonly property color knobFaceEdge: pick("#090908", "#060806", "#151612")
    readonly property color knobRidge: pick("#716044", "#5c705f", "#62635c")
    readonly property color knobRidgeLight: pick("#8d7752", "#7d9480", "#7d7e76")
    readonly property color knobPointer: "#f5f0e4"
    readonly property color knobBase: pick("#544833", "#3a4a3c", "#e4dece")
    readonly property color knobBaseLight: pick("#6b5a40", "#556b56", "#f6f1e5")
    readonly property color knobBaseDark: pick("#332a1e", "#1d261e", "#b9b2a2")
    readonly property color knobShadow: pick("#080807", "#030504", "#8a8375")
    readonly property int knobShadowBlur: 6
    readonly property int knobShadowOffset: 3
    readonly property color knobSpecular: "#4dffffff"
    // Bevel language shared by buttons, panels and screws.
    readonly property color bevelLight: pick("#6d5b40", "#4d6a52", "#fbf6ea")
    readonly property color bevelDark: pick("#000000", "#000000", "#6f695d")
    readonly property color buttonFaceTop: pick("#3b3225", "#243027", "#eae4d6")
    readonly property color buttonFaceBottom: pick("#231e17", "#131b15", "#c1bbae")
    readonly property color buttonFaceDownTop: pick("#1e1912", "#0e140f", "#b4aea1")
    readonly property color buttonFaceDownBottom: pick("#2c2418", "#1b241c", "#cfc9bc")
    readonly property color buttonHover: "#14ffffff"
    readonly property color dropShadow: pick("#66000000", "#80000000", "#4a000000")
    readonly property int dropShadowOffset: compactMode ? 2 : 3
    readonly property int dropShadowPressedOffset: 1
    readonly property color windowTop: pick("#191712", "#101712", "#c7c1b2")
    readonly property color windowBottom: pick("#0c0b09", "#060a07", "#aea89a")
    readonly property color screwLight: pick("#8b7652", "#4f6a55", "#f1ebdc")
    readonly property color screwDark: pick("#2a2319", "#101610", "#8f887a")
    readonly property color screwEdge: pick("#0a0908", "#030504", "#4f4b42")
    readonly property color screwSlot: pick("#1a150f", "#0a100b", "#5b564c")
    readonly property int screwDiameter: compactMode ? 6 : 7
    readonly property color toggleOn: pick("#bd842c", "#2f7a4a", "#607b61")
    readonly property color toggleOff: pick("#29251e", "#1b241d", "#777568")
    readonly property color dialTrack: pick("#514330", "#3d5142", "#827e72")
    readonly property color dialValue: accent

    readonly property string labelFont: "Arial Narrow"
    readonly property string bodyFont: "Arial"
    readonly property string displayFont: "monospace"
    readonly property string brandFont: "Arial"

    readonly property int windowWidth: 1280
    readonly property int windowHeight: 720
    readonly property int frameInset: compactMode ? 8 : 12
    readonly property int padding: compactMode ? 8 : 12
    readonly property int spacing: compactMode ? 6 : 7
    readonly property int groupPadding: compactMode ? 6 : 9
    readonly property int corner: 4
    readonly property int mainBorderWidth: compactMode ? 3 : 4
    readonly property int moduleBorderWidth: compactMode ? 2 : 3
    readonly property int hairlineWidth: 1
    readonly property int insetFrameGap: 2
    readonly property int sectionRuleHeight: compactMode ? 1 : 2
    readonly property int brandTracking: 2
    readonly property int serigraphyTracking: 1
    readonly property real knobLabelTracking: 0.6
    readonly property int sectionHeight: compactMode ? 20 : 23
    readonly property int titleSize: scaled(compactMode ? 18 : 23)
    readonly property int sectionSize: scaled(compactMode ? 11 : 13)
    readonly property int labelSize: scaled(compactMode ? 10 : 11)
    // Nothing below this size is legible on a 960x600 window; everything that
    // used to ask for 7px now uses this token.
    readonly property int minimumFontSize: scaled(compactMode ? 8 : 9)
    readonly property int tinySize: minimumFontSize
    readonly property int displaySize: scaled(compactMode ? 10 : 11)
    readonly property int smallTextSize: scaled(compactMode ? 9 : 10)
    readonly property int brandSize: scaled(compactMode ? 21 : 26)
    readonly property int lcdWidth: compactMode ? 91 : 112
    readonly property int lcdHeight: compactMode ? 38 : 45
    readonly property int lcdWideWidth: compactMode ? 124 : 150
    readonly property int lcdWideHeight: compactMode ? 41 : 48
    readonly property int knobSmallDiameter: compactMode ? 54 : 62
    readonly property int knobSmallWidth: compactMode ? 76 : 88
    readonly property int knobSmallHeight: compactMode ? 90 : 110
    readonly property int knobLargeDiameter: compactMode ? 68 : 102
    readonly property int knobLargeWidth: compactMode ? 100 : 142
    readonly property int knobLargeHeight: compactMode ? 100 : 130
    readonly property int knobSmallTicks: compactMode ? 15 : 17
    readonly property int knobLargeTicks: 21
    readonly property int knobRingWidth: compactMode ? 3 : 4
    readonly property int knobRidgeWidth: 1
    readonly property int knobRidgeCount: 32
    readonly property int knobValueLineWidth: 3
    readonly property int knobDragDistance: compactMode ? 100 : 120
    readonly property int knobLabelHeight: compactMode ? 12 : 14
    readonly property int knobValueHeight: compactMode ? 12 : 14
    readonly property int rotaryDiameter: compactMode ? 66 : 78
    readonly property int rotaryWidth: compactMode ? 94 : 112
    readonly property int rotaryHeight: compactMode ? 104 : 118
    readonly property int rotaryLabelSize: scaled(compactMode ? 8 : 9)
    readonly property int rotaryPositionLabelSize: scaled(compactMode ? 8 : 9)
    readonly property int rotaryLabelHeight: compactMode ? 10 : 12
    readonly property int rotaryOptionLabelWidth: compactMode ? 28 : 32
    readonly property int rotaryDragStep: compactMode ? 16 : 20
    readonly property int rotaryRidgeCount: 24
    readonly property int rotaryMinimumTickCount: 3
    readonly property int selectorWidth: compactMode ? 104 : 124
    readonly property int selectorHeight: compactMode ? 29 : 32
    readonly property int selectorIndicatorWidth: compactMode ? 8 : 9
    readonly property int selectorIndicatorHeight: compactMode ? 5 : 6
    readonly property int toggleWidth: compactMode ? 88 : 102
    readonly property int toggleHeight: compactMode ? 28 : 32
    readonly property int pressedOffset: 2
    readonly property int chordButtonWidth: compactMode ? 124 : 142
    readonly property int chordButtonHeight: compactMode ? 80 : 92
    readonly property int chordLedDiameter: compactMode ? 7 : 8
    readonly property int chordButtonGap: compactMode ? 3 : 4
    readonly property color keyWhiteTop: pick("#efe6d2", "#d9d4c6", "#faf6eb")
    readonly property color keyWhiteBottom: pick("#c9bca2", "#b3ad9f", "#d9d2c2")
    readonly property color keyWhiteDownTop: pick("#b9a986", "#a29c8f", "#cec7b6")
    readonly property color keyWhiteDownBottom: pick("#d8c9a8", "#c3bdaf", "#e8e1d1")
    readonly property color keyBlackTop: pick("#3b342a", "#2b302b", "#31322e")
    readonly property color keyBlackBottom: pick("#16130e", "#0c110d", "#111210")
    readonly property color keyBlackDownTop: pick("#241f17", "#171d18", "#1c1d1a")
    readonly property color keyBlackDownBottom: pick("#302920", "#202720", "#26271f")
    readonly property color keySounding: accent
    readonly property color keyInk: pick("#3a3226", "#1b241d", "#3b3c36")
    readonly property int pianoKeyRadius: 3
    readonly property int pianoKeyHeight: chordButtonHeight
    readonly property int pianoOctaveThreshold: 1100
    readonly property int meterBarWidth: compactMode ? 5 : 6
    readonly property int meterBarHeight: compactMode ? 11 : 13
    readonly property int ledBorderWidth: 1
    readonly property int masterValueHeight: compactMode ? 14 : 16
    readonly property int effectSlotWidth: compactMode ? 145 : 190
    readonly property int effectSlotHeight: compactMode ? 110 : 126
    readonly property int futureDockHeight: compactMode ? 42 : 50
    readonly property int futureDockWidth: compactMode ? 130 : 190
    readonly property int masterHeaderHeight: compactMode ? 100 : 130
    readonly property int controlSideWidth: compactMode ? 202 : 238
    readonly property int parameterGroupPadding: compactMode ? 3 : 5
    readonly property int rotaryPanelWidth: compactMode ? 94 : 112
    readonly property int slotLabelHeight: compactMode ? 10 : 12
    readonly property int sliderHeight: compactMode ? 18 : 20
    readonly property int inlineGap: compactMode ? 3 : 5
    readonly property int stepCellWidth: compactMode ? 23 : 30
    readonly property int stepCellHeight: compactMode ? 22 : 28
    readonly property int futureLabelWidth: compactMode ? 120 : 170
    readonly property int themeToggleWidth: compactMode ? 66 : 78
}
