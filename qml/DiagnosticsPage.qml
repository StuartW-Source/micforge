import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Diagnostics: is the audio path healthy, what went wrong, and a report that's safe to share.
Flickable {
    id: root
    property var app
    readonly property var d: controller.diagnostics
    readonly property var i: d.info
    readonly property int cols: width > 1100 ? 3 : width > 700 ? 2 : 1
    property bool withDevices: false
    property bool withLog: false
    property string toast: ""

    contentHeight: body.implicitHeight + Theme.sp4
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    onVisibleChanged: d.active = visible
    Component.onCompleted: { d.active = visible; logText.text = d.logTail(200) }
    Component.onDestruction: d.active = false

    function fmt(v, digits, unit) { return v === undefined ? "—" : Number(v).toFixed(digits) + (unit ? " " + unit : "") }
    function since(s) {
        if (s === undefined || s < 0) return "never"
        if (s < 60) return Math.round(s) + " s ago"
        if (s < 3600) return Math.round(s / 60) + " min ago"
        return Math.round(s / 3600) + " h ago"
    }
    Timer { id: toastTimer; interval: 3000; onTriggered: root.toast = "" }
    function say(t) { toast = t; toastTimer.restart() }

    FileDialog {
        id: saveDialog
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: ["Text files (*.txt)"]
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        onVisibleChanged: if (visible) selectedFile = currentFolder + "/micforge-report.txt"
        onAccepted: root.say(d.saveReport(selectedFile, root.withDevices, root.withLog) ? "Report saved." : "Couldn't save the report.")
    }

    ColumnLayout {
        id: body
        width: root.width
        spacing: Theme.sp4

        // ------------------------------------------------------------ summary + report
        Card {
            Layout.fillWidth: true
            accent: (i.issues || []).length === 0 ? Theme.gate : Theme.warn
            title: (i.issues || []).length === 0 ? "No problems found" : (i.issues.length === 1 ? "1 thing to look at" : i.issues.length + " things to look at")
            subtitle: (i.issues || []).length === 0 ? "Audio is flowing normally. Counters below cover this session; reset them before reproducing a problem." : ""
            Repeater {
                model: i.issues || []
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.sp2
                    Text { text: "●"; color: Theme.warn; font.pixelSize: Theme.fsSmall; Layout.alignment: Qt.AlignTop; Layout.topMargin: 2 }
                    Text { text: modelData; color: Theme.text; font.pixelSize: Theme.fsBody; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textDim; font.pixelSize: Theme.fsBody
                text: "A report helps when asking for help. It never contains audio, recordings or your sound settings, and your home folder, user name, computer name, preset names and plugin file locations are removed."
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.sp4
                RowLayout {
                    spacing: Theme.sp2
                    ToggleSwitch { checked: root.withDevices; label: "Include device names"; accent: Theme.focus; onToggled: (on) => root.withDevices = on }
                    Text { text: "Include device names"; color: Theme.text; font.pixelSize: Theme.fsBody }
                }
                RowLayout {
                    spacing: Theme.sp2
                    ToggleSwitch { checked: root.withLog; label: "Include recent log lines"; accent: Theme.focus; onToggled: (on) => root.withLog = on }
                    Text { text: "Include recent log lines"; color: Theme.text; font.pixelSize: Theme.fsBody }
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.sp2
                PillButton { text: "Copy report"; primary: true; onClicked: { d.refresh(); d.copyReport(root.withDevices, root.withLog); root.say("Report copied to the clipboard.") } }
                PillButton { text: "Save report…"; onClicked: { d.refresh(); saveDialog.open() } }
                PillButton { text: "Preview"; checkable: true; checked: preview.visible; onClicked: { d.refresh(); preview.visible = !preview.visible } }
                PillButton { text: "Reset counters"; onClicked: d.resetCounters() }
                Text { visible: root.toast !== ""; text: root.toast; color: Theme.gate; font.pixelSize: Theme.fsBody; height: Theme.controlHeight; verticalAlignment: Text.AlignVCenter }
            }
            Rectangle {
                id: preview
                visible: false
                Layout.fillWidth: true
                implicitHeight: 260
                radius: Theme.radiusSm + 2
                color: Theme.well
                border.color: Theme.border
                ScrollView {
                    anchors { fill: parent; margins: Theme.sp2 }
                    TextArea {
                        readOnly: true
                        text: preview.visible ? (root.i, d.report(root.withDevices, root.withLog)) : ""
                        color: Theme.text
                        font.family: Theme.mono; font.pixelSize: Theme.fsSmall
                        wrapMode: TextEdit.NoWrap
                        selectByMouse: true
                        background: null
                    }
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.cols
            columnSpacing: Theme.sp4
            rowSpacing: Theme.sp4

            Card {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.alignment: Qt.AlignTop
                title: "Audio engine"
                InfoRow { label: "State"; value: i.appStateText || "" }
                InfoRow { label: "Audio system"; value: (i.backend || "") + (i.backendVersion ? " · " + i.backendVersion : "") }
                InfoRow { label: "Engine"; value: (i.engineState || "") + (i.engineError ? " (" + i.engineError + ")" : ""); valueColor: i.engineState === "running" ? Theme.text : Theme.warn }
                InfoRow { label: "Running for"; value: i.uptimeS === undefined ? "—" : Math.floor(i.uptimeS / 60) + " min " + (i.uptimeS % 60) + " s" }
                InfoRow { label: "MicForge"; value: (i.appVersion || "") + (i.safeMode ? " · safe mode" : "") + " · Qt " + (i.qtVersion || "") }
                InfoRow { label: "System"; value: i.os || "" }
            }
            Card {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.alignment: Qt.AlignTop
                title: "Devices"
                InfoRow { label: "Microphone"; value: (i.inputName || "") + " (" + (i.inputKind || "") + ")" }
                InfoRow { label: "Channel"; value: i.inputChannel || "" }
                InfoRow { label: "Sound arriving"; value: i.captureRunning ? "Yes" : "No"; valueColor: i.captureRunning ? Theme.text : Theme.warn }
                InfoRow { label: "Microphones found"; value: i.inputCount === undefined ? "—" : String(i.inputCount) }
                InfoRow { label: "Processed mic"; value: (i.virtualMicName || "") + (i.virtualMicInUse ? " · in use" : " · not in use") }
                InfoRow { label: "Listening"; value: i.monitor ? "On" : "Off" }
            }
            Card {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.alignment: Qt.AlignTop
                title: "Timing"
                InfoRow { labelWidth: 90; label: "Sample rate"; value: root.fmt(i.processingRate, 0, "Hz") + (i.graphRate && i.graphRate !== i.processingRate ? " (device " + i.graphRate + " Hz, resampled)" : "") }
                InfoRow { labelWidth: 90; label: "Block size"; value: i.quantum ? i.quantum + " frames · " + root.fmt(i.quantumMs, 1, "ms") : "—" }
                InfoRow { labelWidth: 90; label: "Latency"; value: "in " + root.fmt(i.inputLatencyMs, 1, "ms") + " · buffer " + root.fmt(i.bufferMs, 1, "ms") + " · out " + (i.virtualMicInUse ? root.fmt(i.outputLatencyMs, 1, "ms") : "— (no app)") }
                MetricBar {
                    label: "Processing load (last second)"
                    value: root.fmt(i.loadPct, 1, "%")
                    fraction: (i.loadPct || 0) / 100
                }
                MetricBar {
                    label: "Worst this session"
                    value: root.fmt(i.loadSessionPct, 1, "%") + " · " + root.fmt(i.procUsSessionMax, 0, "µs")
                    fraction: (i.loadSessionPct || 0) / 100
                }
                InfoRow { labelWidth: 90; label: "Average time"; value: root.fmt(i.procUsAvg, 0, "µs per block") }
            }
            Card {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.alignment: Qt.AlignTop
                title: "Glitches this session"
                subtitle: "Each of these can be heard as a click, crackle or gap."
                InfoRow { label: "Late input blocks"; value: String(i.lateCallbacks || 0); valueColor: (i.lateCallbacks || 0) > 0 ? Theme.warn : Theme.text }
                InfoRow { label: "Processed mic ran dry"; value: String(i.vmicUnderruns || 0); valueColor: (i.vmicUnderruns || 0) > 0 ? Theme.warn : Theme.text }
                InfoRow { label: "Monitor ran dry"; value: String(i.monitorUnderruns || 0) }
                InfoRow { label: "Mic clipped"; value: (i.inputClips || 0) + " samples · last " + root.since(i.secondsSinceClip); valueColor: (i.inputClips || 0) > 0 ? Theme.warn : Theme.text }
            }
            Card {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.alignment: Qt.AlignTop
                title: "Plugins"
                Text { visible: (i.plugins || []).length === 0; text: "No plugins loaded."; color: Theme.textDim; font.pixelSize: Theme.fsBody }
                Repeater {
                    model: i.plugins || []
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text {
                            Layout.fillWidth: true; elide: Text.ElideRight
                            text: modelData.name + "  ·  " + modelData.format + (modelData.bypass ? "  ·  bypassed" : "") + (modelData.loaded ? "" : "  ·  not loaded")
                            color: modelData.error ? Theme.danger : Theme.text; font.pixelSize: Theme.fsBody
                        }
                        Text { visible: !!modelData.error; text: modelData.error; color: Theme.danger; font.pixelSize: Theme.fsSmall; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    }
                }
            }
            Card {
                Layout.fillWidth: true; Layout.fillHeight: true; Layout.alignment: Qt.AlignTop
                title: "Optional features"
                Text {
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                    text: (i.unavailable || []).length === 0 ? "All available (AI noise removal, LV2 and VST3 plugins)." : i.unavailable.join("\n\n")
                    color: (i.unavailable || []).length === 0 ? Theme.textDim : Theme.warn; font.pixelSize: Theme.fsBody
                }
            }
        }

        // ------------------------------------------------------------ log
        Card {
            Layout.fillWidth: true
            title: "Log"
            subtitle: i.logPath || ""
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 280
                radius: Theme.radiusSm + 2
                color: Theme.well
                border.color: Theme.border
                ScrollView {
                    id: logScroll
                    anchors { fill: parent; margins: Theme.sp2 }
                    TextArea {
                        id: logText
                        readOnly: true
                        color: Theme.text
                        font.family: Theme.mono; font.pixelSize: Theme.fsSmall
                        wrapMode: TextEdit.NoWrap
                        selectByMouse: true
                        background: null
                        Accessible.name: "Recent log lines"
                    }
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.sp2
                PillButton { text: "Refresh"; implicitHeight: Theme.controlHeightSm; onClicked: { logText.text = d.logTail(200); logScroll.ScrollBar.vertical.position = 1 } }
                PillButton { text: "Open log folder"; implicitHeight: Theme.controlHeightSm; onClicked: controller.openLogFolder() }
            }
        }
    }
}
