#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QSqlDatabase>
#include "../Common/messageprotocol.h"

class Server : public QObject {
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    ~Server();
    void start();

private slots:
    void handleNewConnection();
    void handleClientData();
    void handleClientDisconnection();

private:
    struct ClientInfo {
        QTcpSocket *socket;
        bool isLoggedIn = false;
        QString nickname;
    };

    QTcpServer *tcpServer;
    QList<ClientInfo> clients;
    QSqlDatabase db;

    bool initDatabase();
    QString hashPassword(const QString &password);
    bool registerUser(const QString &email, const QString &nickname, const QString &password);
    QString loginUser(const QString &nickname, const QString &password, ClientInfo &client);
    bool addFriend(const QString &user, const QString &friendName);
    bool sendFriendRequest(const QString &from, const QString &to);
    bool acceptFriendRequest(const QString &from, const QString &to);
    bool deleteFriend(const QString &user, const QString &friendName);
    QStringList getFriendList(const QString &user);
    QStringList getFriendRequests(const QString &user);
    bool saveChatMessage(const QString &from, const QString &to, const QString &content);
    QJsonArray getChatHistory(const QString &user1, const QString &user2);
    QString searchUser(const QString &query);
    void updateUserStatus(const QString &nickname, bool isOnline);
    void handleLogout(ClientInfo *clientInfo);
    void notifyFriendsStatusChange(const QString &nickname, bool isOnline);
    bool notifyFriendRequest(const QString &to, const QString &from);
    bool deleteFriendRequest(const QString &from, const QString &to);
    
    // 用户个人信息相关函数
    QJsonObject getUserProfile(const QString &nickname);
    bool updateUserProfile(const QString &nickname, const QJsonObject &profileData);
    bool saveAvatar(const QString &nickname, const QByteArray &avatarData);
    QByteArray getAvatar(const QString &nickname);
    
    // 群聊相关函数
    bool createGroup(const QString &creator, const QString &groupName, const QStringList &members);
    QStringList getGroupList(const QString &user);
    QStringList getGroupMembers(int groupId);
    bool saveGroupChatMessage(int groupId, const QString &from, const QString &content);
    QJsonArray getGroupChatHistory(int groupId);
    bool notifyGroupMessage(int groupId, const QString &from, const QString &content);
    bool notifyGroupCreation(const QString &member, int groupId, const QString &groupName, const QString &creator);
};

#endif