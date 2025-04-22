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
                        text: "返回"
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        background: Rectangle {
                            color: parent.pressed ? "#5A6268" : "#6C757D"
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
                        onClicked: {
                            createGroupDialog.open();
                        }
                    }
                }

                // 群聊列表标题
                Rectangle {
                    Layout.fillWidth: true
                    height: 30
                    color: "#DEE2E6"
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        
                        Text {
                            text: "我的群聊"
                            font.bold: true
                            font.pixelSize: 14
                        }
                        
                        Item { Layout.fillWidth: true }
                        
                        Button {
                            text: "刷新"
                            Layout.preferredWidth: 60
                            Layout.preferredHeight: 24
                            font.pixelSize: 12
                            background: Rectangle {
                                color: parent.pressed ? "#5A6268" : "#6C757D"
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
                        color: chatWindow.currentChatGroup === modelData.split(':')[0] ? "#CED4DA" : "#F8F9FA"
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 10
                            
                            Rectangle {
                                Layout.preferredWidth: 34
                                Layout.preferredHeight: 34
                                radius: 17
                                color: "#28A745"
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.split(':')[1].charAt(0).toUpperCase()
                                    font.pixelSize: 16
                                    font.bold: true
                                    color: "white"
                                }
                            }
                            
                            Text {
                                Layout.fillWidth: true
                                text: modelData.split(':')[1]
                                font.pixelSize: 14
                                elide: Text.ElideRight
                            }
                        }
                        
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                let groupId = modelData.split(':')[0];
                                chatWindow.selectGroup(groupId);
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
                color: "#E9ECEF"
                border.color: "#DEE2E6"
                
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
                        id: messageInput
                        Layout.fillWidth: true
                        placeholderText: "输入消息..."
                        font.pixelSize: 14
                        enabled: chatWindow.isGroupChat
                        background: Rectangle {
                            border.color: messageInput.activeFocus ? "#80BDFF" : "#CED4DA"
                            border.width: 1
                            radius: 4
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
                            color: parent.enabled ? (parent.pressed ? "#0056b3" : "#007BFF") : "#B0B0B0"
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
            color: "#E9ECEF"
            border.color: "#DEE2E6"
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
                        color: "#F8F9FA"
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 5
                            spacing: 10
                            
                            Rectangle {
                                Layout.preferredWidth: 30
                                Layout.preferredHeight: 30
                                radius: 15
                                color: chatWindow.isFriendOnline(modelData) ? "#28A745" : "#6C757D"
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.charAt(0).toUpperCase()
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: "white"
                                }
                            }
                            
                            Text {
                                Layout.fillWidth: true
                                text: modelData
                                font.pixelSize: 13
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
    Dialog {
        id: createGroupDialog
        title: "创建新群聊"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 400
        
        anchors.centerIn: parent
        
        onAccepted: {
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
            } else {
                chatWindow.setStatusMessage("请输入群聊名称并选择至少一个成员");
            }
        }
        
        onRejected: {
            groupNameField.text = "";
            // 重置选中状态
            for (let i = 0; i < friendsCheckboxModel.count; i++) {
                friendsCheckboxModel.setProperty(i, "checked", false);
            }
        }
        
        contentItem: ColumnLayout {
            spacing: 15
            
            RowLayout {
                Layout.fillWidth: true
                
                Label {
                    text: "群聊名称："
                    Layout.preferredWidth: 80
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
            }
            
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
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

    Connections {
        target: chatWindow
        function onMessageReceived(sender, content, timestamp) {
            // 仅当在一对一聊天中显示
            console.log("收到一对一消息信号，发送者:", sender, "，当前是否为群聊:", chatWindow.isGroupChat);
            if (!chatWindow.isGroupChat) {
                messageModel.append({
                    "sender": sender,
                    "content": content,
                    "timestamp": timestamp
                });
                console.log("添加一对一消息到列表");
            }
        }
        
        function onGroupChatMessageReceived(sender, content, timestamp) {
            // 仅当在群聊中显示
            console.log("收到群聊消息信号，发送者:", sender, "，当前是否为群聊:", chatWindow.isGroupChat);
            // 移除条件判断，确保消息总是显示
            messageModel.append({
                "sender": sender,
                "content": content,
                "timestamp": timestamp
            });
            console.log("添加群聊消息到列表");
        }
        
        function onChatDisplayCleared() {
            messageModel.clear();
            console.log("清空消息列表");
        }
        
        function onGroupCreated(groupName) {
            createGroupDialog.close();
        }
    }
} 