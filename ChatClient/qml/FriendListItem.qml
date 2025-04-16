import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    width: ListView.view.width
    height: 40
    color: mouseArea.containsMouse ? "#DEE2E6" : (selected ? "#007BFF" : "transparent")
    radius: 6

    property string friendName
    property bool selected: chatWindow.currentChatFriend === friendName
    signal clicked()

    Text {
        text: friendName
        color: selected ? "white" : "#343A40"
        font.pixelSize: 14
        font.bold: selected
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: parent.clicked()
    }
}