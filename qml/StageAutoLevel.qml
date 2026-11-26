import QtQuick
import QtQuick.Layouts

StagePanel {
    id: root
    title: "Auto Level"
    accent: Theme.autolevel
    enableParam: "autoLevelOn"
    effectName: "autoLevel"
    visualWidth: 220
    summary: "Keeps your volume steady automatically. If you lean back, speak softly or get excited, it gently adjusts so people always hear you at the same level. It pauses while you're silent, so it never turns up background noise."
    help: "Unlike the compressor, which reacts to every word, Auto Level works slowly, over a second or more, like a sound engineer riding the fader. Use both together: Auto Level for the overall level, the compressor for quick peaks."

    property real shown: 0
    Connections {
        target: controller
        function onMetersChanged() { root.shown = root.shown * 0.8 + controller.autoLevelGain * 0.2 }
    }

    visual: ColumnLayout {
        anchors.fill: parent
        spacing: 8
        Text { text: "Currently adjusting by"; color: Theme.textDim; font.pixelSize: 12 }
        Text {
            text: (root.shown >= 0 ? "+" : "−") + Math.abs(root.shown).toFixed(1) + " dB"
            color: Theme.autolevel; font.pixelSize: 30; font.weight: Font.Bold; font.family: Theme.mono
        }
        // Centre-zero bar: left = turning down, right = turning up.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 14
            radius: 7
            color: Theme.track
            Rectangle { x: parent.width / 2 - 1; width: 2; height: parent.height; color: Theme.textFaint }
            Rectangle {
                readonly property real frac: Math.max(-1, Math.min(1, root.shown / Math.max(1, controller.params.autoLevelMaxGain)))
                x: frac >= 0 ? parent.width / 2 : parent.width / 2 + frac * parent.width / 2
                width: Math.abs(frac) * parent.width / 2
                height: parent.height; radius: 7
                color: Theme.autolevel
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { text: "turning down"; color: Theme.textFaint; font.pixelSize: 10 }
            Item { Layout.fillWidth: true }
            Text { text: "turning up"; color: Theme.textFaint; font.pixelSize: 10 }
        }
        Item { Layout.fillHeight: true }
    }

    ParamSlider {
        paramKey: "autoLevelTarget"; label: "Target level"; from: -36; to: -6; stepSize: 0.5
        accent: Theme.autolevel
        format: (v) => Theme.fmtDb(v)
        help: "How loud your speech should sit on average. −18 dB suits most voice chat."
    }
    ParamSlider {
        paramKey: "autoLevelMaxGain"; label: "Max adjustment"; from: 0; to: 24; stepSize: 0.5
        accent: Theme.autolevel
        format: (v) => "± " + v.toFixed(1) + " dB"
        help: "The most it will turn you up or down."
    }
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 6
        Text { text: "Speed"; color: Theme.textDim; font.pixelSize: 12 }
        Segmented {
            Layout.fillWidth: true
            paramKey: "autoLevelSpeed"
            options: ["Slow & smooth", "Medium", "Fast"]
            accent: Theme.autolevel
        }
    }
}
