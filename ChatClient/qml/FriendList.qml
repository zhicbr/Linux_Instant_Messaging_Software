import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root
    color: "#f0f0f0"
    
    property var friendModel: []
    property var requestModel: []
    property string selectedFriend: ""
    
    signal friendSelected(string friendName)
    signal acceptRequest(string friendName)
    signal deleteRequest(string friendName)
    signal deleteFriend(string friendName)

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        // 好友请求部分
        Rectangle {
            id: requestSection
            visible: root.requestModel.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: requestHeader.height + requestListView.height + 10
            color: "transparent"
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 5
                
                Text {
                    id: requestHeader
                    text: "好友请求 (" + root.requestModel.length + ")"
                    font.pixelSize: 16
                    font.bold: true
                    Layout.leftMargin: 10
                    Layout.topMargin: 5
                }

                ListView {
                    id: requestListView
                    Layout.fillWidth: true
                    Layout.preferredHeight: root.requestModel.length * 40
                    Layout.leftMargin: 5
                    Layout.rightMargin: 5
                    interactive: true
                    clip: true
                    model: root.requestModel
                    
                    delegate: FriendListItem {
                        id: requestItem
                        width: requestListView.width
                        height: 40
                        friendName: modelData
                        isRequest: true
                        
                        Component.onCompleted: {
                            console.log("好友请求项创建: " + friendName)
                        }
                        
                        onAcceptClicked: {
                            console.log("FriendList: 接受好友请求 - " + friendName)
                            root.acceptRequest(friendName)
                        }
                        
                        onDeleteClicked: {
                            console.log("FriendList: 删除好友请求 - " + friendName)
                            root.deleteRequest(friendName)
                        }
                    }
                }
            }
        }

        // 好友列表部分
        Rectangle {
            id: friendSection
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 5
                
                Text {
                    id: friendHeader
                    text: "好友列表"
                    font.pixelSize: 16
                    font.bold: true
                    Layout.leftMargin: 10
                    Layout.topMargin: 5
                }

                ListView {
                    id: friendListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 5
                    Layout.rightMargin: 5
                    interactive: true
                    clip: true
                    model: root.friendModel
                    
                    delegate: FriendListItem {
                        id: friendItem
                        width: friendListView.width
                        height: 40
                        friendName: modelData
                        selected: root.selectedFriend === friendName
                        isRequest: false
                        
                        Component.onCompleted: {
                            console.log("好友项创建: " + friendName)
                        }
                        
                        onClicked: {
                            console.log("选择好友: " + friendName)
                            root.selectedFriend = friendName
                            root.friendSelected(friendName)
                        }
                    }
                }
            }
        }
    }
    
    // 监听模型变化
    onFriendModelChanged: {
        console.log("好友模型已更新: " + JSON.stringify(friendModel))
    }
    
    onRequestModelChanged: {
        console.log("好友请求模型已更新: " + JSON.stringify(requestModel))
    }
} 