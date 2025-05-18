import QtQuick

Rectangle {
    id: root
    width: 32
    height: 32
    border.color: boxArea.containsMouse ? "#555555" : "#f0f0f0"
    border.width: boxArea.containsMouse ? 2 : 2
    clip: true

    signal clicked(color color)

    Image {
        anchors.fill: parent
        source: "qrc:/src/assets/TileGradient.png"
    }

    MouseArea {
        id: boxArea
        hoverEnabled: true
        anchors.fill: parent
        onClicked: root.clicked(parent.color)
    }
}
