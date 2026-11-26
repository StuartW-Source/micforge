import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Audio devices: which mic MicForge listens to, what it sends to apps, monitoring and the audio engine.
RowLayout {
    id: root
    property var app
    readonly property bool wide: width > 1050
    spacing: Theme.sp3

    Loader {
        Layout.fillWidth: true
        Layout.fillHeight: true
        source: "StageInput.qml"
    }

    Flickable {
        Layout.preferredWidth: root.wide ? 360 : 300
        Layout.fillHeight: true
        contentHeight: side.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: side
            width: parent.width
            spacing: Theme.sp3

            Card {
                Layout.fillWidth: true
                title: "Processed microphone"
                subtitle: "Apps receive your processed voice from this virtual microphone. Choose it as the input device in Discord, OBS, your browser or game."
                InfoRow { label: "Name"; value: controller.virtualMicName }
                InfoRow {
                    label: "Status"
                    value: controller.virtualMicInUse ? "In use by an app" : "Ready, no app connected"
                    valueColor: controller.virtualMicInUse ? Theme.gate : Theme.text
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textFaint; font.pixelSize: Theme.fsSmall
                    text: "Turn off the app's own noise suppression, echo cancellation and automatic gain: MicForge already does this, and doing it twice makes your voice sound worse."
                }
            }

            Card {
                Layout.fillWidth: true
                title: "Listen to yourself"
                subtitle: "Plays the processed sound to your default speakers or headphones. Use headphones, or you'll hear feedback."
                SettingRow {
                    title: "Monitoring"
                    sub: controller.monitor ? "On: you hear what apps hear." : "Off"
                    checked: controller.monitor
                    onToggled: (on) => controller.monitor = on
                }
            }

            Card {
                Layout.fillWidth: true
                title: "Audio engine"
                InfoRow {
                    label: "Status"
                    value: controller.engineOk ? "Connected to " + controller.backendName
                                               : controller.engineState + (controller.engineError ? " (" + controller.engineError + ")" : "")
                    valueColor: controller.engineOk ? Theme.text : Theme.warn
                }
                InfoRow {
                    label: "Microphone"
                    value: controller.captureRunning ? "Receiving sound" : "Waiting for the microphone"
                    valueColor: controller.captureRunning ? Theme.text : Theme.warn
                }
                PillButton { text: "Restart audio engine"; implicitHeight: Theme.controlHeightSm; onClicked: controller.restartEngine() }
            }
        }
    }
}
