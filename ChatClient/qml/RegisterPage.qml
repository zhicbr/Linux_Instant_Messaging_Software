import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    color: "#FFFFFF"
    radius: 12
    border.color: "#E9ECEF"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 30
        spacing: 10

        Text {
            text: "电子邮件"
            font.pixelSize: 13
            font.bold: true
            color: "#495057"
        }
        TextField {
            id: emailField
            Layout.fillWidth: true
            placeholderText: "输入您的电子邮件"
            font.pixelSize: 15
            inputMethodHints: Qt.ImhEmailCharactersOnly
            background: Rectangle {
                border.color: emailField.activeFocus ? "#80BDFF" : "#CED4DA"
                border.width: 1
                radius: 6
                color: "#FFFFFF"
            }
        }

        Text {
            text: "用户名"
            font.pixelSize: 13
            font.bold: true
            color: "#495057"
        }
        TextField {
            id: nicknameField
            Layout.fillWidth: true
            placeholderText: "选择一个用户名"
            font.pixelSize: 15
            background: Rectangle {
                border.color: nicknameField.activeFocus ? "#80BDFF" : "#CED4DA"
                border.width: 1
                radius: 6
                color: "#FFFFFF"
            }
        }

        Text {
            text: "密码"
            font.pixelSize: 13
            font.bold: true
            color: "#495057"
        }
        TextField {
            id: passwordField
            Layout.fillWidth: true
            placeholderText: "创建密码"
            echoMode: TextInput.Password
            font.pixelSize: 15
            background: Rectangle {
                border.color: passwordField.activeFocus ? "#80BDFF" : "#CED4DA"
                border.width: 1
                radius: 6
                color: "#FFFFFF"
            }
        }

        Button {
            text: "注册"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 150
            Layout.preferredHeight: 45
            font.pixelSize: 15
            font.bold: true
            background: Rectangle {
                color: "#0069d9"
                radius: 8
                border.width: 0
            }
            contentItem: Text {
                text: parent.text
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font: parent.font
            }
            onClicked: chatWindow.registerUser(emailField.text, nicknameField.text, passwordField.text)
        }
    }
}