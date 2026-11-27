import QtQuick

// Hardware-style patch browser: an LCD with the patch name and category, plus
// PREV / filter / NEXT. It replaces the rotary selector because fifteen factory
// patches no longer fit around a dial. The filter cycles through the categories
// that actually exist in the bank, so no extra row of buttons is needed.
Item {
    id: root
    objectName: "patchBrowser"
    property var names: []
    property var categories: []
    property int currentIndex: 0
    property string filter: "ALL"
    property bool userActive: false
    property string userName: ""
    signal activated(int index)

    readonly property int optionCount: names ? names.length : 0
    readonly property var filterOptions: ["ALL", "PAD", "PLUCK", "LEAD",
                                          "KEYS", "BASS", "AMBIENT"]
    readonly property string activeName: userActive
        ? userName
        : (currentIndex >= 0 && currentIndex < optionCount
           ? names[currentIndex].toString() : "")
    readonly property string activeCategory: userActive
        ? "USER"
        : (currentIndex >= 0 && currentIndex < optionCount && categories
           ? categories[currentIndex].toString() : "")

    implicitWidth: 2 * Theme.rotaryWidth + Theme.inlineGap
    implicitHeight: Theme.rotaryHeight
    focus: true

    // A focused browser owns the arrows and ignores typing, so the chord and
    // piano hotkeys do not fire behind it.
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Left) {
            step(-1)
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            step(1)
            event.accepted = true
        } else if (event.text.length > 0) {
            event.accepted = true
        }
    }

    function indexMatches(index) {
        return filter === "ALL" || (categories && categories[index] === filter)
    }

    function filteredIndexes() {
        const result = []
        for (let index = 0; index < optionCount; ++index) {
            if (indexMatches(index))
                result.push(index)
        }
        return result
    }

    function step(delta) {
        const indexes = filteredIndexes()
        if (indexes.length === 0)
            return
        let position = indexes.indexOf(currentIndex)
        if (position < 0)
            position = delta > 0 ? -1 : 0
        const next = (position + delta + indexes.length) % indexes.length
        const target = indexes[next]
        if (target !== currentIndex)
            activated(target)
    }

    function cycleFilter() {
        const current = filterOptions.indexOf(filter)
        filter = filterOptions[(current + 1) % filterOptions.length]
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height - root.buttonHeight - Theme.inlineGap
        radius: Theme.corner
        border.color: Theme.edge
        border.width: Theme.moduleBorderWidth
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.darker(Theme.lcdFace, 1.35) }
            GradientStop { position: 0.12; color: Theme.lcdFace }
            GradientStop { position: 1.0; color: Theme.lcdFace }
        }

        // Inset lip, the same language as the LCD displays.
        Rectangle {
            anchors.fill: parent
            anchors.margins: Theme.moduleBorderWidth
            radius: Math.max(0, Theme.corner - 1)
            color: Theme.clear
            border.color: Theme.bevelLight
            border.width: Theme.hairlineWidth
            opacity: 0.25
        }

        Column {
            width: parent.width - 2 * (Theme.hairlineWidth + 2)
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: "PATCH" + (root.userActive ? "  /  USER" : "")
                color: Theme.lcdMuted
                font.family: Theme.displayFont
                font.pixelSize: Theme.tinySize
                font.bold: true
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: root.activeName.toUpperCase()
                color: Theme.lcdInk
                font.family: Theme.displayFont
                font.pixelSize: Theme.sectionSize
                font.bold: true
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: (root.filter === "ALL" ? "" : root.filter + "  ") +
                      (root.currentIndex + 1) + "/" + root.optionCount
                color: Theme.lcdMuted
                font.family: Theme.displayFont
                font.pixelSize: Theme.tinySize
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }
        }
    }

    readonly property int buttonHeight: Theme.toggleHeight
    readonly property real buttonWidth: (width - 2 * Theme.inlineGap) / 3

    ToggleButton {
        id: previousButton
        objectName: "patchPreviousButton"
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: root.buttonWidth
        height: root.buttonHeight
        checkable: false
        label: "PREV"
        onClicked: root.step(-1)
    }

    ToggleButton {
        id: filterButton
        objectName: "patchFilterButton"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: root.buttonWidth
        height: root.buttonHeight
        checkable: false
        label: root.filter
        onClicked: root.cycleFilter()
    }

    ToggleButton {
        id: nextButton
        objectName: "patchNextButton"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: root.buttonWidth
        height: root.buttonHeight
        checkable: false
        label: "NEXT"
        onClicked: root.step(1)
    }
}
