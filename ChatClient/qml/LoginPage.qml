import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    color: theme.inputBackgroundColor
    radius: 12
    border.color: theme.borderColor
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 30
        spacing: 10

        Text {
            text: "用户名"
            font.pixelSize: 13
            font.bold: true
            color: theme.primaryTextColor
        }
        TextField {
            id: nicknameField
            Layout.fillWidth: true
            placeholderText: "输入您的用户名"
            font.pixelSize: 15
            color: theme.primaryTextColor
            placeholderTextColor: theme.secondaryTextColor
            background: Rectangle {
                border.color: nicknameField.activeFocus ? theme.inputFocusBorderColor : theme.inputBorderColor
                border.width: 1
                radius: 6
                color: theme.inputBackgroundColor
            }
        }

        Text {
            text: "密码"
            font.pixelSize: 13
            font.bold: true
            color: theme.primaryTextColor
        }
        TextField {
            id: passwordField
            Layout.fillWidth: true
            placeholderText: "输入您的密码"
            echoMode: TextInput.Password
            font.pixelSize: 15
            color: theme.primaryTextColor
            placeholderTextColor: theme.secondaryTextColor
            background: Rectangle {
                border.color: passwordField.activeFocus ? theme.inputFocusBorderColor : theme.inputBorderColor
                border.width: 1
                radius: 6
                color: theme.inputBackgroundColor
            }
        }

        Button {
            text: "登录"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 150
            Layout.preferredHeight: 45
            font.pixelSize: 15
            font.bold: true
            background: Rectangle {
                color: theme.primaryButtonColor
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
            onClicked: chatWindow.login(nicknameField.text, passwordField.text)
        }
    }
}