import QtQuick
import QtQuick.Controls.Basic as B

// Themed single-line text input.
B.TextField {
    id: root
    implicitHeight: Theme.controlHeight
    font.pixelSize: Theme.fsBodyLg
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.alpha(Theme.focus, 0.35)
    selectedTextColor: Theme.text
    leftPadding: Theme.sp3
    rightPadding: Theme.sp3
    background: Rectangle {
        radius: Theme.radiusSm + 2
        color: Theme.card
        border.color: root.activeFocus ? Theme.focus : Theme.border
        border.width: root.activeFocus ? 2 : 1
    }
}
