import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    color: theme.backgroundColor

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // 左侧面板
        Rectangle {
            Layout.preferredWidth: 250
            Layout.fillHeight: true
            color: theme.sidebarColor
            border.color: theme.borderColor
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
                        text: "个人资料"
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        background: Rectangle {
                            color: parent.pressed ? theme.primaryButtonPressedColor : theme.primaryButtonColor
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
                        onClicked: stackView.push(profilePage)
                    }

                    Button {
                        text: "设置"
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        background: Rectangle {
                            color: parent.pressed ? theme.primaryButtonPressedColor : theme.primaryButtonColor
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
                        text: "群聊"
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        background: Rectangle {
                            color: parent.pressed ? theme.successButtonPressedColor : theme.successButtonColor
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
                            chatWindow.refreshGroupList();
                            stackView.push(groupChatPage);
                        }
                    }

                    Button {
                        text: "登出"
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        background: Rectangle {
                            color: parent.pressed ? theme.dangerButtonPressedColor : theme.dangerButtonColor
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
                        color: theme.primaryTextColor
                        placeholderTextColor: theme.secondaryTextColor
                        background: Rectangle {
                            border.color: searchField.activeFocus ? theme.inputFocusBorderColor : theme.inputBorderColor
                            border.width: 1
                            radius: 15
                            color: theme.inputBackgroundColor
                        }
                    }
                    Button {
                        text: "+"
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        font.bold: true
                        background: Rectangle {
                            color: parent.pressed ? theme.primaryButtonPressedColor : theme.primaryButtonColor
                            radius: 15
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: chatWindow.sendFriendRequest(searchField.text)
                    }
                }

                Button {
                    text: "搜索用户"
                    Layout.fillWidth: true
                    font.pixelSize: 13
                    background: Rectangle {
                        color: parent.pressed ? theme.secondaryButtonPressedColor : theme.secondaryButtonColor
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

                FriendList {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    friendModel: chatWindow.friendList
                    requestModel: chatWindow.friendRequests
                    
                    onFriendSelected: function(friendName) {
                        chatWindow.selectFriend(friendName)
                    }
                    
                    onAcceptRequest: function(friendName) {
                        chatWindow.acceptFriendRequest(friendName)
                    }
                    
                    onDeleteRequest: function(friendName) {
                        chatWindow.deleteFriendRequest(friendName)
                    }
                    
                    onDeleteFriend: function(friendName) {
                        chatWindow.deleteFriend(friendName)
                    }
                }
            }
        }

        // 右侧聊天区域
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ListView {
                id: messageListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: ListModel { id: messageModel }
                delegate: MessageBubble {
                    sender: model.sender
                    content: model.content
                    timestamp: model.timestamp
                    avatarSource: model.avatarSource || ""
                    isOwnMessage: model.sender === chatWindow.currentNickname
                    width: parent.width
                }
                clip: true
                ScrollBar.vertical: ScrollBar {}
                onCountChanged: Qt.callLater(function() {
                    messageListView.positionViewAtEnd();
                })
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: theme.backgroundColor
                border.color: theme.borderColor
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    TextField {
                        id: messageInput
                        Layout.fillWidth: true
                        placeholderText: "输入消息..."
                        font.pixelSize: 14
                        background: Rectangle {
                            border.color: messageInput.activeFocus ? "#80BDFF" : "#CED4DA"
                            border.width: 1
                            radius: 4
                        }
                        Keys.onReturnPressed: {
                            if (text.trim() !== "") {
                                chatWindow.sendMessage(text);
                                text = "";
                            }
                        }
                    }

                    Button {
                        text: "发送"
                        Layout.preferredWidth: 80
                        font.pixelSize: 14
                        background: Rectangle {
                            color: parent.pressed ? "#0056b3" : "#007BFF"
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (messageInput.text.trim() !== "") {
                                chatWindow.sendMessage(messageInput.text);
                                messageInput.text = "";
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: chatWindow
        function onMessageReceived(sender, content, timestamp) {
            messageModel.append({
                "sender": sender,
                "content": content,
                "timestamp": timestamp
            });
        }
        function onChatDisplayCleared() {
            messageModel.clear();
        }
    }
}