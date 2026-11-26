import QtQuick
import QtQuick.Layouts

// Plain surface for grouping related information on a page. Children stack vertically.
Rectangle {
    id: root
    property string title: ""
    property string subtitle: ""
    property color accent: "transparent"     // optional thin accent line on the left
    property int pad: Theme.sp4 + 2
    default property alias content: col.data

    implicitHeight: col.implicitHeight + 2 * pad
    implicitWidth: col.implicitWidth + 2 * pad
    color: Theme.card
    radius: Theme.radius
    border.color: Theme.border

    Rectangle {
        visible: root.accent.a > 0
        x: 0; y: Theme.radius; width: 3; height: parent.height - 2 * Theme.radius
        color: root.accent
    }

    ColumnLayout {
        id: col
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: root.pad }
        spacing: Theme.sp3
        ColumnLayout {
            visible: root.title !== ""
            Layout.fillWidth: true
            spacing: 2
            Text { text: root.title; color: Theme.text; font.pixelSize: Theme.fsTitle; font.weight: Font.Bold }
            Text {
                visible: root.subtitle !== ""
                Layout.fillWidth: true
                text: root.subtitle
                color: Theme.textDim
                font.pixelSize: Theme.fsBody
                wrapMode: Text.WordWrap
            }
        }
    }
}
