import QtQuick
import QtQuick.Controls.Basic as B

// Themed pop-up menu. Use with MfMenuItem / MfMenuSeparator.
B.Menu {
    id: root
    padding: 4
    implicitWidth: 260
    background: Rectangle {
        implicitWidth: 260
        color: Theme.panel
        radius: Theme.radiusSm + 2
        border.color: Theme.border
    }
}
