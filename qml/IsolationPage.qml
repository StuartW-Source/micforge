import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Voice Isolation: Setup (calibration), learning other voices, and the Voice Gate controls.
ColumnLayout {
    id: root
    property var app
    readonly property var p: controller.params
    readonly property var zones: controller.zones
    readonly property bool gateIn: p.gateAdded > 0.5
    spacing: Theme.sp3

    // Call to action until Setup has been run (afterwards the Voice Gate panel shows the result and Re-run).
    Card {
        visible: !root.zones.valid || !root.gateIn
        Layout.fillWidth: true
        accent: Theme.gate
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.sp4
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.sp1
                Text {
                    text: !root.zones.valid ? "MicForge hasn't been tuned to your voice yet"
                        : root.zones.others > root.zones.room ? "Tuned to your voice and to the other voices in your room"
                        : "Tuned to your voice"
                    color: Theme.text; font.pixelSize: Theme.fsTitle; font.weight: Font.Bold
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim; font.pixelSize: Theme.fsBody
                    text: !root.zones.valid
                          ? "Setup records a few seconds of room noise and your voice (and, if you share the room, other voices), then sets the Voice Gate so only you get through. Recordings stay on this computer."
                          : "Re-run Setup after moving your mic or changing rooms. If someone new is talking nearby, use “Learn other voices” while you stay silent."
                }
            }
            PillButton {
                text: root.zones.valid ? "Re-run Setup" : "Run Setup"
                icon: "✦"
                primary: true
                accent: Theme.gate
                enabled: !controller.safeMode
                onClicked: app.openSetup()
            }
            PillButton {
                visible: controller.hasRecordings && root.p.needOthers > 0.5
                text: "Learn other voices"
                icon: "+"
                enabled: !controller.safeMode
                onClicked: app.openLearn()
            }
        }
    }

    // The gate is removable; offer to put it back.
    Card {
        visible: !root.gateIn
        Layout.fillWidth: true
        title: "The Voice Gate isn't in your effects chain"
        subtitle: "Without it, everything your mic picks up is sent, including other people and background noise."
        PillButton { text: "Add Voice Gate"; primary: true; accent: Theme.gate; onClicked: controller.addEffect("gate") }
    }

    Loader {
        id: gate
        visible: root.gateIn
        Layout.fillWidth: true
        Layout.fillHeight: true
        active: root.gateIn
        source: "StageGate.qml"
    }
    Connections {
        target: gate.item
        ignoreUnknownSignals: true
        function onSetupRequested() { app.openSetup() }
        function onLearnRequested() { app.openLearn() }
    }
    Item { visible: !root.gateIn; Layout.fillHeight: true }
}
