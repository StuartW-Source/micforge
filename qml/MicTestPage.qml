import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Mic test: record yourself once, then compare the raw mic with MicForge's processed sound.
Flickable {
    id: root
    property var app
    readonly property var t: controller.micTest
    readonly property var a: t.analysis
    readonly property bool hasTake: t.takeSeconds > 0
    property string exportMsg: ""

    contentHeight: body.implicitHeight + Theme.sp4
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    // Leaving the page stops recording/playback.
    onVisibleChanged: if (!visible && (t.state === "recording" || t.state === "playing")) t.stop()

    function db(v) { return v === undefined || v <= -119 ? "—" : (v > 0 ? "+" : "") + v.toFixed(1) + " dB" }

    FolderDialog {
        id: folderDialog
        onAccepted: root.exportMsg = t.exportWav(selectedFolder)
    }

    ColumnLayout {
        id: body
        width: Math.min(root.width, 1100)
        spacing: Theme.sp4

        Card {
            Layout.fillWidth: true
            title: "Hear what MicForge does"
            subtitle: "Talk normally for a few seconds, the way you would in a call. MicForge records your raw microphone and the processed result at the same time, so you can compare them. Playback goes only to your speakers or headphones: apps using MicForge never hear it. Nothing is saved unless you export it."

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.sp3
                PillButton {
                    text: t.state === "recording" ? "Stop" : root.hasTake ? "Record again" : "Record"
                    icon: t.state === "recording" ? "■" : "●"
                    primary: t.state !== "recording"
                    accent: Theme.danger
                    enabled: controller.captureRunning || t.state === "recording"
                    onClicked: t.state === "recording" ? t.stop() : t.record()
                }
                Text { text: "Length"; color: Theme.textDim; font.pixelSize: Theme.fsBody }
                Segmented {
                    Layout.preferredWidth: 220
                    options: ["5 s", "10 s", "20 s", "30 s"]
                    selectedIndex: [5, 10, 20, 30].indexOf(t.seconds)
                    enabled: t.state !== "recording"
                    accent: Theme.focus
                    onChosen: (i) => t.seconds = [5, 10, 20, 30][i]
                    Accessible.name: "Recording length"
                }
                Item { Layout.fillWidth: true }
                Text {
                    visible: !controller.captureRunning
                    text: "No sound is arriving from the microphone."
                    color: Theme.warn; font.pixelSize: Theme.fsBody
                }
            }
            ColumnLayout {
                visible: t.state === "recording"
                Layout.fillWidth: true
                spacing: Theme.sp1
                Text { text: "Recording… talk now (" + Math.ceil(t.seconds * (1 - t.progress)) + " s left)"; color: Theme.danger; font.pixelSize: Theme.fsBodyLg; font.weight: Font.DemiBold }
                Rectangle {
                    Layout.fillWidth: true; implicitHeight: 6; radius: 3; color: Theme.track
                    Rectangle { width: parent.width * t.progress; height: parent.height; radius: 3; color: Theme.danger }
                }
            }
        }

        // ------------------------------------------------------------ the two takes
        Repeater {
            model: [
                { which: 1, title: "Raw microphone", sub: "What your mic sends before MicForge (input gain only).", accent: Theme.warn, key: "raw" },
                { which: 2, title: "Processed by MicForge", sub: "What your apps hear.", accent: Theme.gate, key: "processed" }
            ]
            Card {
                Layout.fillWidth: true
                visible: root.hasTake
                accent: modelData.accent
                readonly property var s: root.a[modelData.key] || ({})
                readonly property bool isPlaying: t.playing === modelData.which
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.sp3
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: modelData.title; color: Theme.text; font.pixelSize: Theme.fsTitle; font.weight: Font.Bold }
                        Text { text: modelData.sub; color: Theme.textDim; font.pixelSize: Theme.fsBody; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                    PillButton {
                        text: isPlaying ? "Stop" : "Play"
                        icon: isPlaying ? "■" : "▶"
                        primary: !isPlaying
                        accent: modelData.accent
                        enabled: t.state !== "recording"
                        onClicked: isPlaying ? t.stop() : t.play(modelData.which)
                        Accessible.name: (isPlaying ? "Stop " : "Play ") + modelData.title
                    }
                }
                WaveView {
                    Layout.fillWidth: true
                    values: modelData.which === 1 ? t.rawWave : t.processedWave
                    accent: modelData.accent
                    playhead: isPlaying ? t.progress : -1
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.sp5
                    Repeater {
                        model: [
                            ["Peak", root.db(s.peak)],
                            ["Speech level", root.db(s.speech)],
                            ["Background", s.floor !== undefined && s.floor < -90 ? "silent" : root.db(s.floor)],
                            ["Voice above background", s.range === undefined ? "—" : s.floor < -90 ? "> 80 dB" : s.range.toFixed(0) + " dB"]
                        ]
                        Column {
                            spacing: 1
                            Text { text: modelData[0]; color: Theme.textFaint; font.pixelSize: Theme.fsSmall }
                            Text { text: modelData[1]; color: Theme.text; font.pixelSize: Theme.fsBodyLg; font.family: Theme.mono; font.weight: Font.DemiBold }
                        }
                    }
                }
            }
        }

        Card {
            visible: root.hasTake
            Layout.fillWidth: true
            title: "What this test shows"
            Repeater {
                model: root.a.notes || []
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.sp2
                    Text { text: "●"; color: Theme.focus; font.pixelSize: Theme.fsSmall; Layout.alignment: Qt.AlignTop; Layout.topMargin: 2 }
                    Text { text: modelData; color: Theme.text; font.pixelSize: Theme.fsBody; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }
            SettingRow {
                title: "Match loudness when comparing"
                sub: "Plays the raw take at the same loudness as the processed one (" + ((root.a.matchGain || 0) >= 0 ? "+" : "") + (root.a.matchGain || 0).toFixed(1) + " dB), so you judge the sound, not the volume."
                checked: t.matchLevels
                onToggled: (on) => t.matchLevels = on
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.sp2
                PillButton { text: "Export WAV files…"; onClicked: { root.exportMsg = ""; folderDialog.open() } }
                PillButton { text: "Discard"; onClicked: { root.exportMsg = ""; t.discard() } }
                Text { visible: root.exportMsg !== ""; text: root.exportMsg; color: Theme.textDim; font.pixelSize: Theme.fsBody; height: Theme.controlHeight; verticalAlignment: Text.AlignVCenter }
            }
        }
    }
}
