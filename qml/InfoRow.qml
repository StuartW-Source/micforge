import QtQuick
import QtQuick.Layouts

// "Label   value" line used on the Overview and Devices pages.
RowLayout {
    id: root
    property string label: ""
    property string value: ""
    property color valueColor: Theme.text
    Layout.fillWidth: true
    spacing: Theme.sp3
    property int labelWidth: 120
    Text { text: root.label; color: Theme.textFaint; font.pixelSize: Theme.fsBody; Layout.preferredWidth: root.labelWidth; Layout.alignment: Qt.AlignTop; wrapMode: Text.WordWrap }
    Text {
        Layout.fillWidth: true
        text: root.value
        color: root.valueColor
        font.pixelSize: Theme.fsBody
        font.weight: Font.DemiBold
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        maximumLineCount: 3
        elide: Text.ElideRight
    }
}
