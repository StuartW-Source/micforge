import QtQuick
import QtQuick.Layouts

StagePanel {
    title: "Doubler"
    accent: Theme.doubler
    enableParam: "doublerOn"
    effectName: "doubler"
    visualWidth: 0
    summary: "Layers two slightly different copies of your voice behind you, like a singer recording the same line twice. Sounds thicker, smoother and bigger."
    help: "Low mix (20–40%) = subtle thickening for voice. High width and mix = obvious chorus effect, fun for singing or character voices."

    ParamSlider {
        paramKey: "doublerMix"; label: "Amount"; from: 0; to: 100; stepSize: 1
        accent: Theme.doubler
        format: (v) => Math.round(v) + " %"
    }
    ParamSlider {
        paramKey: "doublerWidth"; label: "Width · movement"; from: 0; to: 100; stepSize: 1
        accent: Theme.doubler
        format: (v) => Math.round(v) + " %"
        help: "How different the copies are from you. More = more obvious, swirling chorus."
    }
}
