import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Every effect, grouped by purpose. Add anything to the chain, or remove anything already in it.
Popup {
    id: root
    property var stages: []
    signal picked(string id)

    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(1100, parent ? parent.width - 60 : 1100)
    height: Math.min(820, parent ? parent.height - 60 : 820)
    padding: 28
    background: Rectangle { color: Theme.panel; radius: 18; border.color: Theme.border }
    Overlay.modal: Rectangle { color: Theme.scrim }

    readonly property var groups: [
        { name: "Clean up", sub: "Remove what you don't want people to hear." },
        { name: "Shape",    sub: "Make your voice sound its best." },
        { name: "Space",    sub: "Add depth, width and atmosphere." },
        { name: "Fun",      sub: "Silly voices for games, streams and messing about." },
        { name: "Your plugins", sub: "Third-party VST3 and LV2 effects installed on your computer." }
    ]
    readonly property var p: controller.params

    contentItem: ColumnLayout {
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                spacing: 2
                Text { text: "Effect library"; color: Theme.text; font.pixelSize: 24; font.weight: Font.Bold }
                Text { text: "Add any effect to your chain, or remove one you don't need. Effects always run in the best order automatically."; color: Theme.textDim; font.pixelSize: 13 }
            }
            Item { Layout.fillWidth: true }
            PillButton { text: "Close"; onClicked: root.close() }
        }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: col.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            ColumnLayout {
                id: col
                width: flick.width - 12
                spacing: 22
                Repeater {
                    model: root.groups
                    ColumnLayout {
                        id: grp
                        Layout.fillWidth: true
                        spacing: 10
                        readonly property string groupName: modelData.name
                        RowLayout {
                            spacing: 10
                            Text { text: modelData.name; color: Theme.text; font.pixelSize: 16; font.bold: true }
                            Text { text: modelData.sub; color: Theme.textFaint; font.pixelSize: 12 }
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: Math.max(1, Math.floor(col.width / 330))
                            columnSpacing: 12
                            rowSpacing: 12
                            Repeater {
                                model: root.stages.filter(s => s.group === grp.groupName)
                                Rectangle {
                                    id: card
                                    readonly property bool added: root.p[modelData.optional + "Added"] > 0.5
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 1
                                    implicitHeight: 92
                                    radius: 12
                                    color: m.containsMouse ? Theme.cardHi : Theme.card
                                    border.color: added ? Theme.alpha(modelData.accent, 0.6) : Theme.border
                                    RowLayout {
                                        anchors { fill: parent; margins: 12 }
                                        spacing: 12
                                        Rectangle {
                                            Layout.alignment: Qt.AlignTop
                                            width: 38; height: 38; radius: 10
                                            color: Theme.alpha(modelData.accent, 0.18)
                                            Text { anchors.centerIn: parent; text: modelData.glyph; color: modelData.accent; font.pixelSize: 20 }
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Text { text: modelData.title; color: Theme.text; font.pixelSize: 14; font.bold: true }
                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.blurb
                                                color: Theme.textDim; font.pixelSize: 12
                                                wrapMode: Text.WordWrap
                                                maximumLineCount: 2
                                                elide: Text.ElideRight
                                            }
                                        }
                                        ColumnLayout {
                                            Layout.alignment: Qt.AlignVCenter
                                            spacing: 4
                                            PillButton {
                                                text: card.added ? "Open" : "Add"
                                                implicitHeight: 28
                                                primary: !card.added
                                                accent: modelData.accent
                                                onClicked: {
                                                    if (!card.added) controller.addEffect(modelData.optional)
                                                    root.picked(modelData.id)
                                                    root.close()
                                                }
                                            }
                                            Text {
                                                visible: card.added
                                                Layout.alignment: Qt.AlignHCenter
                                                text: "Remove"
                                                color: Theme.textFaint; font.pixelSize: 11; font.underline: rm.containsMouse
                                                MouseArea { id: rm; anchors.fill: parent; anchors.margins: -4; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: controller.removeEffect(modelData.optional) }
                                            }
                                        }
                                    }
                                    MouseArea { id: m; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
