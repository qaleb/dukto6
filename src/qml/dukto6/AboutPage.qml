import QtQuick

Item {
    clip: true

    function handleLinkActivated(link) {
        Qt.openUrlExternally(link);
    }

    Rectangle {
        anchors.fill: parent
        color: "white"

        Rectangle {
            x: 25
            y: 15
            width: 64
            height: 64
            color: theme.color2
            Image {
                source: "qrc:/src/assets/DuktoMetroIcon.svg"
                anchors.fill: parent
                sourceSize.width: parent.width
                fillMode: Image.PreserveAspectFit
                anchors.margins: 5
            }
        }

        SmoothText {
            x: 25
            y: 81
            font.pixelSize: 100
            text: "Dukto"
            color: "#000"
        }
        SmoothText {
            x: 25
            y: 137
            text: qsTr("Version: " + guiBehind.appVersion)
            font.pixelSize: 28
            color: "#000"
        }
        SmoothText {
            x: 25
            y: 70 + 100
            font.pixelSize: 38
            text: "Created by Emanuele Colombo"
            color: "#000"
        }
        SmoothText {
            x: 25
            y: 100 + 100
            font.pixelSize: 32
            text: qsTr("Website") + ": <a href=\"http://www.msec.it/\">http://www.msec.it/</a>"
            onLinkActivated: function(link) {
                handleLinkActivated(link)
            }
            HoverHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                cursorShape: Qt.PointingHandCursor
            }
            color: "#000"
        }
        SmoothText {
            x: 25
            y: 120 + 100
            font.pixelSize: 32
            text: "QT5: <a href=\"https://github.com/coolshou/dukto\">https://github.com/coolshou/dukto</a>"
            onLinkActivated: handleLinkActivated('https://github.com/coolshou/dukto')
            HoverHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                cursorShape: Qt.PointingHandCursor
            }
            color: "#000"
        }
        SmoothText {
            x: 25
            y: 120 + 140
            font.pixelSize: 38
            text: "Updated by Caleb Maangi"
            color: "#000"
        }
        SmoothText {
            x: 25
            y: 120 + 165
            font.pixelSize: 32
            text: "QT6: <a href=\"https://github.com/qaleb/dukto6\">https://github.com/qaleb/dukto6</a>"
            onLinkActivated: handleLinkActivated('https://github.com/qaleb/dukto6')
            HoverHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                cursorShape: Qt.PointingHandCursor
            }
            color: "#000"
        }

        // Horizontal rule separator
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 25
            anchors.rightMargin: 41
            y: 320
            height: 1
            color: theme.color2
            radius: 1
        }
        
        // About dukto application
        SText {
            anchors.right: parent.right
            anchors.rightMargin: 41
            anchors.left: parent.left
            anchors.leftMargin: 25
            y: 335
            font.pixelSize: 12
            color: "#000"
            wrapMode: "WordWrap"
            text: qsTr("Dukto is a simple file transfer app for LAN that lets you transfer all kinds of files and documents between devices, regardless of their operating system.")
        }
    }
}
