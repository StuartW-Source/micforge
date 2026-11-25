import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

StagePanel {
    title: "Input"
    accent: Theme.input
    visualWidth: 90
    summary: "Choose your microphone and get a healthy level. Aim for the meter peaking in the yellow when you talk normally."
    help: "On a 2-input audio interface (Focusrite, PreSonus, Behringer…) Input 1 is usually the LEFT channel and Input 2 the RIGHT. USB mics and headsets: choose Both.\n\nGain: first set the gain knob on your interface or mic (if it has one) so normal speech lights the meter around −18 to −9 dB, then fine-tune here. If the meter hits red, you're clipping (distorting): turn it down."

    visual: LevelMeter {
        anchors.fill: parent
        level: controller.inPeak
        label: "IN"
        accent: Theme.input
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Microphone"; color: Theme.textDim; font.pixelSize: 12 }
        ComboBox {
            Layout.fillWidth: true
            model: controller.sources
            textRole: "description"
            currentIndex: {
                for (var i = 0; i < controller.sources.length; ++i)
                    if (controller.sources[i].name === controller.currentSource) return i
                return -1
            }
            displayText: currentIndex < 0 ? "Choose a microphone…" : currentText
            onActivated: (i) => controller.currentSource = controller.sources[i].name
        }
    }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Which input is the mic plugged into?"; color: Theme.textDim; font.pixelSize: 12 }
        Segmented {
            Layout.fillWidth: true
            paramKey: "inputChannel"
            options: ["Input 1 (L)", "Input 2 (R)", "Both"]
            accent: Theme.input
        }
    }
    ParamSlider {
        paramKey: "inputGain"; label: "Gain"; from: -12; to: 36; stepSize: 0.5
        accent: Theme.input
        format: (v) => Theme.fmtDb(v)
        help: "Digital boost on top of your interface's gain knob. Setup sets this for you."
    }
    Text {
        id: levelHint
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignBottom
        wrapMode: Text.WordWrap
        font.pixelSize: 13
        property real recent: -120
        Connections {
            target: controller
            function onMetersChanged() { levelHint.recent = Math.max(controller.inPeak, levelHint.recent - 0.15) }
        }
        color: recent > -1 ? Theme.danger : recent > -24 ? Theme.gate : Theme.textFaint
        text: !controller.captureRunning ? "No signal from the microphone yet."
            : recent > -1 ? "Too loud: it's clipping. Turn the gain down."
            : recent > -24 ? "✓ Good level."
            : recent > -45 ? "A bit quiet. Speak up or raise the gain."
            : "Say something to test your level."
    }
}
