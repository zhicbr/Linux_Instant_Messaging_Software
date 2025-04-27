#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QMutex>
#include "../Common/messageprotocol.h"
#include "threadpool.h"
#include "semaphore.h"
#include "filelock.h"
#include "readwritelock.h"
#include "threadmessagequeue.h"
#include "sharedmemory.h"
#include "processmanager.h"

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

    // 在主线程中发送响应给客户端
    void sendResponseToClient(QTcpSocket *clientSocket, const QByteArray &response);

private:
    // 处理客户端数据的线程函数
    void processClientData(QTcpSocket *clientSocket, const QByteArray &data);

    // 为当前线程创建数据库连接
    QSqlDatabase getThreadLocalDatabase();

private:
    struct ClientInfo {
        QTcpSocket *socket;
        bool isLoggedIn = false;
        QString nickname;
    };

    QTcpServer *tcpServer;
    QList<ClientInfo> clients;
    QMutex m_clientsMutex;  // 保护clients列表的互斥锁
    QSqlDatabase db;

    // 图片存储路径
    QString m_imageStoragePath;

    // 线程池
    ThreadPool *m_threadPool;

    // 数据库访问信号量
    Semaphore *m_dbSemaphore;

    // 数据库文件锁
    FileLock *m_dbFileLock;

    // 聊天历史读写锁
    ReadWriteLock *m_chatHistoryLock;

    // 消息队列
    ThreadMessageQueue *m_messageQueue;

    // 共享内存
    SharedMemory *m_sharedMemory;

    // 进程管理器
    ProcessManager *m_processManager;

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

    // 图片处理相关函数
    bool saveImage(const QString &imageId, const QByteArray &imageData);
    QByteArray getImage(const QString &imageId);
    QString generateUniqueImageId(const QString &fileExtension);

    // 分块图片上传相关函数
    void handleChunkedImageStart(QTcpSocket *clientSocket, const QJsonObject &msgData, ClientInfo *clientInfo);
    void handleChunkedImageChunk(QTcpSocket *clientSocket, const QJsonObject &msgData, ClientInfo *clientInfo);
    void handleChunkedImageEnd(QTcpSocket *clientSocket, const QJsonObject &msgData, ClientInfo *clientInfo);

    // 临时存储分块上传的图片数据
    struct ChunkedImageData {
        QString tempId;
        QString fileExtension;
        int totalChunks;
        int receivedChunks;
        QByteArray imageData;
        int width;
        int height;
    };
    QMap<QString, ChunkedImageData> m_pendingChunkedImages;
};

#endif