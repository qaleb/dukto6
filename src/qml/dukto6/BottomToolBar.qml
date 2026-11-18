import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Controls.impl

Rectangle {
    id: bottomToolBar
    readonly property bool isAndroid: Qt.platform.os === "android"
    height: 64
    color: theme.color2
    signal showIpList()
    signal showSettings()

    Row {
        anchors.centerIn: parent
        spacing: 16
        component NavigationButton: C.ToolButton {
            id: control
            property url imageSource
            width: 88; height: 60
            padding: 0
            contentItem: Column {
                spacing: 3
                ColorImage {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 30; height: 30; source: control.imageSource; color: theme.accentText
                    sourceSize: Qt.size(width, height)
                }
                Text {
                    width: parent.width; horizontalAlignment: Text.AlignHCenter
                    text: control.text; color: theme.accentText; font.pixelSize: 12
                }
            }
            background: Rectangle {
                radius: 6
                color: control.down ? theme.color1 : "transparent"
                border.width: control.visualFocus ? 2 : 0; border.color: theme.accentText
            }
        }
        NavigationButton {
            visible: !bottomToolBar.isAndroid
            text: qsTr("Received")
            imageSource: "qrc:/src/assets/OpenFolder.svg"
            onClicked: guiBehind.openDestinationFolder()
        }
        NavigationButton {
            objectName: "addressesNavigation"
            text: qsTr("Addresses")
            imageSource: "qrc:/src/assets/ShowIpDark.svg"
            onClicked: { guiBehind.refreshIpList(); bottomToolBar.showIpList(); }
        }
        NavigationButton {
            objectName: "settingsNavigation"
            text: qsTr("Settings")
            imageSource: "qrc:/src/assets/SettingsDark.svg"
            onClicked: bottomToolBar.showSettings()
        }
    }
}
