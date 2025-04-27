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

    // 图片消息相关属性
    property bool isImageMessage: false
    property string imageSource: ""
    property int imageWidth: 200
    property int imageHeight: 150

    // 在组件创建时检查是否为图片消息
    Component.onCompleted: {
        // 尝试解析JSON内容
        try {
            // 检查内容是否已经是对象（发送方）或者是JSON字符串（接收方）
            var contentObj;
            if (typeof content === 'object') {
                // 内容已经是对象
                contentObj = content;
            } else if (typeof content === 'string' && content.trim() !== '') {
                // 尝试解析JSON字符串
                try {
                    contentObj = JSON.parse(content);
                } catch (parseError) {
                    console.log("JSON解析失败，作为普通文本处理:", parseError);
                    // 解析失败，当作普通文本处理
                    return;
                }
            } else {
                // 空内容或其他类型，当作普通文本处理
                return;
            }
            
            if (contentObj && contentObj.type === "image") {
                isImageMessage = true;
                // 使用imageId作为标识，localPath可能只在发送方存在
                if (contentObj.localPath) {
                    imageSource = contentObj.localPath;
                } else if (contentObj.imageId) {
                    // 对于接收方，可能需要从缓存或服务器获取图片
                    // 这里假设已经下载到本地缓存，路径格式与发送方相同
                    var cachePath = chatWindow.getImageCachePath(contentObj.imageId);
                    if (cachePath) {
                        imageSource = cachePath;
                    } else {
                        // 如果没有缓存路径，可能需要触发下载
                        console.log("需要下载图片:", contentObj.imageId);
                        // 这里可以调用C++方法下载图片
                        // chatWindow.downloadImage(contentObj.imageId);
                    }
                }
                
                if (contentObj.width && contentObj.height) {
                    // 保持宽高比，但限制最大尺寸
                    var aspectRatio = contentObj.width / contentObj.height;
                    var maxWidth = layout.width * 0.6;
                    var maxHeight = 300;

                    if (contentObj.width > maxWidth) {
                        imageWidth = maxWidth;
                        imageHeight = imageWidth / aspectRatio;
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
        } catch (e) {
            // 不是有效的JSON，当作普通文本处理
            console.log("解析消息内容失败:", e);
            isImageMessage = false;
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
                color: theme.borderColor
                visible: avatarImage.status === Image.Error || avatarImage.status === Image.Null

                Text {
                    anchors.centerIn: parent
                    text: sender ? sender.charAt(0).toUpperCase() : "?"
                    font.pixelSize: parent.width * 0.5
                    color: theme.backgroundColor
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
                implicitHeight: isImageMessage ? imageBubble.height : textBubble.height

                // 文本消息气泡
                Rectangle {
                    id: textBubble
                    visible: !isImageMessage
                    width: Math.min(contentText.implicitWidth + 16, layout.width * 0.7)
                    height: contentText.implicitHeight + 16
                    radius: 12
                    color: isOwnMessage ? "#40E0D0" : "#FFF"
                    border.color: isOwnMessage ? "#40E0D0" : "#E0E0E0"

                    anchors.right: isOwnMessage ? parent.right : undefined
                    anchors.left: isOwnMessage ? undefined : parent.left

                    Text {
                        id: contentText
                        width: parent.width - 16
                        anchors.centerIn: parent
                        text: content
                        wrapMode: Text.Wrap
                        font.pixelSize: 14
                        color: "#000"
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
                            ctx.fillStyle = textBubble.color
                            ctx.fill()
                        }
                    }
                }

                // 图片消息气泡
                Rectangle {
                    id: imageBubble
                    visible: isImageMessage
                    width: imageContainer.width + 12
                    height: imageContainer.height + 12
                    radius: 12
                    color: isOwnMessage ? "#40E0D0" : "#FFF"
                    border.color: isOwnMessage ? "#40E0D0" : "#E0E0E0"

                    anchors.right: isOwnMessage ? parent.right : undefined
                    anchors.left: isOwnMessage ? undefined : parent.left

                    Item {
                        id: imageContainer
                        width: 200
                        height: 150
                        anchors.centerIn: parent

                        // 在组件完成后检查图片尺寸
                        Component.onCompleted: {
                            if (imageWidth > 0 && imageHeight > 0) {
                                // 如果有指定尺寸，使用指定尺寸
                                width = imageWidth > 300 ? 300 : imageWidth;
                                height = imageHeight > 300 ? 300 : imageHeight;
                            }
                        }

                        Image {
                            id: messageImage
                            anchors.fill: parent
                            // 使用file://前缀确保正确加载本地文件
                            source: {
                                // 检查路径是否为空
                                if (!imageSource || imageSource === "") {
                                    return "";
                                }

                                // 检查路径是否已经有file://前缀
                                if (imageSource.startsWith("file://")) {
                                    return imageSource;
                                }
                                // 检查是否是本地文件路径
                                else if (imageSource.startsWith("/")) {
                                    return "file://" + imageSource;
                                }
                                // 其他情况直接使用原始路径
                                else {
                                    return imageSource;
                                }
                            }
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            cache: false // 禁用缓存，确保每次都重新加载图片

                            // 调试输出
                            Component.onCompleted: {
                                console.log("图片路径:", source);

                                // 如果图片路径为空，尝试从父组件获取
                                if (source === "") {
                                    try {
                                        var parentContent;
                                        if (typeof content === 'object') {
                                            // 内容已经是对象
                                            parentContent = content;
                                        } else if (typeof content === 'string' && content.trim() !== '') {
                                            // 尝试解析JSON字符串
                                            try {
                                                parentContent = JSON.parse(content);
                                            } catch (parseError) {
                                                console.log("JSON解析失败，无法获取图片信息:", parseError);
                                                return;
                                            }
                                        } else {
                                            // 空内容或其他类型
                                            return;
                                        }
                                        
                                        if (parentContent && parentContent.type === "image") {
                                            var path = "";
                                            if (parentContent.localPath) {
                                                path = parentContent.localPath;
                                            } else if (parentContent.imageId) {
                                                // 尝试从缓存获取图片路径
                                                path = chatWindow.getImageCachePath(parentContent.imageId);
                                            }
                                            
                                            if (path) {
                                                if (path.startsWith("file://")) {
                                                    source = path;
                                                } else if (path.startsWith("/")) {
                                                    source = "file://" + path;
                                                } else {
                                                    source = path;
                                                }
                                                console.log("从父组件获取图片路径:", source);
                                            }
                                        }
                                    } catch (e) {
                                        console.log("解析图片内容失败:", e);
                                    }
                                }
                            }

                            onStatusChanged: {
                                if (status === Image.Error) {
                                    console.log("图片加载失败:", source);
                                } else if (status === Image.Ready) {
                                    console.log("图片加载成功:", source);

                                    // 尝试从JSON内容中获取尺寸
                                    var jsonWidth = 0;
                                    var jsonHeight = 0;

                                    try {
                                        var contentObj;
                                        if (typeof content === 'string') {
                                            contentObj = JSON.parse(content);
                                        } else {
                                            contentObj = content;
                                        }
                                        
                                        if (contentObj && contentObj.type === "image") {
                                            if (contentObj.width > 0) jsonWidth = contentObj.width;
                                            if (contentObj.height > 0) jsonHeight = contentObj.height;
                                        }
                                    } catch (e) {
                                        console.log("解析图片尺寸失败:", e);
                                        // 不是JSON，继续处理
                                    }

                                    // 图片加载成功后，更新容器大小
                                    if (imageWidth > 0 && imageHeight > 0) {
                                        // 使用属性中指定的尺寸
                                        var maxWidth = 300;
                                        var maxHeight = 300;

                                        if (imageWidth > maxWidth || imageHeight > maxHeight) {
                                            var aspectRatio = imageWidth / imageHeight;
                                            if (aspectRatio > 1) {
                                                // 宽图
                                                imageContainer.width = maxWidth;
                                                imageContainer.height = maxWidth / aspectRatio;
                                            } else {
                                                // 高图
                                                imageContainer.height = maxHeight;
                                                imageContainer.width = maxHeight * aspectRatio;
                                            }
                                        } else {
                                            imageContainer.width = imageWidth;
                                            imageContainer.height = imageHeight;
                                        }
                                    } else if (jsonWidth > 0 && jsonHeight > 0) {
                                        // 使用JSON中的尺寸
                                        var maxWidth = 300;
                                        var maxHeight = 300;

                                        if (jsonWidth > maxWidth || jsonHeight > maxHeight) {
                                            var aspectRatio = jsonWidth / jsonHeight;
                                            if (aspectRatio > 1) {
                                                // 宽图
                                                imageContainer.width = maxWidth;
                                                imageContainer.height = maxWidth / aspectRatio;
                                            } else {
                                                // 高图
                                                imageContainer.height = maxHeight;
                                                imageContainer.width = maxHeight * aspectRatio;
                                            }
                                        } else {
                                            imageContainer.width = jsonWidth;
                                            imageContainer.height = jsonHeight;
                                        }
                                    } else {
                                        // 如果没有指定尺寸，使用图片的实际尺寸，但限制最大尺寸
                                        var maxWidth = 300;
                                        var maxHeight = 300;
                                        var aspectRatio = sourceSize.width / sourceSize.height;

                                        if (sourceSize.width > 0 && sourceSize.height > 0) {
                                            if (sourceSize.width > maxWidth || sourceSize.height > maxHeight) {
                                                if (aspectRatio > 1) {
                                                    // 宽图
                                                    imageContainer.width = maxWidth;
                                                    imageContainer.height = maxWidth / aspectRatio;
                                                } else {
                                                    // 高图
                                                    imageContainer.height = maxHeight;
                                                    imageContainer.width = maxHeight * aspectRatio;
                                                }
                                            } else {
                                                imageContainer.width = sourceSize.width;
                                                imageContainer.height = sourceSize.height;
                                            }
                                        }
                                    }
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
                                        console.log("点击查看图片:", imageSource);
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
