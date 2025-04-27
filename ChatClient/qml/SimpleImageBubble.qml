import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: root
    width: parent.width
    height: layout.height + 10

    property string sender: ""
    property string content: ""
    property string timestamp: ""
    property string avatarSource: ""
    property bool isOwnMessage: false
    property string imagePath: ""
    property int imageWidth: 300
    property int imageHeight: 200

    // 在组件创建时解析图片路径
    Component.onCompleted: {
        console.log("SimpleImageBubble 创建，内容:", content);
        
        try {
            // 尝试解析JSON内容
            if (content && content.trim().startsWith("{") && content.trim().endsWith("}")) {
                var contentObj = JSON.parse(content);
                if (contentObj && contentObj.type === "image") {
                    // 直接使用本地路径，无论是发送方还是接收方
                    var path = contentObj.localPath;
                    console.log("图片路径:", path);
                    
                    // 检查路径是否已经有file://前缀
                    if (path.startsWith("file://")) {
                        imagePath = path;
                    }
                    // 检查是否是本地文件路径
                    else if (path.startsWith("/")) {
                        imagePath = "file://" + path;
                    }
                    // 其他情况直接使用原始路径
                    else {
                        imagePath = path;
                    }
                    
                    console.log("最终图片路径:", imagePath);
                    
                    // 设置图片尺寸
                    if (contentObj.width && contentObj.height) {
                        var aspectRatio = contentObj.width / contentObj.height;
                        var maxWidth = 300;
                        var maxHeight = 300;
                        
                        if (contentObj.width > maxWidth) {
                            imageWidth = maxWidth;
                            imageHeight = maxWidth / aspectRatio;
                        } else {
                            imageWidth = contentObj.width;
                            imageHeight = contentObj.height;
                        }
                        
                        if (imageHeight > maxHeight) {
                            imageHeight = maxHeight;
                            imageWidth = imageHeight * aspectRatio;
                        }
                    }
                }
            }
        } catch (e) {
            console.log("解析图片内容失败:", e);
        }
    }

    RowLayout {
        id: layout
        width: parent.width - 20
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 0
        layoutDirection: isOwnMessage ? Qt.RightToLeft : Qt.LeftToRight

        Rectangle {
            id: avatar
            width: 40
            height: 40
            radius: 20
            color: "#e0e0e0"
            Layout.alignment: Qt.AlignTop
            clip: true

            Image {
                id: avatarImage
                anchors.fill: parent
                source: avatarSource
                fillMode: Image.PreserveAspectCrop
                layer.enabled: true
                layer.smooth: true
                onStatusChanged: {
                    if (status === Image.Error) {
                        console.log("头像图片加载失败:", avatarSource);
                    } else if (status === Image.Ready) {
                        console.log("头像图片加载成功:", avatarSource);
                    }
                }
            }

            // 显示默认背景色和用户首字母，当图像加载失败时
            Rectangle {
                anchors.fill: parent
                color: "#cccccc"
                visible: avatarImage.status === Image.Error || avatarImage.status === Image.Null

                Text {
                    anchors.centerIn: parent
                    text: sender ? sender.charAt(0).toUpperCase() : "?"
                    font.pixelSize: parent.width * 0.5
                    color: "#ffffff"
                }
            }
        }

        ColumnLayout {
            id: messageColumn
            spacing: 2
            Layout.maximumWidth: parent.width * 0.7
            Layout.alignment: Qt.AlignTop

            Text {
                text: sender
                font.pixelSize: 12
                color: "#666"
                visible: !isOwnMessage
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignLeft
            }

            Item {
                Layout.fillWidth: true
                implicitHeight: imageBubble.height

                // 图片消息气泡
                Rectangle {
                    id: imageBubble
                    width: imageContainer.width + 12
                    height: imageContainer.height + 12
                    radius: 12
                    color: isOwnMessage ? "#40E0D0" : "#FFF"
                    border.color: isOwnMessage ? "#40E0D0" : "#E0E0E0"

                    anchors.right: isOwnMessage ? parent.right : undefined
                    anchors.left: isOwnMessage ? undefined : parent.left

                    Item {
                        id: imageContainer
                        width: imageWidth
                        height: imageHeight
                        anchors.centerIn: parent

                        Image {
                            id: messageImage
                            anchors.fill: parent
                            source: imagePath
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            cache: false // 禁用缓存，确保每次都重新加载图片

                            onStatusChanged: {
                                if (status === Image.Error) {
                                    console.log("图片加载失败:", source);
                                } else if (status === Image.Ready) {
                                    console.log("图片加载成功:", source);
                                }
                            }

                            // 加载状态指示器
                            BusyIndicator {
                                anchors.centerIn: parent
                                running: messageImage.status === Image.Loading
                                visible: running
                            }

                            // 加载失败提示
                            Rectangle {
                                anchors.fill: parent
                                color: "#f0f0f0"
                                visible: messageImage.status === Image.Error

                                Text {
                                    anchors.centerIn: parent
                                    text: "图片加载失败"
                                    color: "#666"
                                }
                            }

                            // 点击放大查看
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    if (messageImage.status === Image.Ready) {
                                        // 这里可以实现点击放大查看的功能
                                        console.log("点击查看图片:", imagePath);
                                    }
                                }
                            }
                        }
                    }

                    Canvas {
                        width: 8
                        height: 12
                        anchors {
                            verticalCenter: parent.verticalCenter
                            right: isOwnMessage ? undefined : parent.left
                            left: isOwnMessage ? parent.right : undefined
                        }

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            ctx.beginPath()
                            if(isOwnMessage) {
                                ctx.moveTo(0, 0)
                                ctx.lineTo(width, height/2)
                                ctx.lineTo(0, height)
                            } else {
                                ctx.moveTo(width, 0)
                                ctx.lineTo(0, height/2)
                                ctx.lineTo(width, height)
                            }
                            ctx.closePath()
                            ctx.fillStyle = imageBubble.color
                            ctx.fill()
                        }
                    }
                }
            }

            Text {
                text: timestamp
                font.pixelSize: 10
                color: "#999"
                Layout.fillWidth: true
                horizontalAlignment: isOwnMessage ? Text.AlignRight : Text.AlignLeft
            }
        }
    }
}
