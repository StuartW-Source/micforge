import QtQuick
import QtQuick.Controls

// Small dark tooltip with a delay, matching the app's style.
ToolTip {
    id: tt
    delay: 450
    contentItem: Text { text: tt.text; color: Theme.text; font.pixelSize: 12; wrapMode: Text.WordWrap }
    background: Rectangle { color: Theme.tooltip; border.color: Theme.border; radius: 8 }
}
