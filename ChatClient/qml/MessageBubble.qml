import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    width: ListView.view.width
    height: column.implicitHeight + 15
    color: "transparent"

    property bool isSelf: false
    property string sender
    property string content
    property string timestamp

    ColumnLayout {
        id: column
        width: parent.width - 20
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 2

        Text {
            text: isSelf ? "" : sender
            font.pixelSize: 13
            font.bold: true
            color: "#495057"
            visible: !isSelf
            Layout.alignment: Qt.AlignLeft
        }

        Rectangle {
            Layout.preferredWidth: Math.min(implicitWidth, parent.width * 0.75)
            Layout.alignment: isSelf ? Qt.AlignRight : Qt.AlignLeft
            implicitWidth: messageText.implicitWidth + 20
            implicitHeight: messageText.implicitHeight + 20
            color: isSelf ? "#007BFF" : "#E9ECEF"
            radius: 20

            Text {
                id: messageText
                text: content
                color: isSelf ? "white" : "#212529"
                font.pixelSize: 15
                wrapMode: Text.Wrap
                anchors.centerIn: parent
                width: Math.min(implicitWidth, parent.width * 0.75 - 20)
            }
        }

        Text {
            text: timestamp
            font.pixelSize: 11
            color: "#868E96"
            Layout.alignment: isSelf ? Qt.AlignRight : Qt.AlignLeft
        }
    }
}