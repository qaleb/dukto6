import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: rootWindow
    width: 360
    height: 600
    visible: true
    color: theme.color6
    maximumHeight: 600
    maximumWidth: 360
    minimumHeight: 600
    minimumWidth: 360
    title: qsTr("Dukto 6")

    Component.onCompleted: {
        var rect = guiBehind.windowGeometry();

        // Apply x and y position only on desktop platforms
        if (Qt.platform.os === "windows" || Qt.platform.os === "linux" || Qt.platform.os === "osx") {
            x = rect.x;
            y = rect.y;
        }

        // Connect the signal emitted from guiBehind
        guiBehind.onTransferStart.connect(() => handleOverlayState("progress"));
        guiBehind.onReceiveCompleted.connect(() => {
                                                 handleOverlayState("");
                                                 duktoInner.handleTabClick("recent");
                                             });
        guiBehind.onGotoTextSnippet.connect(() => handleOverlayState("showtext"));
        guiBehind.onGotoSendPage.connect(() => handleOverlayState("send"));
        guiBehind.onGotoMessagePage.connect(() => handleOverlayState("message"));
        guiBehind.onHideAllOverlays.connect(() => handleOverlayState(""));
    }

    signal showIpList()
    signal showSettings()

    FontLoader {
        id: duktofont
        source: "qrc:/src/assets//Klill-Light.ttf"
    }

    FontLoader {
        id: duktofontsmall
        source: "qrc:/src/assets//LiberationSans-Regular.ttf"
    }

    FontLoader {
        id: duktofonthappy
        source: "qrc:/src/assets//KGLikeASkyscraper.ttf"
    }

    DuktoInner {
        id: duktoInner
        anchors.fill: parent
        onShowIpList: handleOverlayState("ip")
        onShowSettings: {
            duktoOverlay.refreshSettingsColor();
            handleOverlayState("settings")
        }
    }

    UpdatesBox {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 100
    }

    DuktoOverlay {
        id: duktoOverlay
        anchors.fill: parent
    }

    Binding {
        target: guiBehind
        property: "overlayState"
        value: duktoOverlay.state
    }

    // JavaScript functions
    function handleOverlayState(command) {
        duktoOverlay.state = command;
    }

    function saveWindowGeometry() {
        var rect = Qt.rect(rootWindow.x, rootWindow.y, rootWindow.width, rootWindow.height);
        guiBehind.setWindowGeometry(rect);
    }

    onClosing: function(close) {
        if (Qt.platform.os === "android") {
            if (duktoOverlay.state !== "") {
                duktoOverlay.state = ""
                return close.accepted = false
            }
        }
        close.accepted = true
        saveWindowGeometry();
        guiBehind.close();
    }
}
