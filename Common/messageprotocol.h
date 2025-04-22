#ifndef MESSAGEPROTOCOL_H
#define MESSAGEPROTOCOL_H

#include <QJsonObject>
#include <QString>

enum class MessageType {
    Login,
    Chat,
    Register,
    Error,
    SearchUser,
    AddFriend,
    GetFriendList,
    GetChatHistory,
    Logout,
    UserStatus,
    FriendRequest,      // 发送好友请求
    AcceptFriend,       // 接受好友请求
    DeleteFriend,       // 删除好友
    DeleteFriendRequest,// 删除好友请求
    GetFriendRequests   // 获取好友请求列表
};

class MessageProtocol {
public:
    static QJsonObject createMessage(MessageType type, const QJsonObject &data);
    static bool parseMessage(const QByteArray &data, MessageType &type, QJsonObject &msgData);
    
    // 获取消息类型的字符串表示
    static QString messageTypeToString(MessageType type) {
        switch (type) {
            case MessageType::Login: return "Login";
            case MessageType::Chat: return "Chat";
            case MessageType::Register: return "Register";
            case MessageType::Error: return "Error";
            case MessageType::SearchUser: return "SearchUser";
            case MessageType::AddFriend: return "AddFriend";
            case MessageType::GetFriendList: return "GetFriendList";
            case MessageType::GetChatHistory: return "GetChatHistory";
            case MessageType::Logout: return "Logout";
            case MessageType::UserStatus: return "UserStatus";
            case MessageType::FriendRequest: return "FriendRequest";
            case MessageType::AcceptFriend: return "AcceptFriend";
            case MessageType::DeleteFriend: return "DeleteFriend";
            case MessageType::DeleteFriendRequest: return "DeleteFriendRequest";
            case MessageType::GetFriendRequests: return "GetFriendRequests";
            default: return "Unknown";
        }
    }
};

#endif
