import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Overview: everything needed day to day on one page — is it working, what is it doing, quick controls.
Flickable {
    id: root
    property var app                    // the main window (stage definitions, helpers, dialogs)
    readonly property var p: controller.params
    readonly property bool wide: width > 1060
    readonly property int cols: width > 1060 ? 3 : width > 640 ? 2 : 1
    readonly property var zones: controller.zones

    contentHeight: body.implicitHeight + Theme.sp2
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    function sourceName() {
        for (var i = 0; i < controller.sources.length; ++i)
            if (controller.sources[i].name === controller.currentSource) return controller.sources[i].description
        return controller.currentSource === "" ? "None selected" : "Not connected"
    }
    readonly property color stateColor: {
        switch (controller.appState) {
        case "processing": return controller.virtualMicInUse ? Theme.gate : Theme.input
        case "muted": return Theme.danger
        case "bypassed": case "connecting": case "no-input": return Theme.warn
        }
        return Theme.danger
    }

    ColumnLayout {
        id: body
        width: root.width
        spacing: Theme.sp4

        // Getting started.
        Rectangle {
            visible: controller.showWelcome
            Layout.fillWidth: true
            implicitHeight: welcome.implicitHeight + 2 * Theme.sp4
            radius: Theme.radius
            color: Theme.tint(Theme.input)
            border.color: Theme.tintBorder(Theme.input)
            GridLayout {
                id: welcome
                anchors { left: parent.left; right: parent.right; top: parent.top; margins: Theme.sp4; rightMargin: 36 }
                columns: root.wide ? 3 : 1
                columnSpacing: Theme.sp5
                rowSpacing: Theme.sp3
                Repeater {
                    model: [
                        ["1", "Run Setup", "Tell MicForge about your mic and room, then talk for a few seconds. It tunes everything to your voice."],
                        ["2", "Choose MicForge in your app", "In Discord, OBS or any other app pick “" + controller.virtualMicName + "” as the input, and turn OFF their noise suppression, echo cancellation and automatic gain."],
                        ["3", "Fine-tune by ear", "Effects lets you adjust each step. Listen = hear yourself, Compare = raw vs processed. Hover any ? for help."]
                    ]
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        Layout.alignment: Qt.AlignTop
                        spacing: Theme.sp3
                        Rectangle {
                            Layout.alignment: Qt.AlignTop
                            width: 24; height: 24; radius: 12
                            color: Theme.input
                            Text { anchors.centerIn: parent; text: modelData[0]; font.bold: true; color: Theme.onAccent }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: modelData[1]; color: Theme.text; font.pixelSize: Theme.fsBodyLg; font.weight: Font.Bold }
                            Text { text: modelData[2]; color: Theme.textDim; font.pixelSize: Theme.fsBody; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                    }
                }
            }
            PillButton {
                anchors { right: parent.right; top: parent.top; margins: Theme.sp2 }
                text: "✕"
                implicitHeight: 26; implicitWidth: 26
                onClicked: controller.showWelcome = false
                Accessible.name: "Hide getting-started tips"
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.cols
            columnSpacing: Theme.sp4
            rowSpacing: Theme.sp4

            // ---------------------------------------------------------- status
            Card {
                Layout.fillWidth: true
                Layout.columnSpan: root.cols === 3 ? 2 : 1
                Layout.fillHeight: true
                accent: root.stateColor

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.sp3
                    Rectangle {
                        width: 12; height: 12; radius: 6
                        color: root.stateColor
                        SequentialAnimation on opacity {
                            running: controller.appState === "processing" && controller.virtualMicInUse
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 900 }
                            NumberAnimation { to: 1; duration: 900 }
                            onStopped: parent.opacity = 1
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: controller.appStateText
                        color: Theme.text
                        font.pixelSize: Theme.fsHeading
                        font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                    }
                }
                InfoRow { label: "Microphone"; value: root.sourceName() + "  ·  " + ["input 1", "input 2", "both inputs"][Math.round(root.p.inputChannel)] }
                InfoRow {
                    label: "Apps hear"
                    value: controller.virtualMicName + (controller.virtualMicInUse ? "  ·  in use" : "  ·  no app connected")
                    valueColor: controller.virtualMicInUse ? Theme.gate : Theme.text
                }
                InfoRow {
                    label: "Audio engine"
                    value: controller.engineOk ? "Connected to " + controller.backendName
                                               : controller.engineState + (controller.engineError ? " (" + controller.engineError + ")" : "")
                    valueColor: controller.engineOk ? Theme.text : Theme.warn
                }
                InfoRow { label: "Sound profile"; value: controller.currentPreset === "" ? "Custom (edited)" : controller.currentPreset }

                Flow {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.sp1
                    spacing: Theme.sp2
                    PillButton {
                        text: controller.muted ? "Unmute" : "Mute"
                        icon: controller.muted ? "⊘" : "●"
                        checkable: true
                        checked: controller.muted
                        accent: Theme.danger
                        onClicked: controller.toggleMute()
                    }
                    PillButton {
                        text: app.bypassed ? "Hearing raw mic" : "Compare with raw mic"
                        icon: "⇄"
                        checkable: true
                        checked: app.bypassed
                        accent: Theme.warn
                        onClicked: controller.setParam("bypass", app.bypassed ? 0 : 1)
                    }
                    PillButton {
                        text: "Listen to myself"
                        icon: "🎧"
                        checkable: true
                        checked: controller.monitor
                        accent: Theme.output
                        onClicked: controller.monitor = !controller.monitor
                    }
                }
            }

            // ---------------------------------------------------------- meters
            Card {
                Layout.fillWidth: true
                Layout.preferredWidth: root.cols > 1 ? 300 : -1
                Layout.maximumWidth: root.cols > 1 ? 340 : 100000
                Layout.fillHeight: true
                title: "Levels"
                RowLayout {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 360
                    Layout.preferredHeight: 200
                    spacing: Theme.sp2
                    LevelMeter { Layout.fillHeight: true; Layout.fillWidth: true; level: controller.inPeak; label: "MIC IN"; accent: Theme.input }
                    LevelMeter { Layout.fillHeight: true; Layout.fillWidth: true; showScale: false; level: controller.outPeak; label: "TO APPS"; accent: Theme.output }
                }
            }

            // ---------------------------------------------------------- voice isolation
            Card {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.columnSpan: root.cols === 2 ? 2 : 1
                title: "Voice isolation"
                subtitle: "Keeps other people, keyboards and fans off your mic."
                readonly property bool gateIn: root.p.gateAdded > 0.5
                readonly property bool gateOn: gateIn && root.p.gateOn > 0.5

                GateLight { Layout.fillWidth: true; app: root.app }
                InfoRow {
                    label: "Tuned to you"
                    value: root.zones.valid ? "Yes" + (root.zones.others > root.zones.room ? ", including other voices" : "") : "Not yet: run Setup"
                    valueColor: root.zones.valid ? Theme.text : Theme.warn
                }
                InfoRow {
                    label: "Closeness check"
                    value: root.p.gateCloseCheck > 0.5 ? "On" : "Off"
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.sp2
                    PillButton { text: root.zones.valid ? "Re-run Setup" : "Run Setup"; icon: "✦"; primary: !root.zones.valid; accent: Theme.gate; enabled: !controller.safeMode; onClicked: app.openSetup() }
                    PillButton { text: "Details"; onClicked: app.navigate("isolation") }
                }
            }

            // ---------------------------------------------------------- effects summary
            Card {
                Layout.fillWidth: true
                Layout.columnSpan: root.cols === 1 ? 1 : 2
                Layout.fillHeight: true
                title: "Effects"
                subtitle: "Processing order, left to right. Click one to adjust it."
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.sp2
                    Repeater {
                        model: app.visibleStages
                        Rectangle {
                            readonly property bool on: modelData.enable === "" || root.p[modelData.enable] > 0.5
                            implicitWidth: chip.implicitWidth + 2 * Theme.sp3
                            implicitHeight: 30
                            radius: 15
                            color: chipMouse.containsMouse ? Theme.cardHi : Theme.card
                            border.color: on ? Theme.tintBorder(modelData.accent) : Theme.border
                            activeFocusOnTab: true
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.title + (on ? "" : ", off")
                            Keys.onSpacePressed: app.openStage(modelData.id)
                            Keys.onReturnPressed: app.openStage(modelData.id)
                            Row {
                                id: chip
                                anchors.centerIn: parent
                                spacing: 6
                                Rectangle { width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: on ? modelData.accent : Theme.track }
                                Text { text: modelData.title; color: on ? Theme.text : Theme.textFaint; font.pixelSize: Theme.fsBody }
                            }
                            MouseArea { id: chipMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: app.openStage(modelData.id) }
                            FocusRing {}
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.sp2
                    Text { text: "Sound profile"; color: Theme.textDim; font.pixelSize: Theme.fsBody }
                    MfComboBox {
                        Layout.preferredWidth: 240
                        model: controller.presetNames
                        displayText: controller.currentPreset === "" ? "Custom (edited)" : controller.currentPreset
                        currentIndex: controller.presetNames.indexOf(controller.currentPreset)
                        onActivated: (i) => controller.loadPreset(controller.presetNames[i])
                        Accessible.name: "Sound profile"
                    }
                    Item { Layout.fillWidth: true }
                    PillButton { text: "Open effects"; onClicked: app.navigate("effects") }
                }
            }
        }
    }
}
