pragma Singleton
import QtQuick

QtObject {
    readonly property color background: "#111313"
    readonly property color panel: "#1b1e1e"
    readonly property color raised: "#262a29"
    readonly property color edge: "#3d4541"
    readonly property color trim: "#8c6730"
    readonly property color accent: "#e5aa49"
    readonly property color accentBright: "#ffd27a"
    readonly property color ink: "#f0e8d8"
    readonly property color mutedInk: "#9b9b8e"
    readonly property color danger: "#d46a52"
    readonly property color meterOn: "#e5aa49"
    readonly property color meterOff: "#393c38"

    readonly property string displayFont:
        Qt.platform.os === "windows" ? "Lucida Console" : "monospace"
    readonly property string labelFont: "Segoe UI"

    readonly property int windowWidth: 1180
    readonly property int windowHeight: 780
    readonly property int padding: 18
    readonly property int compactPadding: 10
    readonly property int spacing: 12
    readonly property int corner: 5
    readonly property int titleSize: 20
    readonly property int sectionSize: 14
    readonly property int labelSize: 11
    readonly property int displaySize: 14
    readonly property int knobDiameter: 72
    readonly property int knobWidth: 112
    readonly property int knobHeight: 142
    readonly property int knobDragDistance: 120
    readonly property int arcWidth: 5
    readonly property int effectSlotWidth: 166
    readonly property int effectSlotHeight: 186
    readonly property int chordButtonWidth: 118
    readonly property int chordButtonHeight: 92
}
