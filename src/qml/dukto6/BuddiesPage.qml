import QtQuick
import QtQuick.Controls

Item {
    id: buddiesPage
    clip: true

    Rectangle {
        anchors.fill: parent
        color: theme.color6
    }

    DropArea {
        id: dropTarget
        anchors.fill: parent
        enabled: guiBehind.overlayState === "" && buddiesList.count === 3 && guiBehind.canAcceptDrop()

        onDropped: function(drop) {
            if(guiBehind.canAcceptDrop()){
                if (drop.hasUrls) {
                    var filesList = drop.urls.map(function(url) { return url.toString(); });
                    // Pass the list of file URLs to the sendDroppedFiles function
                    guiBehind.sendDroppedFiles(filesList);
                }
            }
        }
    }

    Rectangle {
        id: sharePanel
        objectName: "pendingSharePanel"
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 12 }
        visible: guiBehind.hasPendingShare
        height: shareDetails.implicitHeight + 24
        color: theme.color8
        radius: 6
        border.color: theme.accentForeground
        Column {
            id: shareDetails
            x: 12; y: 12; width: parent.width - 24; spacing: 8
            SText { width: parent.width; text: qsTr("Choose a device"); color: theme.color4; font.pixelSize: 16; wrapMode: Text.WordWrap }
            SText {
                objectName: "pendingShareSummary"
                width: parent.width; text: guiBehind.pendingShareSummary; color: theme.color5
                font.pixelSize: 12; wrapMode: Text.WrapAnywhere; maximumLineCount: 3; elide: Text.ElideRight
            }
            Row {
                width: parent.width; height: 36; spacing: 12
                ActionButton {
                    objectName: "cancelSharedSelection"
                    width: 88; height: 36; text: qsTr("Cancel")
                    Accessible.name: qsTr("Cancel shared selection")
                    onClicked: guiBehind.discardPendingShare()
                }
                SText {
                    width: parent.width - 100; height: parent.height; verticalAlignment: Text.AlignVCenter
                    text: qsTr("%1 selections waiting").arg(guiBehind.pendingShareCount)
                    visible: guiBehind.pendingShareCount > 1; color: theme.color5; font.pixelSize: 11; wrapMode: Text.WordWrap
                }
            }
        }
    }

    ListView {
        id: buddiesList
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; top: sharePanel.visible ? sharePanel.bottom : parent.top; topMargin: sharePanel.visible ? 12 : 0 }
        spacing: 10
        anchors.leftMargin: 25
        anchors.rightMargin: 0
        model: buddiesListData // Use the ListModel defined above

        Component {
            id: contactDelegate
            BuddyListComponent {
                buddyIp: ip
                buddyAvatar: avatar
                buddyGeneric: generic
                buddyUsername: username
                buddySystem: system
                buddyIpAddress: ip !== "IP" ? ip.substring(7) : ""
                buddyOsLogo: oslogo
                buddyShowBack: showback
            }
        }

        delegate: contactDelegate

    }
}
