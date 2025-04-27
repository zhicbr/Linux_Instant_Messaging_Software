import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.1 // Use Qt.labs.platform instead of QtQuick.Dialogs

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

                    Button {
                        id: imageButton
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40

                        background: Rectangle {
                            color: parent.pressed ? "#e0e0e0" : "#f0f0f0"
                            radius: 4
                        }

                        contentItem: Text {
                            text: "📷"
                            font.pixelSize: 20
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            imageFileDialog.open();
                        }

                        ToolTip {
                            visible: parent.hovered
                            text: "发送图片"
                            delay: 500
                        }
                    }

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

    // 图片选择对话框
    FileDialog {
        id: imageFileDialog
        title: "选择图片"
        nameFilters: ["图片文件 (*.jpg *.jpeg *.png *.gif)"]
        fileMode: FileDialog.OpenFile

        onAccepted: {
            // 发送图片消息
            chatWindow.sendImageMessage(imageFileDialog.file.toString());
        }
    }

    Connections {
        target: chatWindow
        function onMessageReceived(sender, content, timestamp, avatarSource) {
            messageModel.append({
                "sender": sender,
                "content": content,
                "timestamp": timestamp,
                "avatarSource": avatarSource
            });
        }
        function onChatDisplayCleared() {
            messageModel.clear();
        }
        function onImageDownloaded(imageId, localPath) {
            console.log("图片下载完成，ID:", imageId, "本地路径:", localPath);
            // 不再需要刷新整个列表，我们会通过imageMessageUpdated信号更新特定消息
        }

        function onUpdatePlaceholderMessages(imageId, jsonContent) {
            console.log("更新图片占位符消息，ID:", imageId, "内容:", jsonContent);

            // 查找并更新所有包含"[图片加载中...]"的消息
            for (var i = 0; i < messageModel.count; i++) {
                var content = messageModel.get(i).content;

                // 检查是否是占位符消息
                if (content === "[图片加载中...]") {
                    console.log("找到图片占位符消息，索引:", i);

                    // 获取当前消息的所有属性
                    var currentMessage = messageModel.get(i);
                    var sender = currentMessage.sender;
                    var timestamp = currentMessage.timestamp;
                    var avatarSource = currentMessage.avatarSource;

                    // 更新消息内容，保留其他属性
                    messageModel.set(i, {
                        "sender": sender,
                        "content": jsonContent,
                        "timestamp": timestamp,
                        "avatarSource": avatarSource
                    });
                }
            }

            // 强制ListView刷新
            messageListView.forceLayout();
        }

        function onImageMessageUpdated(imageId, jsonContent) {
            console.log("更新图片消息，ID:", imageId, "内容:", jsonContent);

            // 查找并更新包含该图片ID的消息
            for (var i = 0; i < messageModel.count; i++) {
                var content = messageModel.get(i).content;

                // 检查消息内容是否包含该图片ID
                if (content.indexOf(imageId) !== -1) {
                    console.log("找到包含图片ID的消息，索引:", i);

                    // 更新消息内容
                    messageModel.set(i, {
                        "content": jsonContent
                    });

                    // 获取当前消息的所有属性
                    var currentMessage = messageModel.get(i);
                    var sender = currentMessage.sender;
                    var timestamp = currentMessage.timestamp;
                    var avatarSource = currentMessage.avatarSource;

                    // 更新消息内容，保留其他属性
                    messageModel.set(i, {
                        "sender": sender,
                        "content": jsonContent,
                        "timestamp": timestamp,
                        "avatarSource": avatarSource
                    });

                    // 如果有多个消息包含同一个图片ID，继续查找
                    continue;
                }

                // 尝试解析JSON内容
                try {
                    var contentObj = JSON.parse(content);
                    if (contentObj && contentObj.type === "image" && contentObj.imageId === imageId) {
                        console.log("找到包含图片ID的JSON消息，索引:", i);

                        // 更新消息内容
                        messageModel.set(i, {
                            "content": jsonContent
                        });

                        // 获取当前消息的所有属性
                        var currentMessage = messageModel.get(i);
                        var sender = currentMessage.sender;
                        var timestamp = currentMessage.timestamp;
                        var avatarSource = currentMessage.avatarSource;

                        // 更新消息内容，保留其他属性
                        messageModel.set(i, {
                            "sender": sender,
                            "content": jsonContent,
                            "timestamp": timestamp,
                            "avatarSource": avatarSource
                        });
                    }
                } catch (e) {
                    // 不是JSON，继续检查下一条消息
                }
            }

            // 强制ListView刷新
            messageListView.forceLayout();
        }
    }
}