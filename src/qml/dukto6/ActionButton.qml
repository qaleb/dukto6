import QtQuick
import QtQuick.Controls.Basic as C

C.Button {
    id: control
    property bool primary: false
    implicitHeight: 44
    implicitWidth: 110
    padding: 10
    font.pixelSize: 14
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? theme.color5 : (control.primary ? theme.accentText : theme.color4)
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 6
        color: control.primary && control.enabled ? theme.color2 : (control.down ? theme.color7 : theme.color8)
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? theme.accentForeground : theme.color5
        opacity: control.enabled ? 1 : 0.6
    }
}
