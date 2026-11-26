import QtQuick
import QtQuick.Layouts

// Label + value + a thin bar (0..1) that turns amber/red above the warn/bad marks.
ColumnLayout {
    id: root
    property string label: ""
    property string value: ""
    property real fraction: 0
    property real warnAt: 0.5
    property real badAt: 0.8
    spacing: 3
    Layout.fillWidth: true
    RowLayout {
        Layout.fillWidth: true
        Text { text: root.label; color: Theme.textFaint; font.pixelSize: Theme.fsBody; Layout.fillWidth: true }
        Text { text: root.value; color: Theme.text; font.pixelSize: Theme.fsBody; font.family: Theme.mono; font.weight: Font.DemiBold }
    }
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: 5; radius: 3
        color: Theme.track
        Rectangle {
            height: parent.height; radius: 3
            width: parent.width * Math.max(0, Math.min(1, root.fraction))
            color: root.fraction >= root.badAt ? Theme.danger : root.fraction >= root.warnAt ? Theme.warn : Theme.gate
        }
    }
}
