import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Detail view for one effect: header, plain-English summary, a visual on the left,
// controls on the right, and an "Advanced" section that stays hidden until asked for.
Rectangle {
    id: root
    property string title: ""
    property string summary: ""
    property string help: ""
    property color accent: Theme.input
    property string enableParam: ""
    property string effectName: ""            // set for optional effects (shows "Remove")
    property real visualWidth: 260
    property bool advancedOpen: false
    property bool hasAdvanced: advGrid.children.length > 0
    default property alias controls: ctrlGrid.data
    property alias visual: visualBox.data
    property alias advanced: advGrid.data

    readonly property bool active: (enableParam === "" || controller.params[enableParam] > 0.5)
                                   && controller.params["bypass"] < 0.5
    readonly property int columns: ctrlArea.width > 640 ? 2 : 1

    color: Theme.card
    radius: Theme.radius
    border.color: Theme.border

    Rectangle {
        anchors { top: parent.top; left: parent.left; right: parent.right; margins: 1 }
        height: 3; radius: 2
        color: root.accent
        opacity: root.active ? 1 : 0.25
    }

    Flickable {
        id: flick
        anchors { fill: parent; margins: 22; topMargin: 20 }
        contentHeight: body.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: body
            width: flick.width
            spacing: 16

            // Header.
            RowLayout {
                id: headerRow
                Layout.fillWidth: true
                spacing: 12
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text { text: root.title; color: Theme.text; font.pixelSize: 22; font.weight: Font.Bold }
                    Text {
                        text: root.summary
                        color: Theme.textDim
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        Layout.maximumWidth: 820
                        lineHeight: 1.2
                    }
                }
                Item { Layout.fillWidth: true }
                HelpButton { visible: root.help !== ""; text: root.help; Layout.alignment: Qt.AlignTop }
                PillButton {
                    visible: root.effectName !== ""
                    Layout.alignment: Qt.AlignTop
                    text: "Remove"
                    implicitHeight: 28
                    onClicked: controller.removeEffect(root.effectName)
                }
                ToggleSwitch {
                    visible: root.enableParam !== ""
                    Layout.alignment: Qt.AlignTop
                    paramKey: root.enableParam
                    accent: root.accent
                    implicitWidth: 48; implicitHeight: 26
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: flick.width < 760 ? Theme.sp4 : 28

                Item {
                    id: visualBox
                    visible: children.length > 0
                    // Narrower on small windows so the controls keep their room.
                    Layout.preferredWidth: Math.min(root.visualWidth, Math.max(90, flick.width * 0.22))
                    Layout.preferredHeight: Math.max(300, Math.min(560, flick.height - headerRow.height - 40))
                    Layout.alignment: Qt.AlignTop
                    opacity: root.active ? 1 : 0.5
                }

                ColumnLayout {
                    id: ctrlArea
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: 1000
                    Layout.alignment: Qt.AlignTop
                    spacing: 14
                    opacity: root.active ? 1 : 0.5
                    Behavior on opacity { NumberAnimation { duration: 150 } }

                    GridLayout {
                        id: ctrlGrid
                        Layout.fillWidth: true
                        columns: root.columns
                        columnSpacing: 32
                        rowSpacing: 16
                    }

                    // Advanced toggle.
                    Item {
                        visible: root.hasAdvanced
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        implicitHeight: 26
                        Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 1; color: Theme.border }
                        Rectangle {
                            anchors.centerIn: parent
                            width: advText.implicitWidth + 22; height: 26; radius: 13
                            color: advMouse.containsMouse ? Theme.cardHi : Theme.card
                            border.color: Theme.border
                            Text {
                                id: advText
                                anchors.centerIn: parent
                                text: root.advancedOpen ? "Hide advanced ▴" : "Advanced settings ▾"
                                color: Theme.textDim
                                font.pixelSize: 11
                            }
                            MouseArea {
                                id: advMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.advancedOpen = !root.advancedOpen
                            }
                        }
                    }

                    GridLayout {
                        id: advGrid
                        visible: root.advancedOpen
                        Layout.fillWidth: true
                        columns: root.columns
                        columnSpacing: 32
                        rowSpacing: 16
                    }
                }
            }
        }
    }
}
