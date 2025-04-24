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
                implicitHeight: bubble.height
                
                Rectangle {
                    id: bubble
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
                            ctx.fillStyle = bubble.color
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
