import QtQuick
import QtQuick.Layouts

StagePanel {
    id: root
    title: "Output"
    accent: Theme.output
    visualWidth: 140
    summary: "The final volume sent to your apps. The safety limiter stops distortion if you suddenly shout or laugh."
    help: "Aim for the OUT meter peaking around −12 to −6 dB in normal speech. The LIMIT meter shows when the limiter catches a peak. Occasional flickers are fine."

    visual: RowLayout {
        anchors.fill: parent
        spacing: 10
        LevelMeter {
            Layout.fillHeight: true
            Layout.fillWidth: true
            level: controller.outPeak
            label: "OUT"
            accent: Theme.output
            hasThreshold: controller.params.limiterOn > 0.5 && controller.params.bypass < 0.5
            threshold: controller.params.limiterCeiling
            thresholdColor: Theme.danger
        }
        ReductionMeter {
            Layout.fillHeight: true
            reduction: controller.limReduction
            maxDb: 12
            label: "LIMIT"
            accent: Theme.danger
        }
    }

    ParamSlider {
        paramKey: "outputGain"; label: "Output volume"; from: -24; to: 24; stepSize: 0.5; bipolar: true
        accent: Theme.output
        format: (v) => Theme.fmtDb(v)
    }
    RowLayout {
        Layout.fillWidth: true
        Text { text: "Safety limiter"; color: Theme.textDim; font.pixelSize: 12; Layout.fillWidth: true }
        HelpButton { text: "A very fast, very strong compressor that makes sure nothing goes above the ceiling (red line), so you can't clip or distort. Leave it on." }
        ToggleSwitch { paramKey: "limiterOn"; accent: Theme.output }
    }
    ParamSlider {
        enabled: controller.params.limiterOn > 0.5
        paramKey: "limiterCeiling"; label: "Limiter ceiling"; from: -12; to: 0; stepSize: 0.5
        accent: Theme.output
        format: (v) => Theme.fmtDb(v)
        help: "The maximum level allowed out. −1 dB leaves a little room for the app's own processing."
    }
}
