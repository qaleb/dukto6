import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Controls.impl

Item {
    id: bottomToolBar
    height: 71
    anchors.bottomMargin: -18
    state: "WithoutLabels"

    signal showIpList();
    signal showSettings();

    Behavior on anchors.bottomMargin { NumberAnimation { duration: 200; easing.type: "OutCubic" } }

    Image {
        source: "qrc:/src/assets/BottomShadow.png"
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        fillMode: Image.TileHorizontally
    }

    Rectangle {
        y: 3
        id: mainRect
        anchors {
            left: parent.left
            right: parent.right
        }

        height: parent.height
        color: theme.color2

        ColorImage {
            id: openFolderIcon
            anchors {
                top: parent.top
                topMargin: 5
                right: showIpIcon.left
                rightMargin: 50
            }
            sourceSize.width: 40
            sourceSize.height: 40
            source: "qrc:/src/assets/OpenFolder.svg"
            color: "#fff"
            visible: (Qt.platform.os === "windows" || Qt.platform.os === "linux" || Qt.platform.os === "osx")
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    if (Qt.platform.os === "windows" || Qt.platform.os === "linux" || Qt.platform.os === "osx") {
                        guiBehind.openDestinationFolder();
                    } else {
                        guiBehind.openDestinationFolder();
                        // folderDialog.open();
                    }
                }
            }
        }

        Text {
            anchors {
                top: openFolderIcon.bottom
                topMargin: 3
                horizontalCenter: openFolderIcon.horizontalCenter
            }
            text: qsTr("Received")
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            width: 1
            color: "#fff"
            visible: openFolderIcon.visible
        }

        FolderDialog {
            id: folderDialog
            title: qsTr("Change Folder")
            options: {
                FolderDialog.ShowDirsOnly;
                FolderDialog.ReadOnly;
            }
            currentFolder: guiBehind.currentPath
        }

        ColorImage {
            id: showIpIcon
            anchors.top: parent.top
            anchors.topMargin: 5
            anchors.horizontalCenter: parent.horizontalCenter
            sourceSize.width: 40
            sourceSize.height: 40
            source: "qrc:/src/assets/ShowIpDark.svg"
            color: "#fff"
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    guiBehind.refreshIpList();
                    showIpList();
                }
            }
        }

        Text {
            anchors {
                top: showIpIcon.bottom
                topMargin: 3
                horizontalCenter: showIpIcon.horizontalCenter
            }
            text: qsTr("Addresses")
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            width: 1
            color: "#fff"
        }

        ColorImage {
            id: settingsIcon
            anchors.top: parent.top
            anchors.topMargin: 5
            anchors.left: showIpIcon.right
            anchors.leftMargin: 50
            sourceSize.width: 40
            sourceSize.height: 40
            source: "qrc:/src/assets/SettingsDark.svg"
            color: "#fff"

            MouseArea {
                anchors.fill: parent
                onClicked: showSettings();
            }
        }

        Text {
            anchors {
                top: settingsIcon.bottom
                topMargin: 3
                horizontalCenter: settingsIcon.horizontalCenter
            }
            text: qsTr("Settings")
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            width: 1
            color: "#fff"
        }

        ColorImage {
            id: moreIcon
            anchors.top: parent.top
            anchors.topMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 20
            width: 20; height: 20
            source: "qrc:/src/assets/SwipeUp.png"
            opacity: 1.0
            color: "#fff"
        }

        MouseArea {
            anchors {
                top: parent.top
                bottom: parent.bottom
                left: settingsIcon.right
                leftMargin: 25
                right: parent.right
            }
            onClicked: {
                if (bottomToolBar.state === "WithoutLabels") {
                    bottomToolBar.state = "WithLabels";
                    moreIcon.opacity = 0.0;
                    moreIcon.source = "qrc:/src/assets/SwipeDown.png";
                    moreIcon.opacity = 1.0;
                } else {
                    bottomToolBar.state = "WithoutLabels";
                    moreIcon.opacity = 0.0;
                    moreIcon.source = "qrc:/src/assets/SwipeUp.png";
                    moreIcon.opacity = 1.0;
                }
            }
        }
    }

    states: [
        State {
            name: "WithoutLabels"

            PropertyChanges {
                target: bottomToolBar
                anchors.bottomMargin: -18
            }
        },
        State {
            name: "WithLabels"

            PropertyChanges {
                target: bottomToolBar
                anchors.bottomMargin: 0
            }
        }
    ]
}
