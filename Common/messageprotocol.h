#ifndef MESSAGEPROTOCOL_H
#define MESSAGEPROTOCOL_H

#include <QJsonObject>
#include <QString>

// 定义消息类型
enum MessageType {
    // 用户登录注册消息
    Register = 1,       // 注册
    Login = 2,          // 登录
    Logout = 3,         // 注销
    Message = 4,        // 聊天消息
    ChatHistory = 5,    // 聊天历史
    FriendList = 6,     // 好友列表

    // 用户搜索和好友请求
    SearchUser = 7,     // 搜索用户
    SearchResult = 8,   // 搜索结果
    AddFriend = 9,      // 添加好友请求
    AcceptFriend = 10,  // 接受好友请求
    FriendStatus = 11,  // 好友状态变更
    DeleteFriend = 12,  // 删除好友
    FriendRequest = 13, // 好友请求通知
    FriendRequestList = 14, // 好友请求列表
    DeleteFriendRequest = 15, // 删除好友请求

    // 群聊相关消息类型
    CreateGroup = 16,   // 创建群聊
    GroupList = 17,     // 获取群聊列表
    GroupMembers = 18,  // 获取群成员列表
    GroupChat = 19,     // 群聊消息
    GroupChatHistory = 20, // 群聊历史记录

    // 用户个人信息相关消息类型
    GetUserProfile = 21,    // 获取用户个人资料
    UpdateUserProfile = 22, // 更新用户个人资料
    UploadAvatar = 23,      // 上传头像
    GetAvatar = 24,         // 获取头像

    // 图片消息相关类型
    UploadImageRequest = 25,   // C->S: 请求上传图片 (含 Base64 数据)
    UploadImageResponse = 26,  // S->C: 图片上传结果 (返回 imageId)
    DownloadImageRequest = 27, // C->S: 请求下载图片 (根据 imageId)
    DownloadImageResponse = 28, // S->C: 返回图片数据 (Base64)

    // 分块图片传输相关类型
    ChunkedImageStart = 29,    // C->S: 开始分块图片传输 (包含总块数、文件信息)
    ChunkedImageChunk = 30,    // C->S: 图片数据块 (包含块索引和数据)
    ChunkedImageEnd = 31,      // C->S: 结束分块图片传输 (确认所有块已发送)
    ChunkedImageResponse = 32, // S->C: 分块图片传输响应 (成功/失败)

    // 二进制图片数据传输
    BinaryImageData = 33       // S->C: 二进制图片数据 (不使用JSON)
};

class MessageProtocol {
public:
    static QJsonObject createMessage(MessageType type, const QJsonObject &data);
    static bool parseMessage(const QByteArray &data, MessageType &type, QJsonObject &msgData);

    // 获取消息类型的字符串表示
    static QString messageTypeToString(MessageType type) {
        switch (type) {
            case MessageType::Login: return "Login";
            case MessageType::Message: return "Message";
            case MessageType::Register: return "Register";
            case MessageType::SearchUser: return "SearchUser";
            case MessageType::AddFriend: return "AddFriend";
            case MessageType::FriendList: return "FriendList";
            case MessageType::ChatHistory: return "ChatHistory";
            case MessageType::Logout: return "Logout";
            case MessageType::FriendStatus: return "FriendStatus";
            case MessageType::FriendRequest: return "FriendRequest";
            case MessageType::AcceptFriend: return "AcceptFriend";
            case MessageType::DeleteFriend: return "DeleteFriend";
            case MessageType::DeleteFriendRequest: return "DeleteFriendRequest";
            case MessageType::FriendRequestList: return "FriendRequestList";
            case MessageType::CreateGroup: return "CreateGroup";
            case MessageType::GroupList: return "GroupList";
            case MessageType::GroupMembers: return "GroupMembers";
            case MessageType::GroupChat: return "GroupChat";
            case MessageType::GroupChatHistory: return "GroupChatHistory";
            case MessageType::GetUserProfile: return "GetUserProfile";
            case MessageType::UpdateUserProfile: return "UpdateUserProfile";
            case MessageType::UploadAvatar: return "UploadAvatar";
            case MessageType::GetAvatar: return "GetAvatar";
            case MessageType::UploadImageRequest: return "UploadImageRequest";
            case MessageType::UploadImageResponse: return "UploadImageResponse";
            case MessageType::DownloadImageRequest: return "DownloadImageRequest";
            case MessageType::DownloadImageResponse: return "DownloadImageResponse";
            case MessageType::ChunkedImageStart: return "ChunkedImageStart";
            case MessageType::ChunkedImageChunk: return "ChunkedImageChunk";
            case MessageType::ChunkedImageEnd: return "ChunkedImageEnd";
            case MessageType::ChunkedImageResponse: return "ChunkedImageResponse";
            case MessageType::BinaryImageData: return "BinaryImageData";
            default: return "Unknown";
        }
    }
};

#endif
