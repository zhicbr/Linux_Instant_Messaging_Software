import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.1

Rectangle {
    color: theme.backgroundColor

    property string avatarSource: "qrc:/images/default_avatar.png"
    property bool isEditing: false

    // 初始化加载用户资料
    Component.onCompleted: {
        if (chatWindow.isLoggedIn) {
            chatWindow.requestUserProfile();
            
            // 确保每次进入页面时都请求头像
            chatWindow.requestAvatar(chatWindow.currentNickname);
        }
    }

    // 监听用户资料变化
    Connections {
        target: chatWindow
        function onUserProfileChanged() {
            var profile = chatWindow.userProfile;
            
            nicknameField.text = profile.nickname || chatWindow.currentNickname;
            signatureField.text = profile.signature || "这个人很懒，什么都没留下";
            
            // 更新其他字段
            if (profile.gender) {
                for (var i = 0; i < genderComboBox.model.length; i++) {
                    if (genderComboBox.model[i] === profile.gender) {
                        genderComboBox.currentIndex = i;
                        break;
                    }
                }
            }
            
            birthdayField.text = profile.birthday || "";
            locationField.text = profile.location || "";
            phoneField.text = profile.phone || "";
            
            // 显示注册时间和上次登录时间
            registerTimeText.text = profile.register_time || "未知";
            lastLoginTimeText.text = profile.last_login_time || "未知";
            
            // 显示好友数和群组数
            friendCountText.text = profile.friend_count || "0";
            groupCountText.text = profile.group_count || "0";
            
            // 如果有头像信息，请求头像
            if (profile.avatar) {
                console.log("检测到用户有头像，再次请求头像");
                chatWindow.requestAvatar(chatWindow.currentNickname);
            }
        }
        
        function onAvatarReceived(nickname, localPath) {
            console.log("收到头像信号，用户:", nickname, "本地路径:", localPath);
            if (nickname === chatWindow.currentNickname) {
                avatarSource = "file:///" + localPath;
                console.log("更新头像源为:", avatarSource);
            }
        }
        
        function onProfileUpdateSuccess() {
            isEditing = false;
        }
        
        function onAvatarUploadSuccess() {
            // 不需要额外处理，头像会通过 onAvatarReceived 更新
            console.log("头像上传成功，等待接收新头像");
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: "返回"
                font.pixelSize: 14
                background: Rectangle {
                    color: theme.primaryButtonColor
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
                onClicked: stackView.pop()
            }

            Text {
                text: "个人资料"
                font.pixelSize: 20
                font.bold: true
                color: theme.primaryTextColor
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            Button {
                text: isEditing ? "保存" : "编辑"
                font.pixelSize: 14
                background: Rectangle {
                    color: isEditing ? theme.successButtonColor : theme.secondaryButtonColor
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
                    if (isEditing) {
                        // 保存个人资料
                        chatWindow.updateUserProfile(
                            nicknameField.text,
                            signatureField.text,
                            genderComboBox.currentText,
                            birthdayField.text,
                            locationField.text,
                            phoneField.text
                        );
                    } else {
                        isEditing = true;
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: theme.borderColor
        }

        // 头像和基本信息
        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            // 头像区域
            Rectangle {
                width: 120
                height: 120
                color: "transparent"

                Rectangle {
                    id: avatarMask
                    anchors.fill: parent
                    radius: width / 2
                    color: "white"
                    clip: true
                    
                    Image {
                        id: avatarImage
                        anchors.fill: parent
                        source: avatarSource
                        fillMode: Image.PreserveAspectCrop
                    }
                    
                    // 显示默认背景色，当图像加载失败时
                    Rectangle {
                        anchors.fill: parent
                        color: theme.borderColor
                        visible: avatarImage.status === Image.Error || avatarImage.status === Image.Null
                        
                        Text {
                            anchors.centerIn: parent
                            text: chatWindow.currentNickname ? chatWindow.currentNickname.charAt(0).toUpperCase() : "?"
                            font.pixelSize: parent.width * 0.5
                            color: theme.backgroundColor
                        }
                    }
                }

                Button {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: 36
                    height: 36
                    visible: isEditing
                    background: Rectangle {
                        color: theme.primaryButtonColor
                        radius: width / 2
                    }
                    contentItem: Text {
                        text: "✎"
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: 16
                    }
                    onClicked: fileDialog.open()
                }
            }

            // 基本信息
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "昵称:"
                        font.pixelSize: 16
                        color: theme.primaryTextColor
                        width: 80
                    }

                    TextField {
                        id: nicknameField
                        Layout.fillWidth: true
                        text: chatWindow.currentNickname
                        readOnly: true // 昵称不可修改
                        background: Rectangle {
                            color: "transparent"
                            border.color: "transparent"
                            border.width: 0
                            radius: 4
                        }
                        color: theme.primaryTextColor
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "个性签名:"
                        font.pixelSize: 16
                        color: theme.primaryTextColor
                        width: 80
                    }

                    TextField {
                        id: signatureField
                        Layout.fillWidth: true
                        text: "这个人很懒，什么都没留下"
                        readOnly: !isEditing
                        background: Rectangle {
                            color: isEditing ? theme.inputBackgroundColor : "transparent"
                            border.color: isEditing ? theme.inputBorderColor : "transparent"
                            border.width: isEditing ? 1 : 0
                            radius: 4
                        }
                        color: theme.primaryTextColor
                    }
                }
            }
        }

        // 详细信息
        GroupBox {
            title: "详细资料"
            Layout.fillWidth: true
            Layout.preferredHeight: 250
            background: Rectangle {
                color: "transparent"
                border.color: theme.borderColor
                border.width: 1
                radius: 6
            }
            label: Label {
                text: parent.title
                color: theme.primaryTextColor
                font.pixelSize: 16
                font.bold: true
                padding: 10
            }

            GridLayout {
                anchors.fill: parent
                anchors.margins: 20
                columns: 2
                rowSpacing: 15
                columnSpacing: 10

                Text {
                    text: "性别:"
                    font.pixelSize: 14
                    color: theme.primaryTextColor
                }

                ComboBox {
                    id: genderComboBox
                    Layout.fillWidth: true
                    model: ["未设置", "男", "女", "其他"]
                    enabled: isEditing
                    background: Rectangle {
                        color: isEditing ? theme.inputBackgroundColor : "transparent"
                        border.color: isEditing ? theme.inputBorderColor : "transparent"
                        border.width: isEditing ? 1 : 0
                        radius: 4
                    }
                    contentItem: Text {
                        text: genderComboBox.displayText
                        color: theme.primaryTextColor
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                    }
                }

                Text {
                    text: "生日:"
                    font.pixelSize: 14
                    color: theme.primaryTextColor
                }

                TextField {
                    id: birthdayField
                    Layout.fillWidth: true
                    placeholderText: "YYYY-MM-DD"
                    readOnly: !isEditing
                    background: Rectangle {
                        color: isEditing ? theme.inputBackgroundColor : "transparent"
                        border.color: isEditing ? theme.inputBorderColor : "transparent"
                        border.width: isEditing ? 1 : 0
                        radius: 4
                    }
                    color: theme.primaryTextColor
                }

                Text {
                    text: "所在地:"
                    font.pixelSize: 14
                    color: theme.primaryTextColor
                }

                TextField {
                    id: locationField
                    Layout.fillWidth: true
                    readOnly: !isEditing
                    background: Rectangle {
                        color: isEditing ? theme.inputBackgroundColor : "transparent"
                        border.color: isEditing ? theme.inputBorderColor : "transparent"
                        border.width: isEditing ? 1 : 0
                        radius: 4
                    }
                    color: theme.primaryTextColor
                }

                Text {
                    text: "手机号:"
                    font.pixelSize: 14
                    color: theme.primaryTextColor
                }

                TextField {
                    id: phoneField
                    Layout.fillWidth: true
                    readOnly: !isEditing
                    background: Rectangle {
                        color: isEditing ? theme.inputBackgroundColor : "transparent"
                        border.color: isEditing ? theme.inputBorderColor : "transparent"
                        border.width: isEditing ? 1 : 0
                        radius: 4
                    }
                    color: theme.primaryTextColor
                }
            }
        }

        // 账号信息
        GroupBox {
            title: "账号信息"
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            background: Rectangle {
                color: "transparent"
                border.color: theme.borderColor
                border.width: 1
                radius: 6
            }
            label: Label {
                text: parent.title
                color: theme.primaryTextColor
                font.pixelSize: 16
                font.bold: true
                padding: 10
            }

            GridLayout {
                anchors.fill: parent
                anchors.margins: 20
                columns: 2
                rowSpacing: 15
                columnSpacing: 10

                Text {
                    text: "注册时间:"
                    font.pixelSize: 14
                    color: theme.primaryTextColor
                }

                Text {
                    id: registerTimeText
                    Layout.fillWidth: true
                    font.pixelSize: 14
                    color: theme.secondaryTextColor
                    text: "未知"
                }

                Text {
                    text: "上次登录:"
                    font.pixelSize: 14
                    color: theme.primaryTextColor
                }

                Text {
                    id: lastLoginTimeText
                    Layout.fillWidth: true
                    font.pixelSize: 14
                    color: theme.secondaryTextColor
                    text: "未知"
                }

                Text {
                    text: "好友数量:"
                    font.pixelSize: 14
                    color: theme.primaryTextColor
                }

                Text {
                    id: friendCountText
                    Layout.fillWidth: true
                    font.pixelSize: 14
                    color: theme.secondaryTextColor
                    text: "0"
                }

                Text {
                    text: "群组数量:"
                    font.pixelSize: 14
                    color: theme.primaryTextColor
                }

                Text {
                    id: groupCountText
                    Layout.fillWidth: true
                    font.pixelSize: 14
                    color: theme.secondaryTextColor
                    text: "0"
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }

    FileDialog {
        id: fileDialog
        title: "选择头像"
        folder: StandardPaths.writableLocation(StandardPaths.PicturesLocation)
        nameFilters: ["图片文件 (*.jpg *.jpeg *.png)"]
        onAccepted: {
            // 更新本地显示的头像（临时）
            avatarSource = fileDialog.file
            chatWindow.uploadAvatar(fileDialog.file)
        }
    }
}