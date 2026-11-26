import QtQuick
import QtQuick.Layouts

StagePanel {
    id: root
    title: "Hum Removal"
    accent: Theme.hum
    enableParam: "humOn"
    effectName: "hum"
    visualWidth: 0
    summary: "Removes the steady electrical buzz (mains hum) that cables, USB power, chargers and ground loops add: 50 Hz in most of the world, 60 Hz in North America and parts of Asia, plus its overtones."
    help: "It cuts very narrow slices (3 Hz wide) at the hum frequency and its overtones, so your voice is untouched. In Automatic mode it only locks on when it hears a steady, pure hum for two seconds; your voice can't fool it."

    ColumnLayout {
        Layout.fillWidth: true
        Layout.columnSpan: 2
        spacing: 6
        Text { text: "Mains frequency"; color: Theme.textDim; font.pixelSize: Theme.fsBody }
        Segmented {
            Layout.fillWidth: true
            Layout.maximumWidth: 420
            paramKey: "humFreq"
            options: ["Automatic", "50 Hz", "60 Hz"]
            accent: Theme.hum
        }
        Text {
            text: root.p.humFreq > 0.5 ? "Removing " + (root.p.humFreq > 1.5 ? 60 : 50) + " Hz hum."
                : controller.humBase > 0 ? "Found " + controller.humBase + " Hz hum, removing it."
                : "Listening for hum… (nothing found yet: there may be none)"
            color: controller.humBase > 0 || root.p.humFreq > 0.5 ? Theme.hum : Theme.textFaint
            font.pixelSize: Theme.fsBody
        }
    }
    readonly property var p: controller.params
    ParamSlider {
        paramKey: "humHarmonics"; label: "Overtones removed"; from: 1; to: 10; stepSize: 1
        accent: Theme.hum
        format: (v) => Math.round(v) === 1 ? "just the hum" : Math.round(v) + " (up to " + Math.round(v * (controller.humBase > 0 ? controller.humBase : 50)) + " Hz)"
        help: "Hum has overtones at 2×, 3×, 4×… its frequency. 6 covers most buzz; raise it if you still hear a higher-pitched buzz."
    }
}
