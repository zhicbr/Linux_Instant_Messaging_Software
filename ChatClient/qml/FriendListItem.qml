import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    
    property string friendName: ""
    property bool selected: false
    property bool isRequest: false
    property bool isOnline: chatWindow.isFriendOnline(friendName)
    
    signal clicked()
    signal acceptClicked()
    signal deleteClicked()
    
    height: 40
    width: parent.width
    color: selected ? 
        (appSettings.darkTheme ? "#3B4252" : "#e0e0e0") : 
        (mainMouseArea.containsMouse ? 
            (appSettings.darkTheme ? "#313244" : "#f5f5f5") : 
            theme.inputBackgroundColor)
    
    // 主区域鼠标事件
    MouseArea {
        id: mainMouseArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {
            if (!isRequest) {
                root.clicked()
            }
        }
    }

    // 内容布局
    RowLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 10
        
        // 在线状态指示器
        Rectangle {
            width: 8
            height: 8
            radius: 4
            visible: !isRequest
            color: isOnline ? 
                (appSettings.darkTheme ? "#A6E3A1" : "#4CAF50") : 
                (appSettings.darkTheme ? "#A6ADC8" : "#9E9E9E")
            Layout.alignment: Qt.AlignVCenter
        }
        
        // 好友名称
        Text {
            text: friendName + (isOnline && !isRequest ? " (在线)" : "")
            font.pixelSize: 14
            color: theme.primaryTextColor
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        
        // 好友请求按钮区域
        Item {
            visible: isRequest
            Layout.preferredWidth: 130
            Layout.preferredHeight: 30
            
            // 接受按钮
            Button {
                id: acceptButton
                width: 60
                height: 30
                anchors.left: parent.left
                text: "接受"
                
                background: Rectangle {
                    color: acceptButton.pressed ? 
                        (appSettings.darkTheme ? "#8BD5CA" : "#1976D2") : 
                        (appSettings.darkTheme ? "#94E2D5" : "#2196F3")
                    radius: 4
                }
                
                contentItem: Text {
                    text: acceptButton.text
                    color: appSettings.darkTheme ? "#1E1E2E" : "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    console.log("接受按钮点击事件触发: " + root.friendName)
                    root.acceptClicked()
                }
            }
            
            // 删除按钮
            Button {
                id: rejectButton
                width: 60
                height: 30
                anchors.right: parent.right
                text: "删除"
                
                background: Rectangle {
                    color: rejectButton.pressed ? 
                        (appSettings.darkTheme ? "#D37089" : "#D32F2F") : 
                        (appSettings.darkTheme ? "#F38BA8" : "#f44336")
                    radius: 4
                }
                
                contentItem: Text {
                    text: rejectButton.text
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    console.log("删除按钮点击事件触发: " + root.friendName)
                    root.deleteClicked()
                }
            }
        }
    }

    // 监听在线状态变化
    Connections {
        target: chatWindow
        function onFriendOnlineStatusChanged() {
            isOnline = chatWindow.isFriendOnline(friendName)
        }
    }
}