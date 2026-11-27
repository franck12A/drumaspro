pragma Singleton
import QtQuick

QtObject {
    property bool amberMode: false
    property bool compactMode: false

    readonly property color clear: "transparent"
    readonly property color background: amberMode ? "#11100d" : "#bdb7a9"
    readonly property color panel: amberMode ? "#211d17" : "#cfc9b8"
    readonly property color panelInset: amberMode ? "#15130f" : "#b8b1a1"
    readonly property color panelRaised: amberMode ? "#30291e" : "#ded8c9"
    readonly property color panelLight: amberMode ? "#403728" : "#ebe5d7"
    readonly property color panelShadow: amberMode ? "#080807" : "#918a7c"
    readonly property color ink: amberMode ? "#eee5d3" : "#151612"
    readonly property color mutedInk: amberMode ? "#a99c83" : "#4d4b43"
    readonly property color edge: amberMode ? "#080807" : "#151612"
    readonly property color trim: amberMode ? "#514330" : "#5c5a51"
    readonly property color accent: amberMode ? "#e5aa49" : "#405746"
    readonly property color accentBright: amberMode ? "#ffd27a" : "#6f8b70"
    readonly property color sectionFill: amberMode ? "#090908" : "#171815"
    readonly property color sectionInk: "#f0e9dc"
    readonly property color lcdFace: amberMode ? "#0a0c08" : "#20271f"
    readonly property color lcdInk: amberMode ? "#ffd27a" : "#cee0b5"
    readonly property color lcdMuted: amberMode ? "#a48b61" : "#9ca98b"
    readonly property color ledOff: amberMode ? "#4d2823" : "#783b35"
    readonly property color ledOn: "#ed4338"
    readonly property color ledActive: amberMode ? "#7adb74" : "#4fbf6a"
    readonly property color ledBypass: "#d84438"
    readonly property color ledGlowOn: amberMode ? "#338a32" : "#2f7a3c"
    readonly property color ledGlowBypass: "#8c1f18"
    readonly property color meterOn: amberMode ? "#e5aa49" : "#405746"
    readonly property color meterOff: amberMode ? "#493b29" : "#77766b"
    readonly property color knobFace: amberMode ? "#25231d" : "#343632"
    readonly property color knobFaceEdge: amberMode ? "#090908" : "#151612"
    readonly property color knobRidge: amberMode ? "#716044" : "#62635c"
    readonly property color knobRidgeLight: amberMode ? "#8d7752" : "#7d7e76"
    readonly property color knobPointer: "#f5f0e4"
    readonly property color knobBase: amberMode ? "#544833" : "#e4dece"
    readonly property color knobBaseLight: amberMode ? "#6b5a40" : "#f6f1e5"
    readonly property color knobBaseDark: amberMode ? "#332a1e" : "#b9b2a2"
    readonly property color knobShadow: amberMode ? "#080807" : "#8a8375"
    readonly property int knobShadowBlur: 6
    readonly property int knobShadowOffset: 3
    readonly property color knobSpecular: "#4dffffff"
    // Bevel language shared by buttons, panels and screws.
    readonly property color bevelLight: amberMode ? "#6d5b40" : "#fbf6ea"
    readonly property color bevelDark: amberMode ? "#000000" : "#6f695d"
    readonly property color buttonFaceTop: amberMode ? "#3b3225" : "#eae4d6"
    readonly property color buttonFaceBottom: amberMode ? "#231e17" : "#c1bbae"
    readonly property color buttonFaceDownTop: amberMode ? "#1e1912" : "#b4aea1"
    readonly property color buttonFaceDownBottom: amberMode ? "#2c2418" : "#cfc9bc"
    readonly property color buttonHover: "#14ffffff"
    readonly property color dropShadow: amberMode ? "#66000000" : "#4a000000"
    readonly property int dropShadowOffset: compactMode ? 2 : 3
    readonly property int dropShadowPressedOffset: 1
    readonly property color windowTop: amberMode ? "#191712" : "#c7c1b2"
    readonly property color windowBottom: amberMode ? "#0c0b09" : "#aea89a"
    readonly property color screwLight: amberMode ? "#8b7652" : "#f1ebdc"
    readonly property color screwDark: amberMode ? "#2a2319" : "#8f887a"
    readonly property color screwEdge: amberMode ? "#0a0908" : "#4f4b42"
    readonly property color screwSlot: amberMode ? "#1a150f" : "#5b564c"
    readonly property int screwDiameter: compactMode ? 6 : 7
    readonly property color toggleOn: amberMode ? "#bd842c" : "#607b61"
    readonly property color toggleOff: amberMode ? "#29251e" : "#777568"
    readonly property color dialTrack: amberMode ? "#514330" : "#827e72"
    readonly property color dialValue: amberMode ? "#e5aa49" : "#405746"

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
    readonly property int titleSize: compactMode ? 18 : 23
    readonly property int sectionSize: compactMode ? 11 : 13
    readonly property int labelSize: compactMode ? 10 : 11
    // Nothing below this size is legible on a 960x600 window; everything that
    // used to ask for 7px now uses this token.
    readonly property int minimumFontSize: compactMode ? 8 : 9
    readonly property int tinySize: minimumFontSize
    readonly property int displaySize: compactMode ? 10 : 11
    readonly property int smallTextSize: compactMode ? 9 : 10
    readonly property int brandSize: compactMode ? 21 : 26
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
    readonly property int rotaryLabelSize: compactMode ? 8 : 9
    readonly property int rotaryPositionLabelSize: compactMode ? 8 : 9
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
