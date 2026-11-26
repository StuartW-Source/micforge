import QtQuick

// One effect in the chain strip: name, live status, activity bar and on/off switch.
Rectangle {
    id: root
    property string title: ""
    property string status: ""
    property real activity: 0          // 0..1, drives the glow bar
    property color accent: Theme.input
    property string enableParam: ""
    property bool selected: false
    property int number: 1
    signal clicked()

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: title + ": " + (on ? status : "off")
    Accessible.onPressAction: clicked()
    Keys.onSpacePressed: clicked()
    Keys.onReturnPressed: clicked()

    readonly property bool on: (enableParam === "" || controller.params[enableParam] > 0.5) && controller.params.bypass < 0.5

    width: 168
    height: 84
    radius: 12
    color: selected ? Theme.cardHi : (mouse.containsMouse ? Theme.cardHi : Theme.card)
    border.color: selected ? accent : Theme.border
    border.width: selected ? 2 : 1

    property real shownActivity: 0
    onActivityChanged: shownActivity = activity > shownActivity ? activity : Math.max(activity, shownActivity - 0.06)

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Column {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12; topMargin: 10 }
        spacing: 3
        Row {
            spacing: 6
            Rectangle {
                width: 18; height: 18; radius: 9
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.alpha(root.accent, root.on ? 0.25 : 0.08)
                Text { anchors.centerIn: parent; text: root.number; color: root.on ? root.accent : Theme.textFaint; font.pixelSize: 10; font.bold: true }
            }
            Text {
                text: root.title
                color: root.on ? Theme.text : Theme.textFaint
                font.pixelSize: 13
                font.weight: Font.Bold
                width: root.width - 24 - 24 - 40
                elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Text {
            text: root.on ? root.status : "off"
            color: root.on ? root.accent : Theme.textFaint
            font.pixelSize: 12
            font.family: Theme.mono
            font.weight: Font.DemiBold
            width: parent.width
            elide: Text.ElideRight
        }
    }

    ToggleSwitch {
        visible: root.enableParam !== ""
        anchors { right: parent.right; top: parent.top; margins: 10 }
        paramKey: root.enableParam
        label: root.title + " on"
        accent: root.accent
        implicitWidth: 30; implicitHeight: 17
    }

    // Activity bar.
    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 12; bottomMargin: 10 }
        height: 5
        radius: 3
        color: Theme.track
        Rectangle {
            height: parent.height
            radius: 3
            width: parent.width * Math.max(0, Math.min(1, root.on ? root.shownActivity : 0))
            color: root.accent
        }
    }
    FocusRing {}
}
