import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// App settings: startup behaviour and about.
Popup {
    id: root
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 560
    padding: 28
    background: Rectangle { color: Theme.panel; radius: 18; border.color: Theme.border }
    Overlay.modal: Rectangle { color: "#b0000000" }

    component SettingRow: RowLayout {
        id: row
        property string title: ""
        property string sub: ""
        property bool checked: false
        property bool enabledRow: true
        signal toggled(bool on)
        Layout.fillWidth: true
        spacing: 14
        opacity: enabledRow ? 1 : 0.45
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Text { text: row.title; color: Theme.text; font.pixelSize: 14; font.bold: true }
            Text { Layout.fillWidth: true; text: row.sub; color: Theme.textDim; font.pixelSize: 12; wrapMode: Text.WordWrap }
        }
        ToggleSwitch {
            enabled: row.enabledRow
            checked: row.checked
            accent: Theme.gate
            implicitWidth: 46; implicitHeight: 26
            onToggled: (on) => row.toggled(on)
        }
    }

    contentItem: ColumnLayout {
        spacing: 18
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Settings"; color: Theme.text; font.pixelSize: 24; font.weight: Font.Bold }
            Item { Layout.fillWidth: true }
            PillButton { text: "Close"; onClicked: root.close() }
        }

        Text { text: "STARTUP"; color: Theme.textFaint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }
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
            color: Theme.textFaint; font.pixelSize: 11
            text: trayAvailable ? "Closing the window keeps MicForge running in the tray. Use the tray icon's menu to quit."
                                : "No system tray was found, so closing the window quits MicForge."
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        Text { text: "TROUBLESHOOTING"; color: Theme.textFaint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textDim; font.pixelSize: 12
            text: "Audio engine: " + (controller.engineOk ? "connected to PipeWire" : controller.engineState + (controller.engineError ? " (" + controller.engineError + ")" : ""))
                  + (controller.safeMode ? "\nRunning in safe mode." : "")
                  + "\nLog file: " + controller.logPath
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            PillButton { text: "Open log folder"; implicitHeight: 28; onClicked: controller.openLogFolder() }
            PillButton { text: "Restart audio engine"; implicitHeight: 28; onClicked: controller.restartEngine() }
            PillButton {
                text: controller.safeMode ? "Restart normally" : "Restart in safe mode"
                implicitHeight: 28
                onClicked: controller.requestRestart(!controller.safeMode)
            }
        }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textFaint; font.pixelSize: 11
            text: "Safe mode starts MicForge without your plugins and with default sound settings, and saves nothing, so a problem in your configuration can't stop MicForge from opening. You can also start it from a terminal with: micforge --safe-mode"
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        Text { text: "ABOUT"; color: Theme.textFaint; font.pixelSize: 11; font.bold: true; font.letterSpacing: 1 }
        RowLayout {
            spacing: 14
            Image { source: "qrc:/icons/micforge.svg"; sourceSize.width: 48; sourceSize.height: 48 }
            ColumnLayout {
                spacing: 2
                Text { text: "MicForge " + controller.version; color: Theme.text; font.pixelSize: 15; font.bold: true }
                Text { text: "Free software under the GNU GPL v3."; color: Theme.textDim; font.pixelSize: 12 }
                Text {
                    text: "<a href=\"https://github.com/stuartw-source/micforge\">github.com/stuartw-source/micforge</a>"
                    textFormat: Text.StyledText
                    color: Theme.textDim; linkColor: Theme.input; font.pixelSize: 12
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                    MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton; cursorShape: Qt.PointingHandCursor }
                }
            }
        }
    }
}
