import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl

Rectangle {
    id: ipPage
    color: "#00000000"

    signal back()

    MouseArea {
        anchors.fill: parent
    }

    Item {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        height: 200

        Rectangle {
            id: backRecangle
            color: theme.color2
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 600
        }

        Image {
            source: "qrc:/src/assets/BottomShadow.png"
            anchors.bottom: backRecangle.top
            anchors.left: backRecangle.left
            anchors.right: backRecangle.right
            fillMode: Image.TileHorizontally
        }

        Image {
            source: "qrc:/src/assets/TopShadow.png"
            anchors.top: backRecangle.bottom
            anchors.left: backRecangle.left
            anchors.right: backRecangle.right
            fillMode: Image.TileHorizontally
        }

        Image {
            anchors.top: parent.top
            anchors.left: parent.left
            source: "qrc:/src/assets/PanelGradient.png"
        }

        ColorImage {
            id: backIcon
            sourceSize.width: 40
            sourceSize.height: 40
            source: "qrc:/src/assets/ArrowBack.svg"
            color: theme.color6
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 5
            anchors.leftMargin: 5

            MouseArea {
                anchors.fill: parent
                onClicked: ipPage.back();
            }
        }

        SmoothText {
            id: boxTitle
            anchors.left: backIcon.right
            anchors.top: parent.top
            anchors.leftMargin: 15
            anchors.topMargin: 5
            font.pixelSize: 64
            text: qsTr("Your IP addresses")
        }

        ListView {
            anchors.top: backIcon.bottom
            anchors.left: boxTitle.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.topMargin: 20
            anchors.bottomMargin: 10
            clip: true

            model: ipAddressesData

            Component {
                id: ipDelegate
                SText {
                    text: ip
                    font.pixelSize: 17
                }
            }

            delegate: ipDelegate
        }
    }
}
