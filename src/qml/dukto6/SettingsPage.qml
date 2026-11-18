import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Controls.impl
import QtQuick.Dialogs

Rectangle {
    id: settingsPage
    readonly property bool isAndroid: Qt.platform.os === "android"
    color: theme.color6
    signal back()
    function refreshColor() { picker.setColor(theme.color2); }
    onVisibleChanged: if (visible) refreshColor()

    ColorImage {
        id: backIcon
        x: 8; y: 8; width: 40; height: 40
        source: "qrc:/src/assets/ArrowBack.svg"
        color: theme.color5
        MouseArea { anchors.fill: parent; onClicked: settingsPage.back() }
        Accessible.name: qsTr("Back")
    }
    SmoothText {
        x: 60; y: 6
        font.pixelSize: 56
        text: qsTr("Settings")
        color: theme.color3
    }

    component Field: C.TextField {
        implicitHeight: 40
        font.pixelSize: 14
        color: theme.color4
        selectionColor: theme.color2
        selectedTextColor: theme.accentText
        padding: 10
        background: Rectangle {
            radius: 6; color: theme.color8
            border.color: parent.activeFocus ? theme.accentForeground : theme.color5
        }
    }

    C.ScrollView {
        id: scroll
        anchors { top: parent.top; topMargin: 68; bottom: parent.bottom; left: parent.left; right: parent.right }
        contentWidth: availableWidth
        clip: true
        C.ScrollBar.horizontal.policy: C.ScrollBar.AlwaysOff
        Column {
            x: 16
            width: scroll.availableWidth - 32
            spacing: 16
            topPadding: 4
            bottomPadding: 20

            Column {
                width: parent.width; spacing: 6
                SText { text: qsTr("Device name"); color: theme.color4; font.pixelSize: 14 }
                Field {
                    objectName: "deviceNameField"
                    width: parent.width
                    text: guiBehind.buddyName
                    Accessible.name: qsTr("Device name")
                    onEditingFinished: if (text.trim().length) guiBehind.setBuddyName(text.trim())
                }
            }

            Item {
                id: confirmationRow
                width: parent.width
                height: Math.max(confirmLabel.implicitHeight + 8, 48)
                Column {
                    id: confirmLabel
                    objectName: "confirmReceiveLabel"
                    width: parent.width - 58
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    SText { width: parent.width; wrapMode: Text.WordWrap; text: qsTr("Ask before receiving"); font.pixelSize: 14; color: theme.color4 }
                    SText { width: parent.width; wrapMode: Text.WordWrap; text: qsTr("Confirm incoming files and text"); font.pixelSize: 12; color: theme.color5 }
                }
                C.Switch {
                    id: confirmSwitch
                    objectName: "confirmReceiveSwitch"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 48; height: 48; padding: 0
                    checked: guiBehind.confirmBeforeReceive
                    onToggled: guiBehind.confirmBeforeReceive = checked
                    Accessible.name: qsTr("Ask before receiving")
                    indicator: Rectangle {
                        x: 4; y: 12; width: 40; height: 24; radius: 12
                        color: confirmSwitch.checked ? theme.color2 : theme.color7
                        border.color: confirmSwitch.visualFocus ? theme.accentForeground : theme.color5
                        border.width: confirmSwitch.visualFocus ? 2 : 1
                        Rectangle {
                            x: confirmSwitch.checked ? 20 : 4; y: 4; width: 16; height: 16; radius: 8
                            color: confirmSwitch.checked ? theme.accentText : theme.color4
                        }
                    }
                }
            }

            Row {
                width: parent.width; height: 44; spacing: 12
                SText {
                    width: parent.width - themeModeSelector.width - 12
                    height: parent.height; verticalAlignment: Text.AlignVCenter
                    text: qsTr("Theme mode"); color: theme.color4; font.pixelSize: 14
                }
                C.ComboBox {
                    id: themeModeSelector
                    objectName: "themeModeSelector"
                    width: Math.min(160, parent.width * 0.52); height: 44
                    leftPadding: 12; rightPadding: 32
                    model: [qsTr("System"), qsTr("Light"), qsTr("Dark")]
                    currentIndex: Math.max(0, ["system", "light", "dark"].indexOf(guiBehind.themeMode))
                    onActivated: index => { guiBehind.themeMode = ["system", "light", "dark"][index]; }
                    Accessible.name: qsTr("Theme mode")
                    contentItem: Text {
                        objectName: "themeModeText"
                        text: themeModeSelector.displayText; color: theme.color4
                        font.pixelSize: 14; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
                    }
                    indicator: Canvas {
                        x: parent.width - 24; y: (parent.height - height) / 2
                        width: 12; height: 8
                        property color arrowColor: theme.color4
                        onArrowColorChanged: requestPaint()
                        onPaint: {
                            const context = getContext("2d");
                            context.clearRect(0, 0, width, height);
                            context.fillStyle = arrowColor;
                            context.beginPath(); context.moveTo(0, 0);
                            context.lineTo(width, 0); context.lineTo(width / 2, height);
                            context.closePath(); context.fill();
                        }
                    }
                    background: Rectangle {
                        radius: 6; color: theme.color8
                        border.width: themeModeSelector.visualFocus ? 2 : 1
                        border.color: themeModeSelector.visualFocus ? theme.accentForeground : theme.color5
                    }
                    delegate: C.ItemDelegate {
                        required property int index
                        required property string modelData
                        width: themeModeSelector.width; height: 44
                        highlighted: themeModeSelector.highlightedIndex === index
                        contentItem: Text {
                            text: parent.modelData; font.pixelSize: 14; verticalAlignment: Text.AlignVCenter
                            color: parent.highlighted ? theme.accentText : theme.color4
                        }
                        background: Rectangle { color: parent.highlighted ? theme.color2 : theme.color8 }
                    }
                    popup: C.Popup {
                        y: themeModeSelector.height + 4; width: themeModeSelector.width; padding: 1
                        implicitHeight: contentItem.implicitHeight + 2
                        background: Rectangle { color: theme.color8; radius: 6; border.color: theme.color5 }
                        contentItem: ListView {
                            clip: true; implicitHeight: contentHeight
                            model: themeModeSelector.popup.visible ? themeModeSelector.delegateModel : null
                            currentIndex: themeModeSelector.highlightedIndex
                        }
                    }
                }
            }

            Column {
                width: parent.width; spacing: 8
                SText { text: qsTr("Accent color"); color: theme.color4; font.pixelSize: 14 }
                Row {
                    width: parent.width; spacing: 18
                    ColorPicker {
                        id: picker
                        width: parent.width - presets.width - parent.spacing
                        height: width - 15
                        color: "transparent"
                        onChanged: guiBehind.changeThemeColor(colorValue)
                    }
                    Column {
                        id: presets
                        width: 116; spacing: 10
                        Grid {
                            columns: 3; spacing: 10
                            Repeater {
                                model: ["#248B00", "#A80000", "#3A6CBC", "#2e3436", "#B77994", "#5B2F42", "#353B56", "#FB8504", "#6D0D71"]
                                ColorBox { required property string modelData; color: modelData; onClicked: color => picker.setColor(color) }
                            }
                        }
                        Field {
                            width: parent.width; height: 36
                            text: theme.color2.toUpperCase()
                            Accessible.name: qsTr("Accent color hex code")
                            onTextEdited: if (/^#[0-9A-Fa-f]{6}$/.test(text)) picker.setColor(text)
                        }
                    }
                }
            }

            Column {
                visible: !settingsPage.isAndroid
                width: parent.width; spacing: 6
                SText { text: qsTr("Save received files in"); color: theme.color4; font.pixelSize: 14 }
                SText { width: parent.width; text: guiBehind.currentPath; elide: Text.ElideMiddle; color: theme.color5; font.pixelSize: 12 }
                ActionButton { width: parent.width; text: qsTr("Change folder"); onClicked: folderDialog.open() }
            }

            Column {
                visible: settingsPage.isAndroid
                width: parent.width; spacing: 10
                ActionButton {
                    id: advancedButton
                    width: parent.width; checkable: true
                    text: checked ? qsTr("Hide storage & battery") : qsTr("Storage & battery")
                }
                Column {
                    width: parent.width; spacing: 10; visible: advancedButton.checked
                    SText {
                        width: parent.width; wrapMode: Text.WordWrap; color: theme.color5; font.pixelSize: 12
                        text: guiBehind.androidCustomStorage ? guiBehind.androidStorageDescription
                              : qsTr("Files are saved automatically to Gallery, Documents or Downloads.")
                    }
                    ActionButton { width: parent.width; text: qsTr("Choose shared folder"); onClicked: guiBehind.chooseAndroidStorage() }
                    ActionButton {
                        width: parent.width
                        visible: guiBehind.androidCustomStorage && guiBehind.androidAutomaticStorageAvailable
                        text: qsTr("Use automatic folders"); onClicked: guiBehind.useAutomaticAndroidStorage()
                    }
                    ActionButton { width: parent.width; text: qsTr("Battery settings"); onClicked: guiBehind.openBatterySettings() }
                }
            }
        }
    }
    FolderDialog {
        id: folderDialog
        title: qsTr("Change folder")
        options: FolderDialog.ShowDirsOnly
        currentFolder: guiBehind.currentPath
        onAccepted: guiBehind.changeDestinationFolder(selectedFolder)
    }
}
