import QtQuick

Rectangle {
    id: button
    width: 150
    height: 30
    border.color: buttonEnabled ? theme.accentText : theme.color7
    border.width: 2
    color: buttonArea.containsMouse ? (buttonArea.pressed ? theme.color7 : theme.color8) : "#00000000"

    property bool buttonEnabled: true
    property alias label: textLabel.text
    signal clicked()

    SText {
        id: textLabel
        anchors.fill: parent
        color: buttonEnabled ? (buttonArea.containsMouse ? theme.accentForeground : theme.accentText) : theme.color7
        horizontalAlignment: "AlignHCenter"
        verticalAlignment: "AlignVCenter"
        font.pixelSize: 12
    }

    MouseArea {
        id: buttonArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: button.buttonEnabled
        onClicked: button.clicked();
    }
}
