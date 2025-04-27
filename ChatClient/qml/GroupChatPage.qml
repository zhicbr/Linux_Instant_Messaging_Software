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
                        text: "返回"
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        background: Rectangle {
                            color: parent.pressed ? theme.secondaryButtonPressedColor : theme.secondaryButtonColor
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
                            chatWindow.clearChatType();
                            stackView.pop();
                        }
                    }

                    Button {
                        text: "创建群聊"
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
                        onClicked: {
                            createGroupDialog.open();
                        }
                    }
                }

                // 群聊列表标题
                Rectangle {
                    Layout.fillWidth: true
                    height: 30
                    color: theme.borderColor

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10

                        Text {
                            text: "我的群聊"
                            font.bold: true
                            font.pixelSize: 14
                            color: theme.primaryTextColor
                        }

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "刷新"
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: 24
                            font.pixelSize: 12
                            background: Rectangle {
                                color: parent.pressed ? theme.secondaryButtonPressedColor : theme.secondaryButtonColor
                                radius: 4
                            }
                            contentItem: Text {
                                text: parent.text
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: chatWindow.refreshGroupList()
                        }
                    }
                }

                // 群聊列表
                ListView {
                    id: groupListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: chatWindow.groupList

                    delegate: Rectangle {
                        width: groupListView.width
                        height: 50
                        color: chatWindow.currentChatGroup === modelData.split(':')[0] ?
                            (appSettings.darkTheme ? "#3B4252" : "#CED4DA") :
                            (appSettings.darkTheme ? theme.sidebarColor : "#F8F9FA")

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                radius: 17
                                color: appSettings.darkTheme ? "#A6E3A1" : "#28A745"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.split(':')[1].charAt(0).toUpperCase()
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: appSettings.darkTheme ? "#1E1E2E" : "white"
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData.split(':')[1]
                                font.pixelSize: 14
                                color: theme.primaryTextColor
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                let groupId = modelData.split(':')[0];
                                // 立即设置UI状态，不等待服务器响应
                                groupTitle.text = chatWindow.getGroupName(groupId);

                                // 添加一个系统提示消息，表明正在加载聊天记录
                                messageModel.clear();
                                messageModel.append({
                                    "sender": "系统",
                                    "content": "正在加载聊天记录...",
                                    "timestamp": new Date().toLocaleTimeString(Qt.locale(), "hh:mm"),
                                    "avatarSource": "qrc:/images/default_avatar.png"
                                });

                                // 选择群聊并加载消息
                                chatWindow.selectGroup(groupId);

                                // 添加重试机制，如果2秒后仍未收到消息，则再次请求
                                retryTimer.groupId = groupId;
                                retryTimer.restart();
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }
            }
        }

        // 中间聊天区域
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 群聊标题
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                color: theme.sidebarColor
                border.color: theme.borderColor

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 15
                    anchors.rightMargin: 15

                    Text {
                        id: groupTitle
                        text: chatWindow.isGroupChat ?
                            chatWindow.getGroupName(chatWindow.currentChatGroup) : "请选择一个群聊"
                        font.pixelSize: 16
                        font.bold: true
                        color: theme.primaryTextColor
                    }
                }
            }

            ListView {
                id: chatDisplay
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
                    chatDisplay.positionViewAtEnd();
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
                        enabled: chatWindow.isGroupChat

                        background: Rectangle {
                            color: parent.enabled ?
                                (parent.pressed ? "#e0e0e0" : "#f0f0f0") :
                                (appSettings.darkTheme ? "#45475A" : "#B0B0B0")
                            radius: 4
                        }

                        contentItem: Text {
                            text: "📷"
                            font.pixelSize: 20
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: imageButton.enabled ? theme.primaryTextColor : theme.secondaryTextColor
                        }

                        onClicked: {
                            groupImageFileDialog.open();
                        }

                        ToolTip {
                            visible: parent.hovered && parent.enabled
                            text: "发送图片"
                            delay: 500
                        }
                    }

                    TextField {
                        id: messageInput
                        Layout.fillWidth: true
                        placeholderText: "输入消息..."
                        font.pixelSize: 14
                        enabled: chatWindow.isGroupChat
                        color: theme.primaryTextColor
                        placeholderTextColor: theme.secondaryTextColor
                        background: Rectangle {
                            border.color: messageInput.activeFocus ? theme.inputFocusBorderColor : theme.inputBorderColor
                            border.width: 1
                            radius: 4
                            color: theme.inputBackgroundColor
                        }
                        Keys.onReturnPressed: {
                            if (text.trim() !== "" && chatWindow.isGroupChat) {
                                chatWindow.sendGroupMessage(text);
                                text = "";
                            }
                        }
                    }

                    Button {
                        text: "发送"
                        Layout.preferredWidth: 80
                        font.pixelSize: 14
                        enabled: chatWindow.isGroupChat
                        background: Rectangle {
                            color: parent.enabled ?
                                (parent.pressed ? theme.primaryButtonPressedColor : theme.primaryButtonColor) :
                                (appSettings.darkTheme ? "#45475A" : "#B0B0B0")
                            radius: 4
                        }
                        contentItem: Text {
                            text: parent.text
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (messageInput.text.trim() !== "" && chatWindow.isGroupChat) {
                                chatWindow.sendGroupMessage(messageInput.text);
                                messageInput.text = "";
                            }
                        }
                    }
                }
            }
        }

        // 右侧群成员列表
        Rectangle {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            color: theme.sidebarColor
            border.color: theme.borderColor
            border.width: 1
            visible: chatWindow.isGroupChat

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Text {
                    text: "群成员"
                    font.bold: true
                    font.pixelSize: 16
                    color: theme.primaryTextColor
                }

                ListView {
                    id: membersListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: chatWindow.groupMembers

                    delegate: Rectangle {
                        width: membersListView.width
                        height: 40
                        color: appSettings.darkTheme ? theme.inputBackgroundColor : "#F8F9FA"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 5
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                radius: 15
                                color: chatWindow.isFriendOnline(modelData) ?
                                    (appSettings.darkTheme ? "#A6E3A1" : "#28A745") :
                                    (appSettings.darkTheme ? "#A6ADC8" : "#6C757D")

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.charAt(0).toUpperCase()
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: appSettings.darkTheme && chatWindow.isFriendOnline(modelData) ? "#1E1E2E" : "white"
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: modelData
                                font.pixelSize: 13
                                color: theme.primaryTextColor
                                elide: Text.ElideRight
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }
            }
        }
    }

    // 创建群聊对话框
    Popup {
        id: createGroupDialog
        property string title: "创建新群聊"
        modal: true
        width: 400
        height: 400

        anchors.centerIn: parent

        background: Rectangle {
            color: theme.inputBackgroundColor
            border.color: theme.borderColor
            border.width: 1
            radius: 6
        }

        Rectangle {
            id: dialogHeader
            width: parent.width
            height: 50
            color: theme.sidebarColor

            Text {
                anchors.centerIn: parent
                text: createGroupDialog.title
                font.pixelSize: 16
                font.bold: true
                color: theme.primaryTextColor
            }
        }

        // 使用自定义函数代替onAccepted
        function accept() {
            // 获取选中的好友作为群成员
            let selectedMembers = [];
            for (let i = 0; i < friendsCheckboxModel.count; i++) {
                if (friendsCheckboxModel.get(i).checked) {
                    selectedMembers.push(friendsCheckboxModel.get(i).name);
                }
            }

            if (selectedMembers.length > 0 && groupNameField.text.trim() !== "") {
                chatWindow.createGroup(groupNameField.text, selectedMembers);
                groupNameField.text = "";
                // 重置选中状态
                for (let i = 0; i < friendsCheckboxModel.count; i++) {
                    friendsCheckboxModel.setProperty(i, "checked", false);
                }
                close();
            } else {
                chatWindow.setStatusMessage("请输入群聊名称并选择至少一个成员");
            }
        }

        // 使用自定义函数代替onRejected
        function reject() {
            groupNameField.text = "";
            // 重置选中状态
            for (let i = 0; i < friendsCheckboxModel.count; i++) {
                friendsCheckboxModel.setProperty(i, "checked", false);
            }
            close();
        }

        contentItem: Item {
            anchors.fill: parent
            anchors.topMargin: dialogHeader.height

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 15
                spacing: 15

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        text: "群聊名称："
                        Layout.preferredWidth: 80
                        color: theme.primaryTextColor
                    }

                    TextField {
                        id: groupNameField
                        Layout.fillWidth: true
                        placeholderText: "输入群聊名称"
                    }
                }

                Label {
                    text: "选择群成员："
                    font.bold: true
                    color: theme.primaryTextColor
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    border.color: "#DEE2E6"

                    ScrollView {
                        anchors.fill: parent
                        clip: true

                        ListView {
                            id: friendsCheckList
                            anchors.fill: parent
                            model: ListModel { id: friendsCheckboxModel }

                            delegate: CheckBox {
                                text: name
                                checked: model.checked
                                onCheckedChanged: friendsCheckboxModel.setProperty(index, "checked", checked)
                            }

                            Component.onCompleted: {
                                // 填充好友列表
                                refreshFriendsList();
                            }
                        }
                    }
                }

                // 添加按钮行
                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight
                    spacing: 10

                    Button {
                        text: "取消"
                        onClicked: createGroupDialog.reject()
                    }

                    Button {
                        text: "确定"
                        onClicked: createGroupDialog.accept()
                    }
                }
            }
        }

        // 在对话框打开时刷新好友列表
        onOpened: {
            refreshFriendsList();
        }

        function refreshFriendsList() {
            friendsCheckboxModel.clear();
            for (let i = 0; i < chatWindow.friendList.length; i++) {
                friendsCheckboxModel.append({
                    name: chatWindow.friendList[i],
                    checked: false
                });
            }
        }
    }

    // 图片选择对话框
    FileDialog {
        id: groupImageFileDialog
        title: "选择图片"
        nameFilters: ["图片文件 (*.jpg *.jpeg *.png *.gif)"]
        fileMode: FileDialog.OpenFile

        onAccepted: {
            // 发送图片消息
            chatWindow.sendImageMessage(groupImageFileDialog.file.toString());
        }
    }

    Connections {
        target: chatWindow
        function onMessageReceived(sender, content, timestamp, avatarSource) {
            // 仅当在一对一聊天中显示
            console.log("收到一对一消息信号，发送者:", sender, "，当前是否为群聊:", chatWindow.isGroupChat);
            if (!chatWindow.isGroupChat) {
                messageModel.append({
                    "sender": sender,
                    "content": content,
                    "timestamp": timestamp,
                    "avatarSource": avatarSource
                });
                console.log("添加一对一消息到列表");
            }
        }

        function onGroupChatMessageReceived(sender, content, timestamp, avatarSource) {
            console.log("收到群聊消息信号，发送者:", sender, "，当前是否为群聊:", chatWindow.isGroupChat);
            messageModel.append({
                "sender": sender,
                "content": content,
                "timestamp": timestamp,
                "avatarSource": avatarSource
            });
            console.log("添加群聊消息到列表");
        }

        function onChatDisplayCleared() {
            messageModel.clear();
            console.log("清空消息列表");
        }

        function onGroupCreated(groupName) {
            if (createGroupDialog.visible) {
                createGroupDialog.close();
            }
        }

        function onImageDownloaded(imageId, localPath) {
            console.log("群聊页面：图片下载完成，ID:", imageId, "本地路径:", localPath);
            // 不再需要刷新整个列表，我们会通过imageMessageUpdated信号更新特定消息
        }

        function onUpdatePlaceholderMessages(imageId, jsonContent) {
            console.log("群聊页面：更新图片占位符消息，ID:", imageId, "内容:", jsonContent);

            // 查找并更新所有包含"[图片加载中...]"的消息
            for (var i = 0; i < messageModel.count; i++) {
                var content = messageModel.get(i).content;

                // 检查是否是占位符消息
                if (content === "[图片加载中...]") {
                    console.log("群聊页面：找到图片占位符消息，索引:", i);

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
            chatDisplay.forceLayout();
        }

        function onImageMessageUpdated(imageId, jsonContent) {
            console.log("群聊页面：更新图片消息，ID:", imageId, "内容:", jsonContent);

            // 查找并更新包含该图片ID的消息
            for (var i = 0; i < messageModel.count; i++) {
                var content = messageModel.get(i).content;

                // 检查消息内容是否包含该图片ID
                if (content.indexOf(imageId) !== -1) {
                    console.log("群聊页面：找到包含图片ID的消息，索引:", i);

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
                        console.log("群聊页面：找到包含图片ID的JSON消息，索引:", i);

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
            chatDisplay.forceLayout();
        }
    }

    Component.onCompleted: {
        // 不在这里连接信号，因为已经在Connections中处理了
    }

    Component.onDestruction: {
        // 不需要断开连接，因为未手动连接
    }

    // 添加重试计时器，用于在第一次请求未收到响应时重试
    Timer {
        id: retryTimer
        interval: 1000 // 1秒后重试
        repeat: false
        property string groupId: ""

        onTriggered: {
            console.log("重试加载群聊信息，群ID:", groupId);
            if (messageModel.count === 1 && messageModel.get(0).sender === "系统") {
                // 如果当前只有系统消息，说明之前的请求可能失败了，重新请求
                chatWindow.selectGroup(groupId);
            }
        }
    }
}