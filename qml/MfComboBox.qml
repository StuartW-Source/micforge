import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as B

// Themed drop-down list (replaces the Fusion ComboBox so it follows light/dark and the design tokens).
B.ComboBox {
    id: root
    property color accent: Theme.input
    implicitHeight: Theme.controlHeight
    implicitWidth: 200
    font.pixelSize: Theme.fsBody
    leftPadding: Theme.sp3
    rightPadding: Theme.sp3 + 18

    background: Rectangle {
        radius: Theme.radiusSm + 2
        color: root.hovered || root.popup.visible ? Theme.cardHi : Theme.card
        border.color: root.popup.visible ? root.accent : Theme.border
        opacity: root.enabled ? 1 : 0.45
        FocusRing { target: root; radius: parent.radius + gap; visible: root.visualFocus }
    }
    contentItem: Text {
        text: root.displayText
        color: Theme.text
        font: root.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Text {
        x: root.width - width - Theme.sp3
        anchors.verticalCenter: parent.verticalCenter
        text: "▾"
        color: Theme.textDim
        font.pixelSize: Theme.fsBody
    }
    delegate: B.ItemDelegate {
        id: d
        required property var modelData
        required property int index
        width: ListView.view ? ListView.view.width : root.width
        implicitHeight: 32
        highlighted: root.highlightedIndex === index
        contentItem: Text {
            text: root.textRole && d.modelData && d.modelData[root.textRole] !== undefined ? d.modelData[root.textRole] : d.modelData
            color: root.currentIndex === d.index ? root.accent : Theme.text
            font.pixelSize: Theme.fsBody
            font.weight: root.currentIndex === d.index ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Theme.radiusSm
            color: d.highlighted ? Theme.cardHi : "transparent"
        }
    }
    popup: B.Popup {
        y: root.height + 4
        width: Math.max(root.width, 220)
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 360)
        padding: 4
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            color: Theme.panel
            radius: Theme.radiusSm + 2
            border.color: Theme.border
        }
    }
}
