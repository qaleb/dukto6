import QtQuick

Flipable {
    id: contactDelegateItem
    width: rootWindow.width
    height: buddyIp == "" ? 94 : 64

    property string buddyIp
    property alias buddyGeneric: buddyGenericImage.source
    property alias buddyAvatar: buddyAvatarImage.source
    property alias buddyOsLogo: buddyOsLogoImage.source
    property alias buddyUsername: buddyUsernameText.text
    property alias buddySystem: buddySystemText.text
    property alias buddyIpAddress: buddyIpAddress.text
    property bool buddyShowBack: false

    MouseArea {
        id: buddyMouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: guiBehind.showSendPage(buddyIp)
    }

    Rectangle {
        anchors.fill: parent
        color: "#00000000"
        visible: buddyMouseArea.containsMouse;
        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            height: 64
            width: 5
            color: theme.color2
        }
    }

    front: Item {
        id: temp
        width: contactDelegateItem.width

        Flipable {
            id: flipableAvatar
            width: 64
            height: 64

            front: Rectangle {
                anchors.fill: parent
                color: theme.color2
                Image {
                    anchors.fill: parent
                    source: "qrc:/src/assets/TileGradient.png"
                }
                Image {
                    id: buddyGenericImage
                    anchors.fill: parent
                    sourceSize.width: parent.width
                    fillMode: Image.PreserveAspectFit
                    anchors.margins: 5
                }
                Image {
                    id: buddyAvatarImage
                    anchors.fill: parent
                    sourceSize.width: 64
                    fillMode: Image.PreserveAspectFit
                    anchors.margins: 5
                    smooth: true
                }
            }

            back: Rectangle {
                anchors.fill: parent
                color: theme.color2
                Image {
                    anchors.fill: parent
                    source: "qrc:/src/assets/TileGradient.png"
                }
                Image {
                    id: buddyOsLogoImage
                    anchors.fill: parent
                }
            }

            transform: Rotation {
                id: innerRotation
                origin.x: 32
                origin.y: 32
                axis.x: 1; axis.y: 0; axis.z: 0
                angle: 0
            }

            states: [
                State {
                    name: "OsState"
                    when: buddyShowBack
                }]

            transitions: [
                Transition {
                    from: ""
                    to: "OsState"
                    SequentialAnimation {
                        NumberAnimation { target: innerRotation; property: "angle"; from: 0; to: 180; duration: 400; easing.type: Easing.OutQuad }
                        PauseAnimation { duration: 3000 }
                        NumberAnimation { target: innerRotation; property: "angle"; from: 180; to: 0; duration: 400; easing.type: Easing.OutQuad }
                    }
                }
            ]
        }

        SText {
            id: buddyUsernameText
            anchors.top:  flipableAvatar.top
            anchors.topMargin: 5
            anchors.left: flipableAvatar.right
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 20
            font.pixelSize: 16
            elide: "ElideRight"
            color: theme.color4
        }
        SText {
            id: buddySystemText
            anchors.left: flipableAvatar.right
            anchors.leftMargin: 10
            anchors.top: buddyUsernameText.bottom
            anchors.topMargin: 5
            anchors.right: parent.right
            anchors.rightMargin: 20
            font.pixelSize: 14
            elide: "ElideRight"
            color: theme.color4
        }
        SText {
            id: buddyIpAddress
            anchors.left: flipableAvatar.right
            anchors.leftMargin: 10
            anchors.top: buddySystemText.bottom
            anchors.topMargin: 5
            anchors.right: parent.right
            anchors.rightMargin: 20
            font.pixelSize: 14
            elide: "ElideRight"
            color: theme.color4
        }
    }

    transform: Rotation {
        id: rotation
        origin.x: 32
        origin.y: 32
        axis.x: 1; axis.y: 0; axis.z: 0     // set axis.y to 1 to rotate around y-axis
        angle: 0    // the default angle
    }

    // Create animations separately and give them IDs
    NumberAnimation {
        id: addAnimation
        target: rotation
        property: "angle"
        from: -90
        to: 0
        duration: 300
        easing.type: Easing.OutCubic
    }

    SequentialAnimation {
        id: removeAnimation
        PropertyAction { target: contactDelegateItem; property: "ListView.delayRemove"; value: true }
        NumberAnimation { target: rotation; property: "angle"; from: 0; to: -90; duration: 300; easing.type: Easing.InCubic }
        PropertyAction { target: contactDelegateItem; property: "ListView.delayRemove"; value: false }
    }

    // Call the animations in the signal handlers
    ListView.onAdd: addAnimation.start()
    ListView.onRemove: removeAnimation.start()

    Rectangle {
        color: theme.color2
        x: 40
        width: parent.width - 80
        height: 1
        y: parent.height - 10
        visible: buddyIp == ""
    }
}
