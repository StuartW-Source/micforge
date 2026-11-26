import QtQuick
import QtQuick.Controls.Basic as B

B.MenuItem {
    id: root
    implicitHeight: 32
    contentItem: Text {
        leftPadding: root.checkable ? 22 : 4
        text: root.text
        color: Theme.text
        opacity: root.enabled ? 1 : 0.4
        font.pixelSize: Theme.fsBody
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Text {
        visible: root.checkable && root.checked
        x: 10
        anchors.verticalCenter: parent.verticalCenter
        text: "✓"
        color: Theme.focus
        font.pixelSize: Theme.fsBody
    }
    background: Rectangle {
        radius: Theme.radiusSm
        color: root.highlighted ? Theme.cardHi : "transparent"
    }
}
