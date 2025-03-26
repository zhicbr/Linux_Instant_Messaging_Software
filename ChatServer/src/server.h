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

private:
    struct ClientInfo {
        QTcpSocket *socket;
        bool isLoggedIn = false;
        QString nickname;
    };

    QTcpServer *tcpServer;
    QList<ClientInfo> clients; // 存储客户端信息
    QSqlDatabase db; // SQLite 数据库

    bool initDatabase(); // 初始化数据库
    QString hashPassword(const QString &password); // 密码加密
    bool registerUser(const QString &email, const QString &nickname, const QString &password); // 注册用户
    QString loginUser(const QString &nickname, const QString &password, ClientInfo &client); // 登录用户，返回错误信息
};

#endif
