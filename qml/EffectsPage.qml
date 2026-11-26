import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Effects rack: the processing chain (left to right) and the selected effect's controls.
ColumnLayout {
    id: root
    property var app
    spacing: Theme.sp3

    Flickable {
        Layout.fillWidth: true
        Layout.preferredHeight: 92
        contentWidth: strip.width
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalFlick
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

        Row {
            id: strip
            y: 4
            x: 4
            spacing: 0
            Repeater {
                model: app.visibleStages
                Row {
                    spacing: 0
                    ChainTile {
                        number: index + 1
                        title: modelData.title
                        accent: modelData.accent
                        enableParam: modelData.enable
                        selected: app.selected === modelData.id
                        status: app.stageStatus(modelData.id)
                        activity: app.stageActivity(modelData.id)
                        onClicked: app.selected = modelData.id
                    }
                    Item {
                        width: 18; height: 84
                        Text { anchors.centerIn: parent; text: "›"; color: Theme.textFaint; font.pixelSize: 20 }
                    }
                }
            }
            // Add effect: opens the Effect Library.
            Rectangle {
                id: addTile
                width: 120; height: 84; radius: Theme.radius
                color: addMouse.containsMouse ? Theme.cardHi : "transparent"
                border.color: Theme.border
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: "Add effect"
                Keys.onSpacePressed: app.openLibrary()
                Keys.onReturnPressed: app.openLibrary()
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "+"; color: Theme.textDim; font.pixelSize: 24 }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Add effect"; color: Theme.textDim; font.pixelSize: Theme.fsBody }
                }
                MouseArea {
                    id: addMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: app.openLibrary()
                }
                FocusRing {}
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Theme.sp3

        Loader {
            id: detail
            Layout.fillWidth: true
            Layout.fillHeight: true
            source: app.selectedDef ? app.selectedDef.file : ""
        }
        Connections {
            target: detail.item
            ignoreUnknownSignals: true
            function onSetupRequested() { app.openSetup() }
            function onLearnRequested() { app.openLearn() }
        }

        // Always-visible levels, hidden on narrow windows (the Overview has them too).
        Rectangle {
            visible: root.width > 1000
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.card
            border.color: Theme.border

            ColumnLayout {
                anchors { fill: parent; margins: Theme.sp4 }
                spacing: Theme.sp3
                Text { text: "Live"; color: Theme.text; font.pixelSize: Theme.fsTitle; font.weight: Font.Bold }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 6
                    LevelMeter { Layout.fillHeight: true; Layout.fillWidth: true; level: controller.inPeak; label: "MIC IN"; accent: Theme.input }
                    LevelMeter { Layout.fillHeight: true; Layout.fillWidth: true; showScale: false; level: controller.outPeak; label: "TO APPS"; accent: Theme.output }
                }
                GateLight { Layout.fillWidth: true; app: root.app }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 40
                    radius: Theme.radiusSm + 4
                    color: controller.virtualMicInUse ? Theme.alpha(Theme.output, 0.15) : Theme.well
                    border.color: controller.virtualMicInUse ? Theme.output : Theme.border
                    Text {
                        anchors.centerIn: parent
                        text: controller.virtualMicInUse ? "ON AIR" : "No app connected"
                        color: controller.virtualMicInUse ? Theme.output : Theme.textFaint
                        font.pixelSize: Theme.fsBody; font.weight: Font.Bold
                    }
                }
            }
        }
    }
}
