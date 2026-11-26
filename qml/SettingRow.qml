import QtQuick
import QtQuick.Layouts

// Title + explanation + switch.
RowLayout {
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
        Text { text: row.title; color: Theme.text; font.pixelSize: Theme.fsBodyLg + 1; font.bold: true }
        Text { Layout.fillWidth: true; text: row.sub; color: Theme.textDim; font.pixelSize: Theme.fsBody; wrapMode: Text.WordWrap }
    }
    ToggleSwitch {
        enabled: row.enabledRow
        checked: row.checked
        label: row.title
        accent: Theme.gate
        implicitWidth: 46; implicitHeight: 26
        onToggled: (on) => row.toggled(on)
    }
}
