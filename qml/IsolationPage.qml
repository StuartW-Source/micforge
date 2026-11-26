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

    // What the gate hears, by pitch.
    Card {
        id: specCard
        visible: root.gateIn
        Layout.fillWidth: true
        pad: Theme.sp3 + 2
        property bool open: true
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.sp3
            Text { text: "What the gate hears"; color: Theme.text; font.pixelSize: Theme.fsBodyLg; font.weight: Font.Bold }
            Repeater {
                model: [["live sound", Theme.input, false], ["your voice (Setup)", Theme.gate, true], ["other voices", Theme.tone, true], ["room", Theme.textFaint, true]]
                Row {
                    visible: specCard.open && (index === 0 || controller.spectrum.recorded[["", "you", "others", "room"][index]] !== undefined)
                    spacing: 5
                    Rectangle { width: 14; height: 2; y: 7; color: modelData[1]; opacity: modelData[2] ? 0.8 : 1 }
                    Text { text: modelData[0]; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                }
            }
            Row {
                visible: specCard.open && root.p.gateFocus > 0.5
                spacing: 5
                Rectangle { width: 14; height: 10; y: 2; color: Theme.alpha(Theme.gate, 0.25) }
                Text { text: "listening band"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
            }
            Item { Layout.fillWidth: true }
            HelpButton { text: "Left = low pitch, right = high pitch; higher = louder. The shaded area is the band the gate listens to: Setup picks the range where your voice (green, dashed) stands furthest above other voices (pink) and the room (grey)." }
            PillButton { text: specCard.open ? "Hide" : "Show"; implicitHeight: Theme.controlHeightSm; onClicked: specCard.open = !specCard.open }
        }
        SpectrumView {
            visible: specCard.open
            Layout.fillWidth: true
            Layout.preferredHeight: root.height < 700 ? 100 : 170
            onVisibleChanged: controller.spectrum.active = visible && root.visible
        }
    }
    onVisibleChanged: controller.spectrum.active = visible && specCard.open && root.gateIn
    Component.onCompleted: controller.spectrum.active = visible && specCard.open && root.gateIn
    Component.onDestruction: controller.spectrum.active = false

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
