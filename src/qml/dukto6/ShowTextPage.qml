import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Controls.impl

Rectangle {
    id: showTextPage
    objectName: "textSnippetPage"
    color: theme.color6
    signal back()
    signal backOnSend()
    readonly property bool isAndroid: Qt.platform.os === "android"
    property alias textInputFocus: editor.focus
    // QInputMethod reports the keyboard rectangle in window coordinates. Only
    // subtract its overlap; a window already resized by the OS needs no inset.
    property real keyboardInset: {
        if (visible && guiBehind.androidKeyboardTop >= 0) {
            const top = mapFromGlobal(0, guiBehind.androidKeyboardTop).y;
            return Math.max(0, Math.min(height - 150, height - top));
        }
        const rect = Qt.inputMethod.keyboardRectangle;
        if (!visible || !Qt.inputMethod.visible || rect.height <= 0) return 0;
        const top = mapFromItem(null, rect.x, rect.y).y;
        return Math.max(0, Math.min(height - 150, height - top));
    }
    function sendSnippet() {
        if (!guiBehind.textSnippetSending || !editor.text.length) return;
        editor.deselect();
        editor.focus = false;
        Qt.inputMethod.hide();
        guiBehind.textSnippet = editor.text;
        guiBehind.sendText();
    }
    function openEditingMenu() {
        if (isAndroid) return;
        editingMenu.popup(editButton, 0, -editingMenu.implicitHeight);
    }
    onVisibleChanged: {
        if (!visible) { editor.focus = false; Qt.inputMethod.hide(); }
        else if (guiBehind.textSnippetSending) Qt.callLater(() => {
            if (showTextPage.visible) editor.forceActiveFocus();
        });
    }

    Item {
        id: usableArea
        anchors.fill: parent
        anchors.bottomMargin: showTextPage.keyboardInset
        ColorImage {
            id: backIcon
            x: 8; y: 8; width: 40; height: 40
            sourceSize: Qt.size(width, height)
            source: "qrc:/src/assets/ArrowBack.svg"; color: theme.color5
            Accessible.name: qsTr("Back")
            MouseArea {
                anchors.fill: parent
                onClicked: {
                    editor.focus = false; Qt.inputMethod.hide();
                    if (guiBehind.textSnippetSending) showTextPage.backOnSend();
                    else showTextPage.back();
                }
            }
        }
        Text {
            x: 60; y: 9; width: parent.width - 76
            text: guiBehind.textSnippetSending ? qsTr("Send text") : qsTr("Received text")
            font.pixelSize: 24; color: theme.color4; elide: Text.ElideRight
        }
        SText {
            x: 60; y: 40; width: parent.width - 76
            text: (guiBehind.textSnippetSending ? qsTr("To %1") : qsTr("From %1")).arg(guiBehind.textSnippetBuddy)
            elide: Text.ElideRight; font.pixelSize: 12; color: theme.color5
        }
        Rectangle {
            anchors { top: parent.top; topMargin: 70; left: parent.left; right: parent.right; bottom: actionBar.top; margins: 12; bottomMargin: 10 }
            color: theme.color8; radius: 6
            border.color: editor.activeFocus ? theme.accentForeground : theme.color5
            C.ScrollView {
                anchors.fill: parent
                clip: true
                contentWidth: availableWidth
                C.ScrollBar.horizontal.policy: C.ScrollBar.AlwaysOff
                C.TextArea {
                    id: editor
                    objectName: "snippetEditor"
                    padding: 12
                    color: theme.color4
                    selectionColor: theme.color2; selectedTextColor: theme.accentText
                    placeholderText: guiBehind.textSnippetSending ? qsTr("Type or paste your message…") : ""
                    placeholderTextColor: theme.color5
                    font.pixelSize: 15
                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    readOnly: !guiBehind.textSnippetSending
                    text: guiBehind.textSnippet
                    selectByMouse: true
                    selectByKeyboard: true
                    persistentSelection: true
                    background: Item {}
                    Accessible.name: guiBehind.textSnippetSending ? qsTr("Message to send") : qsTr("Received message")
                    onPressAndHold: event => {
                        if (showTextPage.isAndroid) {
                            // Select at the touch point and consume the release so
                            // TextArea cannot collapse Android's native selection.
                            cursorPosition = positionAt(event.x, event.y);
                            selectWord();
                            event.accepted = true;
                            return;
                        }
                        if (!selectedText.length) selectWord();
                        event.accepted = true;
                        showTextPage.openEditingMenu();
                    }
                    TapHandler {
                        enabled: !showTextPage.isAndroid
                        acceptedButtons: Qt.RightButton
                        onTapped: showTextPage.openEditingMenu()
                    }
                    Keys.onPressed: event => {
                        if ((event.modifiers & Qt.ControlModifier) && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
                            showTextPage.sendSnippet(); event.accepted = true;
                        }
                    }
                }
            }
        }
        Row {
            id: actionBar
            objectName: "snippetActionBar"
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: 12 }
            height: 44; spacing: 8
            ActionButton {
                id: editButton
                objectName: "snippetEditButton"
                visible: !showTextPage.isAndroid
                width: 70; height: parent.height
                text: guiBehind.textSnippetSending ? qsTr("Edit") : qsTr("Select")
                onClicked: showTextPage.openEditingMenu()
            }
            ActionButton {
                id: pasteButton
                objectName: "snippetPasteButton"
                width: guiBehind.textSnippetSending ? 100 : parent.width - (editButton.visible ? editButton.width + parent.spacing : 0)
                height: parent.height
                text: guiBehind.textSnippetSending ? qsTr("Paste") : qsTr("Copy")
                enabled: guiBehind.textSnippetSending ? editor.canPaste : editor.text.length > 0
                onClicked: {
                    if (guiBehind.textSnippetSending) { editor.forceActiveFocus(); editor.paste(); }
                    else { if (!editor.selectedText.length) editor.selectAll(); editor.copy(); }
                }
            }
            ActionButton {
                objectName: "snippetSendButton"
                visible: guiBehind.textSnippetSending
                width: parent.width - pasteButton.width - parent.spacing - (editButton.visible ? editButton.width + parent.spacing : 0)
                height: parent.height
                text: qsTr("Send")
                primary: true
                enabled: editor.text.length > 0
                onClicked: showTextPage.sendSnippet()
                C.ToolTip.visible: hovered && Qt.platform.os !== "android"
                C.ToolTip.text: qsTr("Send (Ctrl+Enter)")
            }
        }
    }
    C.Menu {
        id: editingMenu
        objectName: "snippetEditingMenu"
        popupType: C.Popup.Item
        width: 220
        focus: Qt.platform.os !== "android"
        // Keep the selection and input focus when opening the editing menu.
        C.MenuItem { text: qsTr("Cut"); enabled: !editor.readOnly && editor.selectedText.length > 0; onTriggered: editor.cut() }
        C.MenuItem { text: qsTr("Copy"); enabled: editor.selectedText.length > 0; onTriggered: editor.copy() }
        C.MenuItem { text: qsTr("Paste"); enabled: !editor.readOnly && editor.canPaste; onTriggered: editor.paste() }
        C.MenuSeparator {}
        C.MenuItem { text: qsTr("Select all"); enabled: editor.text.length > 0; onTriggered: editor.selectAll() }
        C.MenuItem { text: qsTr("Undo"); enabled: !editor.readOnly && editor.canUndo; onTriggered: editor.undo() }
        C.MenuItem { text: qsTr("Redo"); enabled: !editor.readOnly && editor.canRedo; onTriggered: editor.redo() }
        delegate: C.MenuItem {
            id: menuItem
            implicitHeight: 44
            contentItem: Text {
                text: menuItem.text; font.pixelSize: 14; verticalAlignment: Text.AlignVCenter
                color: !menuItem.enabled ? theme.color5 : (menuItem.highlighted ? theme.accentText : theme.color4)
            }
            background: Rectangle { color: menuItem.highlighted ? theme.color2 : theme.color8 }
        }
        background: Rectangle { implicitWidth: 220; implicitHeight: 40; color: theme.color8; radius: 6; border.color: theme.color5 }
    }
}
