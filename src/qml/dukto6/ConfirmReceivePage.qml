import QtQuick

Rectangle {
    id: confirmPage
    color: "#00000000"

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        height: 210

        Rectangle {
            color: theme.color2
            anchors.fill: parent
        }

        SmoothText {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 15
            anchors.topMargin: 5
            font.pixelSize: 56
            text: qsTr("Incoming transfer")
        }

        SText {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 68
            anchors.leftMargin: 17
            anchors.rightMargin: 17
            font.pixelSize: 14
            wrapMode: Text.Wrap
            text: qsTr("Accept files from %1?").arg(guiBehind.pendingIncomingSender)
        }

        Button {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: 17
            anchors.bottomMargin: 15
            width: 110
            label: qsTr("Reject")
            onClicked: guiBehind.rejectIncomingTransfer()
        }

        ButtonDark {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 17
            anchors.bottomMargin: 15
            width: 110
            label: qsTr("Accept")
            onClicked: guiBehind.acceptIncomingTransfer()
        }
    }
}
