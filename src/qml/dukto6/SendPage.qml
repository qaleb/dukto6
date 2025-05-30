import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Dialogs

Rectangle {
    id: sendPage
    color: theme.color6

    signal back()
    signal showTextPage()

    MouseArea {
        anchors.fill: parent
        onClicked: forceActiveFocus() // To remove focus from textInputs when user clicks outside
    }

    ColorImage {
        id: backIcon
        sourceSize.width: 40
        sourceSize.height: 40
        source: "qrc:/src/assets/ArrowBack.svg"
        color: theme.color5
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 5
        anchors.leftMargin: 5

        MouseArea {
            anchors.fill: parent
            onClicked: sendPage.back();
        }
    }

    DropArea {
        id: dropTarget
        anchors.fill: parent

        /*onEntered: {
           // Handle when an item is dragged over this area
           console.log("A file is on top of me.");
       }*/

        onDropped: function(drop) {
            if (drop.hasUrls) {
                var filesList = drop.urls.map(function(url) { return url.toString(); });
                // Pass the list of file URLs to the sendDroppedFiles function
                guiBehind.sendBuddyDroppedFiles(filesList);
            }
        }

        Rectangle {
            anchors.fill: parent
            color: "#00000000"
            //           visible: buddyMouseArea.containsMouse

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                height: 64
                width: 5
                color: theme.color3
            }
        }
    }

    SmoothText {
        id: boxTitle
        anchors {
            left: backIcon.right
            top: parent.top
            leftMargin: 15
            topMargin: 5
        }

        font.pixelSize: 64
        text: qsTr("Send data to")
        color: theme.color3
    }

    BuddyListComponent {
        id: localBuddy
        visible: destinationBuddy.ip !== "IP"
        anchors {
            top: backIcon.bottom
            topMargin: 25
            left: parent.left
            leftMargin: 30
            right: parent.right
        }
        buddyGeneric: destinationBuddy.genericAvatar
        buddyAvatar: destinationBuddy.avatar
        buddyOsLogo:destinationBuddy.osLogo
        buddyUsername: destinationBuddy.username
        buddySystem: destinationBuddy.system
        buddyIpAddress: destinationBuddy.ip.substring(7)
        buddyIp: "-"
    }

    BuddyListComponent {
        id: remoteBuddy
        visible: destinationBuddy.ip === "IP"
        anchors {
            top: backIcon.bottom
            topMargin: 25
            left: parent.left
            leftMargin: 30
            right: parent.right
        }
        buddyGeneric: "qrc:/src/assets/UnknownLogo.png"
        buddyAvatar: ""
        buddyOsLogo: ""
        buddyUsername: qsTr("Destination:")
        buddySystem: ""
        buddyIp: "-"
    }

    Rectangle {
        id: destRect
        visible: destinationBuddy.ip === "IP"
        anchors {
            right: localBuddy.right
            bottom: localBuddy.bottom
            bottomMargin: 5
            rightMargin: 20
        }
        border.color: theme.color5
        border.width: 2
        width: 225
        height: 25

        TextInput {
            id: destinationText
            anchors.fill: parent
            anchors.margins: 4
            readOnly: false
            smooth: true
            font.pixelSize: 14
            color: theme.color5
            selectByMouse: true
            focus: destRect.visible
        }

        Binding {
            target: guiBehind
            property: "remoteDestinationAddress"
            value: destinationText.text
        }
    }

    SText {
        id: labelAction
        anchors {
            left: localBuddy.left
            top: localBuddy.bottom
            topMargin: 35
        }
        font.pixelSize: 17
        color: theme.color4
        text: qsTr("What do you want to do?")
    }

    ButtonDark {
        id: buttonSendText
        anchors {
            top: labelAction.bottom
            topMargin: 15
            left: localBuddy.left
        }
        width: 300
        buttonEnabled: guiBehind.currentTransferBuddy !== ""
        label: qsTr("Send some text")
        onClicked: sendPage.showTextPage();
    }

    ButtonDark {
        id: buttonSendClipboardText
        anchors {
            top: buttonSendText.bottom
            topMargin: 15
            left: localBuddy.left
        }
        width: 300
        label: qsTr("Send text from clipboard")
        buttonEnabled: guiBehind.clipboardTextAvailable && (guiBehind.currentTransferBuddy !== "")
        onClicked: guiBehind.sendClipboardText()
    }

    ButtonDark {
        id: buttonSendFiles
        anchors {
            top: buttonSendClipboardText.bottom
            topMargin: 15
            left: localBuddy.left
        }
        width: 300
        buttonEnabled: guiBehind.currentTransferBuddy !== ""
        label: qsTr("Send some files")
        onClicked: fileDialog.open()
    }

    ButtonDark {
        id: buttonSendFolder
        anchors {
            top: buttonSendFiles.bottom
            topMargin: 15
            left: localBuddy.left
        }
        width: 300
        buttonEnabled: guiBehind.currentTransferBuddy !== ""
        label: qsTr("Send a folder")
        onClicked: folderDialog.open()
    }

    ButtonDark {
        id: buttonSendScreen
        anchors {
            top: buttonSendFolder.bottom
            topMargin: 15
            left: localBuddy.left
        }
        width: 300
        buttonEnabled: guiBehind.currentTransferBuddy !== ""
        label: "Send a screenshot"
        onClicked: guiBehind.sendScreenStage2()
        visible: (Qt.platform.os === "windows" || Qt.platform.os === "linux" || Qt.platform.os === "osx")
    }

    FileDialog {
        id: fileDialog
        title: "Select Files to Send"
        acceptLabel: "Select"
        fileMode: FileDialog.OpenFiles // Enable multiple file selection
        onAccepted: guiBehind.sendSomeFiles(fileDialog.selectedFiles);
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Select a Folder to Send")
        acceptLabel: "Select"
        options: FolderDialog.ShowDirsOnly
        currentFolder: guiBehind.currentPath
        onAccepted: {
            var folderList = [];
            folderList.push(folderDialog.selectedFolder); // Add selected folder path to list
            guiBehind.sendAllFiles(folderList);
        }
    }

    SText {
        id: labelDrop
        anchors {
            left: localBuddy.left
            bottom: parent.bottom
            bottomMargin: 20
            right: parent.right
            rightMargin: 30
        }
        font.pixelSize: 14
        color: theme.color5
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: {
            if (Qt.platform.os === "windows" || Qt.platform.os === "linux" || Qt.platform.os === "osx") {
                qsTr("Or simply drag & drop some files and folders\nover this window to send them to your buddy.")
            } else {
                qsTr("Simple and effortless file sharing, made simple with Dukto.\n— Swift and Seamless.")
            }
        }
    }
}
