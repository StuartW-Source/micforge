import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// App settings: appearance, startup, help, troubleshooting and about.
Flickable {
    id: root
    property var app
    contentHeight: body.implicitHeight + Theme.sp4
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    ColumnLayout {
        id: body
        width: Math.min(root.width, 760)
        spacing: Theme.sp4

        Card {
            Layout.fillWidth: true
            title: "Appearance"
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.sp4
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text { text: "Theme"; color: Theme.text; font.pixelSize: Theme.fsBodyLg + 1; font.bold: true }
                    Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "“System” follows your desktop's light or dark setting."; color: Theme.textDim; font.pixelSize: Theme.fsBody }
                }
                Segmented {
                    Layout.preferredWidth: 260
                    options: ["System", "Dark", "Light"]
                    selectedIndex: ["system", "dark", "light"].indexOf(controller.themeMode)
                    accent: Theme.focus
                    onChosen: (i) => controller.themeMode = ["system", "dark", "light"][i]
                    Accessible.name: "Theme"
                }
            }
            SettingRow {
                title: "Show getting-started tips"
                sub: "The three-step guide at the top of the Overview."
                checked: controller.showWelcome
                onToggled: (on) => controller.showWelcome = on
            }
        }

        Card {
            Layout.fillWidth: true
            title: "Keyboard shortcuts in any app"
            subtitle: "Mute, push-to-talk or switch voices while you're in a game or another window. Your desktop asks you to confirm the keys the first time."
            SettingRow {
                title: "Use system-wide shortcuts"
                sub: controller.shortcutsAvailable ? "Status: " + controller.shortcutsStatus : controller.shortcutsStatus
                checked: controller.shortcutsEnabled
                enabledRow: controller.shortcutsAvailable && !controller.safeMode
                onToggled: (on) => controller.shortcutsEnabled = on
            }
            InfoRow {
                visible: controller.shortcutsEnabled
                label: "Mute / unmute"
                value: controller.muteKey !== "" ? controller.muteKey : "not assigned"
            }
            InfoRow {
                visible: controller.shortcutsEnabled
                label: "Push to talk"
                value: controller.pushToTalkKey !== "" ? controller.pushToTalkKey + " (hold)" : "not assigned"
            }
            SettingRow {
                visible: controller.shortcutsEnabled
                title: "Push to talk"
                sub: "Your mic stays muted except while you hold the push-to-talk key. MicForge starts muted while this is on."
                checked: controller.pushToTalk
                onToggled: (on) => controller.pushToTalk = on
            }
            SettingRow {
                visible: controller.shortcutsEnabled
                title: "Shortcuts for voices"
                sub: "A key for each Voice Changer voice (press it again for your own voice). Your desktop lists them so you can choose keys for the ones you want."
                checked: controller.voiceShortcutsEnabled
                onToggled: (on) => controller.voiceShortcutsEnabled = on
            }
            PillButton {
                visible: controller.shortcutsEnabled
                text: "Change keys…"
                implicitHeight: Theme.controlHeightSm
                onClicked: controller.configureShortcuts()
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textFaint; font.pixelSize: Theme.fsSmall
                text: "Inside MicForge: Ctrl+M mutes, Ctrl+Z undoes, Ctrl+1 to Ctrl+8 switch pages."
            }
        }

        Card {
            Layout.fillWidth: true
            title: "Startup"
            SettingRow {
                title: "Start MicForge when I log in"
                sub: "Your processed mic is ready as soon as you log in, so Discord and other apps always find it."
                checked: controller.autostart
                onToggled: (on) => controller.autostart = on
            }
            SettingRow {
                title: "Start hidden in the system tray"
                sub: "When it starts at login, keep the window closed. Open it any time from the tray icon."
                checked: controller.autostartHidden
                enabledRow: controller.autostart
                onToggled: (on) => controller.autostartHidden = on
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textFaint; font.pixelSize: Theme.fsSmall
                text: trayAvailable ? "Closing the window keeps MicForge running in the tray. Use the tray icon's menu to quit."
                                    : "No system tray was found, so closing the window quits MicForge."
            }
        }

        Card {
            Layout.fillWidth: true
            title: "Troubleshooting"
            InfoRow {
                label: "Audio engine"
                value: controller.engineOk ? "Connected to " + controller.backendName
                                           : controller.engineState + (controller.engineError ? " (" + controller.engineError + ")" : "")
            }
            InfoRow { label: "Mode"; value: controller.safeMode ? "Safe mode" : "Normal" }
            InfoRow { label: "Log file"; value: controller.logPath }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.sp2
                PillButton { text: "Open Diagnostics"; primary: true; implicitHeight: Theme.controlHeightSm; onClicked: app.navigate("diagnostics") }
                PillButton { text: "Open log folder"; implicitHeight: Theme.controlHeightSm; onClicked: controller.openLogFolder() }
                PillButton { text: "Restart audio engine"; implicitHeight: Theme.controlHeightSm; onClicked: controller.restartEngine() }
                PillButton {
                    text: controller.safeMode ? "Restart normally" : "Restart in safe mode"
                    implicitHeight: Theme.controlHeightSm
                    onClicked: controller.requestRestart(!controller.safeMode)
                }
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textFaint; font.pixelSize: Theme.fsSmall
                text: "Safe mode starts MicForge without your plugins and with default sound settings, and saves nothing, so a problem in your configuration can't stop MicForge from opening. You can also start it from a terminal with: micforge --safe-mode"
            }
        }

        Card {
            Layout.fillWidth: true
            title: "About"
            RowLayout {
                spacing: 14
                Image { source: "qrc:/icons/micforge.svg"; sourceSize.width: 48; sourceSize.height: 48 }
                ColumnLayout {
                    spacing: 2
                    Text { text: "MicForge " + controller.version; color: Theme.text; font.pixelSize: Theme.fsTitle; font.bold: true }
                    Text { text: "Free software under the GNU GPL v3. Built with Claude (Anthropic)."; color: Theme.textDim; font.pixelSize: Theme.fsBody }
                    Text {
                        text: "<a href=\"https://github.com/stuartw-source/micforge\">github.com/stuartw-source/micforge</a>"
                        textFormat: Text.StyledText
                        color: Theme.textDim; linkColor: Theme.input; font.pixelSize: Theme.fsBody
                        onLinkActivated: (link) => Qt.openUrlExternally(link)
                        MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton; cursorShape: Qt.PointingHandCursor }
                    }
                }
            }
        }
    }
}
