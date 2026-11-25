import QtQuick
import QtQuick.Layouts

StagePanel {
    id: root
    title: "AI Noise Removal"
    accent: "#7ee0c3"
    enableParam: "noiseOn"
    effectName: "noise"
    visualWidth: 220
    summary: "A small neural network (RNNoise) that removes steady background noise (PC fans, keyboard, hum, traffic) while you talk. It can't remove other people's voices, because that's speech too. The Voice Gate handles that."
    help: "It runs before everything else, so the gate and compressor get a cleaner signal. It adds 10 ms of delay. At 100% it can sound slightly processed; 60–80% keeps more natural detail."

    property real vad: 0
    Connections {
        target: controller
        function onMetersChanged() { root.vad = Math.max(controller.noiseVad, root.vad - 0.04) }
    }

    visual: ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Text { text: "Voice detected"; color: Theme.textDim; font.pixelSize: 12 }
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 18
            radius: 9
            color: Theme.track
            Rectangle {
                height: parent.height; radius: 9
                width: parent.width * root.vad
                color: "#7ee0c3"
                Behavior on width { NumberAnimation { duration: 60 } }
            }
        }
        Text {
            text: Math.round(root.vad * 100) + "%"
            color: Theme.text
            font.pixelSize: 28; font.weight: Font.Bold; font.family: Theme.mono
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textFaint
            font.pixelSize: 11
            text: "How sure the AI is that someone is speaking. It should jump up when anyone talks and drop near 0 in silence."
        }
        Item { Layout.fillHeight: true }
    }

    ParamSlider {
        paramKey: "noiseAmount"; label: "Strength"; from: 0; to: 100; stepSize: 1
        accent: "#7ee0c3"
        format: (v) => Math.round(v) + " %"
        help: "How much of the cleaned signal you hear. 100% = fully cleaned. Lower blends some of the original back in for a more natural sound."
    }
}
