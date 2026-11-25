pragma Singleton
import QtQuick

QtObject {
    property bool amberMode: false
    property bool compactMode: false

    readonly property color background: amberMode ? "#120f0a" : "#111313"
    readonly property color panel: amberMode ? "#211a10" : "#1b1e1e"
    readonly property color raised: amberMode ? "#302419" : "#262a29"
    readonly property color edge: amberMode ? "#51402a" : "#3d4541"
    readonly property color trim: amberMode ? "#8c6730" : "#3d4541"
    readonly property color accent: amberMode ? "#e5aa49" : "#58b7a3"
    readonly property color accentBright: amberMode ? "#ffd27a" : "#a0e8d4"
    readonly property color ink: "#f0e8d8"
    readonly property color mutedInk: amberMode ? "#b0a58f" : "#9b9b8e"
    readonly property color danger: "#d46a52"
    readonly property color meterOn: amberMode ? "#e5aa49" : "#58b7a3"
    readonly property color meterOff: amberMode ? "#493b29" : "#393c38"

    readonly property string displayFont:
        Qt.platform.os === "windows" ? "Lucida Console" : "monospace"
    readonly property string labelFont: "Segoe UI"

    readonly property int windowWidth: 1280
    readonly property int windowHeight: 720
    readonly property int padding: compactMode ? 8 : 12
    readonly property int compactPadding: compactMode ? 5 : 7
    readonly property int spacing: compactMode ? 6 : 8
    readonly property int corner: 5
    readonly property int titleSize: compactMode ? 18 : 20
    readonly property int sectionSize: 14
    readonly property int labelSize: 11
    readonly property int displaySize: 14
    readonly property int knobDiameter: compactMode ? 54 : 64
    readonly property int knobWidth: compactMode ? 106 : 112
    readonly property int knobHeight: compactMode ? 100 : 132
    readonly property int knobDragDistance: compactMode ? 100 : 120
    readonly property int arcWidth: compactMode ? 4 : 5
    readonly property int effectSlotWidth: compactMode ? 145 : 166
    readonly property int effectSlotHeight: compactMode ? 152 : 176
    readonly property int chordButtonWidth: compactMode ? 112 : 118
    readonly property int chordButtonHeight: compactMode ? 72 : 80
}
