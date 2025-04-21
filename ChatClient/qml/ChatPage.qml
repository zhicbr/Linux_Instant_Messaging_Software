import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    color: "#F8F9FA"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // 左侧面板
        Rectangle {
            Layout.preferredWidth: 250
            Layout.fillHeight: true
            color: "#E9ECEF"
            border.color: "#DEE2E6"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                // 添加设置和登出按钮
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Button {
                        text: "设置"
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        background: Rectangle {
                            color: parent.pressed ? "#0062cc" : "#0069d9"
                            radius: 6
                            border.width: 0
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font: parent.font
                        }
                        onClicked: stackView.push(settingsPage)
                    }

                    Button {
                        text: "登出"
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        background: Rectangle {
                            color: parent.pressed ? "#c82333" : "#dc3545"
                            radius: 6
                            border.width: 0
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font: parent.font
                        }
                        onClicked: {
                            chatWindow.logout();
                        }
                    }
                }

                RowLayout {
                    spacing: 5
                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: "搜索或添加好友..."
                        font.pixelSize: 13
                        background: Rectangle {
                            border.color: searchField.activeFocus ? "#80BDFF" : "#CED4DA"
                            border.width: 1
                            radius: 15
                            color: "#FFFFFF"
                        }
                    }
                    Button {
                        text: "+"
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        font.bold: true
                        background: Rectangle {
                            color: parent.pressed ? "#0056b3" : "#007BFF"
                            radius: 15
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: chatWindow.addFriend(searchField.text)
                    }
                }

                Button {
                    text: "搜索用户"
                    Layout.fillWidth: true
                    font.pixelSize: 13
                    background: Rectangle {
                        color: parent.pressed ? "#5A6268" : "#6C757D"
                        radius: 6
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: chatWindow.searchUser(searchField.text)
                }

                Text {
                    text: "好友"
                    font.pixelSize: 11
                    font.bold: true
                    color: "#6C757D"
                    textFormat: Text.PlainText
                    Layout.topMargin: 5
                    Layout.leftMargin: 5
                }

                ListView {
                    id: friendList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: chatWindow.getFriendList()
                    delegate: FriendListItem {
                        friendName: modelData
                        onClicked: chatWindow.selectFriend(friendName)
                    }
                    clip: true
                }
            }
        }

        // 右侧聊天区域
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ListView {
                id: chatDisplay
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: ListModel { id: messageModel }
                delegate: MessageBubble {
                    sender: model.sender
                    content: model.content
                    timestamp: model.timestamp
                    isSelf: sender === chatWindow.currentNickname
                }
                clip: true
                ScrollBar.vertical: ScrollBar {}
                onCountChanged: Qt.callLater(function() {
                    chatDisplay.positionViewAtEnd();
                })
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "#F8F9FA"
                border.color: "#DEE2E6"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    TextField {
                        id: messageField
                        Layout.fillWidth: true
                        placeholderText: chatWindow.currentChatFriend ? "给 " + chatWindow.currentChatFriend + " 发送消息..." : "输入消息..."
                        font.pixelSize: 14
                        background: Rectangle {
                            border.color: messageField.activeFocus ? "#80BDFF" : "#CED4DA"
                            border.width: 1
                            radius: 20
                            color: "#FFFFFF"
                        }
                        Keys.onReturnPressed: {
                            chatWindow.sendMessage(text);
                            text = "";
                        }
                    }

                    Button {
                        text: "发送"
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        enabled: chatWindow.currentChatFriend !== ""
                        background: Rectangle {
                            color: parent.enabled ? (parent.pressed ? "#0056b3" : "#007BFF") : "#B0D7FF"
                            radius: 20
                        }
                        contentItem: Text {
                            text: parent.text
                            color: parent.enabled ? "white" : "#E0F0FF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            chatWindow.sendMessage(messageField.text);
                            messageField.text = "";
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: chatWindow
        function onMessageReceived(sender, content, timestamp) {
            messageModel.append({ sender: sender, content: content, timestamp: timestamp });
        }
        function onChatDisplayCleared() {
            messageModel.clear();
        }
        function onFriendListUpdated(friends) {
            friendList.model = friends;
        }
    }
}