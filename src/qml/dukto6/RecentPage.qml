import QtQuick

Item {
    id: recentPage
    clip: true

    signal showText();

    Rectangle {
        anchors.fill: parent
        color: theme.color6
    }

    SmoothText {
        id: badSmile
        anchors.left: parent.left
        anchors.leftMargin: 40
        anchors.top: parent.top
        font.pixelSize: 160
        text: ":-("
        color: theme.color4
        visible: (recentList.count === 0)
    }

    SText  {
        y: badSmile.y + badSmile.height / 2 + 20
        // anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 40
        font.pixelSize: 17
        color: theme.color4
        text: qsTr("Sorry, no data has been\nreceived yet...")
        visible: (recentList.count === 0)
    }

    ListView {
        id: recentList
        anchors.fill: parent
        spacing: 10
        anchors.leftMargin: 25
        model: recentListData // EsempioRecent {}

        Component {
            id: recentDelegate
            Item {
                anchors.left: parent.left
                anchors.right: parent.right
                width: parent.width
                height: 36

                MouseArea {
                    id: recentMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        if (type == "text")
                            guiBehind.showTextSnippet(value, sender);
                        else if (type == "file")
                            guiBehind.openFile(value);
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    color: "#00000000"
                    visible: recentMouseArea.containsMouse;
                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 5
                        color: theme.color3
                    }
                }

                Rectangle {
                    id: fileImage
                    anchors.verticalCenter: parent.verticalCenter
                    width: 32
                    height: 32
                    color: theme.color2
                    Image {
                        source: typeIcon
                        width: 32
                        height: 32
                    }
                }
                SText {
                    id: recentNameText
                    anchors.top: fileImage.top
                    anchors.topMargin: -2
                    anchors.left: fileImage.right
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    font.pixelSize: 14
                    color: theme.color4
                    elide: "ElideRight"
                    text: name
                }
                SText {
                    anchors.left: recentNameText.left
                    anchors.leftMargin: 0
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.bottom: fileImage.bottom
                    anchors.bottomMargin: -1
                    font.pixelSize: 12
                    elide: "ElideRight"
                    color: theme.color5
                    text: qsTr("from ") + sender + ", " + dateTime
                }
            }
        }

        delegate: recentDelegate
    }
}
