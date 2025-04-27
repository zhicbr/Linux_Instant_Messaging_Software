#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonArray>
#include <QStringList>
#include <QMap>
#include <QBuffer>
#include <QTimer>
#include <QImage>
#include <QFileInfo>
#include "../Common/messageprotocol.h"

class ChatWindow : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn WRITE setIsLoggedIn NOTIFY isLoggedInChanged)
    Q_PROPERTY(QString currentNickname READ currentNickname NOTIFY currentNicknameChanged)
    Q_PROPERTY(QString currentChatFriend READ currentChatFriend NOTIFY currentChatFriendChanged)
    Q_PROPERTY(QStringList friendList READ getFriendList NOTIFY friendListUpdated)
    Q_PROPERTY(QStringList friendRequests READ getFriendRequests NOTIFY friendRequestsChanged)
    Q_PROPERTY(QVariantMap friendOnlineStatus READ getFriendOnlineStatus NOTIFY friendOnlineStatusChanged)
    Q_PROPERTY(QStringList groupList READ getGroupList NOTIFY groupListUpdated)
    Q_PROPERTY(QStringList groupMembers READ getGroupMembers NOTIFY groupMembersUpdated)
    Q_PROPERTY(QString currentChatGroup READ currentChatGroup NOTIFY currentChatGroupChanged)
    Q_PROPERTY(bool isGroupChat READ isGroupChat NOTIFY isGroupChatChanged)
    Q_PROPERTY(QVariantMap userProfile READ getUserProfile NOTIFY userProfileChanged)

public:
    explicit ChatWindow(QObject *parent = nullptr);
    ~ChatWindow();

    bool isLoggedIn() const { return m_isLoggedIn; }
    void setIsLoggedIn(bool loggedIn);
    QString currentNickname() const { return m_currentNickname; }
    QString currentChatFriend() const { return m_currentChatFriend; }
    QStringList getFriendList() const { return m_friendList; }
    QStringList getFriendRequests() const { return m_friendRequests; }
    QVariantMap getFriendOnlineStatus() const { return m_friendOnlineStatus; }
    QStringList getGroupList() const { return m_groupList; }
    QStringList getGroupMembers() const { return m_currentGroupMembers; }
    QString currentChatGroup() const { return m_currentChatGroup; }
    bool isGroupChat() const { return m_isGroupChat; }
    QVariantMap getUserProfile() const { return m_userProfile; }

    // QML 可调用的方法
    Q_INVOKABLE void login(const QString &nickname, const QString &password);
    Q_INVOKABLE void registerUser(const QString &email, const QString &nickname, const QString &password);
    Q_INVOKABLE void sendMessage(const QString &content);
    Q_INVOKABLE void selectFriend(const QString &friendName);
    Q_INVOKABLE void searchUser(const QString &query);
    Q_INVOKABLE void sendFriendRequest(const QString &friendName);
    Q_INVOKABLE void acceptFriendRequest(const QString &friendName);
    Q_INVOKABLE void deleteFriendRequest(const QString &friendName);
    Q_INVOKABLE void deleteFriend(const QString &friendName);
    Q_INVOKABLE void logout();
    Q_INVOKABLE bool isFriendOnline(const QString &friendName) const;

    // 群聊相关方法
    Q_INVOKABLE void createGroup(const QString &groupName, const QStringList &members);
    Q_INVOKABLE void refreshGroupList();
    Q_INVOKABLE void selectGroup(const QString &groupId);
    Q_INVOKABLE void sendGroupMessage(const QString &content);
    Q_INVOKABLE void getGroupMembers(const QString &groupId);
    Q_INVOKABLE QString getGroupName(const QString &groupId) const;
    Q_INVOKABLE void clearChatType();

    // 更新 QML 界面的方法
    Q_INVOKABLE void appendMessage(const QString &sender, const QString &content, const QString &timestamp);
    Q_INVOKABLE void clearChatDisplay();
    Q_INVOKABLE void setStatusMessage(const QString &message);
    Q_INVOKABLE void updateFriendList(const QStringList &friends);
    Q_INVOKABLE void updateFriendRequests(const QStringList &requests);
    Q_INVOKABLE void updateGroupList(const QStringList &groups);
    Q_INVOKABLE void updateGroupMembers(const QStringList &members);

    // 添加用户资料相关方法
    Q_INVOKABLE void requestUserProfile();
    Q_INVOKABLE void updateUserProfile(const QString &nickname, const QString &signature,
                                     const QString &gender, const QString &birthday,
                                     const QString &location, const QString &phone);
    Q_INVOKABLE void uploadAvatar(const QString &filePath);
    Q_INVOKABLE void requestAvatar(const QString &nickname);
    Q_INVOKABLE QString getCachedAvatarPath(const QString &nickname) const;

    // 图片消息相关方法
    Q_INVOKABLE void sendImageMessage(const QString &localFilePath);
    Q_INVOKABLE QString getCachedImagePath(const QString &imageId) const;
    Q_INVOKABLE QString getImageCachePath(const QString &imageId) const;

signals:
    void isLoggedInChanged();
    void currentNicknameChanged();
    void currentChatFriendChanged();
    signals:
        // 修改消息接收信号，添加头像参数
        void messageReceived(const QString &sender, const QString &content, const QString &timestamp, const QString &avatarSource);
        void refreshChatMessages();
    void statusMessage(const QString &message);
    void friendListUpdated(const QStringList &friends);
    void friendRequestsChanged();
    void chatDisplayCleared();
    void friendRequestReceived(const QString &from);
    void friendRequestAccepted(const QString &friendName);
    void friendDeleted(const QString &friendName);
    void friendOnlineStatusChanged();
    void groupListUpdated();
    void groupMembersUpdated();
    void currentChatGroupChanged();
    void isGroupChatChanged();
    void groupCreated(const QString &groupName);
    void groupChatMessageReceived(const QString &sender, const QString &content, const QString &timestamp, const QString &avatarSource);
    void userProfileChanged();
    void avatarReceived(const QString &nickname, const QString &localPath);
    void profileUpdateSuccess();
    void avatarUploadSuccess();

    // 图片相关信号
    void imageDownloaded(const QString &imageId, const QString &localPath);
    void imageMessageUpdated(const QString &imageId, const QString &jsonContent);
    void updatePlaceholderMessages(const QString &imageId, const QString &jsonContent);
    void imageUploadSuccess(const QString &imageId);

private slots:
    void handleServerData();
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);

    // 添加新的消息处理函数
    void handleAcceptFriendMessage(const QJsonObject &msgData);
    void handleDeleteFriendRequestMessage(const QJsonObject &msgData);
    void handleGroupChatMessage(const QJsonObject &msgData);
    void handleCreateGroupMessage(const QJsonObject &msgData);

    // 图片相关消息处理函数
    void handleUploadImageResponse(const QJsonObject &msgData);
    void handleDownloadImageResponse(const QJsonObject &msgData);

private:
    QTcpSocket *m_socket;
    bool m_isLoggedIn = false;
    QString m_currentNickname;
    QString m_currentChatFriend;
    QStringList m_friendList;
    QStringList m_friendRequests;
    QVariantMap m_friendOnlineStatus;

    // 群聊相关属性
    QStringList m_groupList;             // 格式: "groupId:groupName"
    QMap<QString, QString> m_groupNames; // groupId -> groupName
    QStringList m_currentGroupMembers;
    QString m_currentChatGroup;
    bool m_isGroupChat = false;

    QVariantMap m_userProfile;

    // 头像缓存
    QMap<QString, QString> m_avatarCache; // nickname -> local file path

    // 图片缓存
    QString m_imageCachePath; // 图片缓存目录
    QMap<QString, QString> m_imageCacheMap; // imageId -> local file path

    // 图片上传临时数据
    struct ImageUploadData {
        QString localFilePath;
        int width;
        int height;
        QString fileExtension;
        QByteArray imageData;  // 用于分块上传
        int totalChunks;       // 总块数
        int currentChunk;      // 当前块索引
        int chunkSize;         // 块大小
        bool isChunked;        // 是否使用分块上传
    };
    QMap<QString, ImageUploadData> m_pendingImageUploads; // 临时存储上传中的图片信息

    void loadChatHistory(const QString &friendName);
    void refreshFriendRequests();
    void updateFriendOnlineStatus(const QString &friendName, bool isOnline);
    void loadGroupChatHistory(const QString &groupId);

    // 图片处理相关私有方法
    void initImageCache();
    void saveImageCacheSettings();
    void downloadImage(const QString &imageId);
    void updateImageMessages(const QString &imageId, const QString &localPath);
    void sendMessageInternal(const QJsonObject &contentJson);
    void sendGroupMessageInternal(const QJsonObject &contentJson);

    // 分块图片上传相关方法
    void sendImageChunked(const QString &tempId);
    void sendNextImageChunk(const QString &tempId);
    void handleChunkedImageResponse(const QJsonObject &msgData);
};

#endif // CHATWINDOW_H