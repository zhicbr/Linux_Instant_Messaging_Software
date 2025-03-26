#include "server.h"
#include <QJsonDocument>
#include <QDebug>
#include <QCoreApplication>
#include <QSqlQuery>
#include <QSqlError>
#include <QCryptographicHash>
#include <QDir>
#include "../Common/config.h"

Server::Server(QObject *parent) : QObject(parent) {
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &Server::handleNewConnection);
    if (!initDatabase()) {
        qDebug() << "Failed to initialize database";
        QCoreApplication::quit();
    }
}

Server::~Server() {
    db.close(); // 关闭数据库
}

void Server::start() {
    if (tcpServer->listen(QHostAddress::Any, Config::DefaultPort)) {
        qDebug() << "Server started, listening on port" << Config::DefaultPort;
    } else {
        qDebug() << "Failed to start server:" << tcpServer->errorString();
        QCoreApplication::quit();
    }
}

void Server::handleNewConnection() {
    QTcpSocket *clientSocket = tcpServer->nextPendingConnection();
    qDebug() << "New client connected from" << clientSocket->peerAddress().toString() << ":" << clientSocket->peerPort();
    ClientInfo clientInfo;
    clientInfo.socket = clientSocket;
    clientInfo.isLoggedIn = false;
    clients.append(clientInfo);
    connect(clientSocket, &QTcpSocket::readyRead, this, &Server::handleClientData);
}

void Server::handleClientData() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    // 查找对应的 ClientInfo
    ClientInfo *clientInfo = nullptr;
    for (ClientInfo &info : clients) {
        if (info.socket == clientSocket) {
            clientInfo = &info;
            break;
        }
    }
    if (!clientInfo) return;

    QByteArray data = clientSocket->readAll();
    qDebug() << "Received data from" << clientSocket->peerAddress().toString() << ":" << clientSocket->peerPort() << ":" << data;
    MessageType type;
    QJsonObject msgData;
    if (MessageProtocol::parseMessage(data, type, msgData)) {
        if (type == MessageType::Register) { // 处理注册
            QString email = msgData["email"].toString();
            QString nickname = msgData["nickname"].toString();
            QString password = msgData["password"].toString();
            qDebug() << "Register request: email=" << email << ", nickname=" << nickname;
            if (email.isEmpty() || nickname.isEmpty() || password.isEmpty()) {
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Register, {{"status", "failed"}, {"reason", "Empty email, nickname, or password"}})).toJson());
                return;
            }
            if (registerUser(email, nickname, password)) {
                qDebug() << "Registration successful for" << nickname;
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Register, {{"status", "success"}})).toJson());
            } else {
                qDebug() << "Registration failed for" << nickname;
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Register, {{"status", "failed"}, {"reason", "Email or nickname already exists"}})).toJson());
            }
        } else if (type == MessageType::Login) { // 处理登录
            QString nickname = msgData["nickname"].toString();
            QString password = msgData["password"].toString();
            qDebug() << "Login request: nickname=" << nickname;
            if (nickname.isEmpty() || password.isEmpty()) {
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Login, {{"status", "failed"}, {"reason", "Empty nickname or password"}})).toJson());
                return;
            }
            QString loginResult = loginUser(nickname, password, *clientInfo);
            if (loginResult == "success") {
                qDebug() << "Login successful for" << nickname;
                clientInfo->isLoggedIn = true;
                clientInfo->nickname = nickname;
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Login, {{"status", "success"}})).toJson());
            } else {
                qDebug() << "Login failed for" << nickname << ":" << loginResult;
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Login, {{"status", "failed"}, {"reason", loginResult}})).toJson());
            }
        } else if (type == MessageType::Chat) { // 处理聊天消息
            if (!clientInfo->isLoggedIn) {
                qDebug() << "Client not logged in, rejecting chat message";
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Error, {{"reason", "Please login first"}})).toJson());
                return;
            }
            qDebug() << "Broadcasting chat message from" << clientInfo->nickname << ":" << msgData;
            // 添加昵称到消息中
            QJsonObject broadcastData;
            broadcastData["content"] = clientInfo->nickname + ": " + msgData["content"].toString();
            QByteArray broadcastMessage = QJsonDocument(MessageProtocol::createMessage(MessageType::Chat, broadcastData)).toJson();
            for (ClientInfo &c : clients) {
                c.socket->write(broadcastMessage); // 广播消息
            }
        }
    } else {
        qDebug() << "Failed to parse message:" << data;
    }
}

bool Server::initDatabase() {
    // 使用绝对路径，确保数据库文件创建在 ChatServer 目录下
    QString dbPath = QCoreApplication::applicationDirPath() + "/../users.db";
    qDebug() << "Database path:" << dbPath;
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qDebug() << "Error: Failed to open database:" << db.lastError().text();
        return false;
    }

    // 创建用户表
    QSqlQuery query;
    QString createTableQuery = R"(
        CREATE TABLE IF NOT EXISTS users (
            email TEXT PRIMARY KEY,
            nickname TEXT UNIQUE,
            password TEXT NOT NULL
        )
    )";
    if (!query.exec(createTableQuery)) {
        qDebug() << "Error: Failed to create table:" << query.lastError().text();
        return false;
    }
    return true;
}

QString Server::hashPassword(const QString &password) {
    // 使用 SHA-256 哈希，加盐
    QString salt = "my_salt"; // 实际项目中应为每个用户生成唯一的盐
    QString saltedPassword = password + salt;
    return QString(QCryptographicHash::hash(saltedPassword.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool Server::registerUser(const QString &email, const QString &nickname, const QString &password) {
    QSqlQuery query;
    // 检查邮箱和昵称是否已存在
    query.prepare("SELECT email, nickname FROM users WHERE email = :email OR nickname = :nickname");
    query.bindValue(":email", email);
    query.bindValue(":nickname", nickname);
    if (!query.exec()) {
        qDebug() << "Error: Failed to check user existence:" << query.lastError().text();
        return false;
    }
    if (query.next()) {
        qDebug() << "User already exists: email=" << email << ", nickname=" << nickname;
        return false; // 邮箱或昵称已存在
    }

    // 插入新用户
    query.prepare("INSERT INTO users (email, nickname, password) VALUES (:email, :nickname, :password)");
    query.bindValue(":email", email);
    query.bindValue(":nickname", nickname);
    query.bindValue(":password", hashPassword(password));
    if (!query.exec()) {
        qDebug() << "Error: Failed to register user:" << query.lastError().text();
        return false;
    }
    qDebug() << "User registered: email=" << email << ", nickname=" << nickname;
    return true;
}

QString Server::loginUser(const QString &nickname, const QString &password, ClientInfo &client) {
    QSqlQuery query;
    query.prepare("SELECT password FROM users WHERE nickname = :nickname");
    query.bindValue(":nickname", nickname);
    if (!query.exec()) {
        qDebug() << "Error: Failed to query user:" << query.lastError().text();
        return "Database error";
    }
    if (query.next()) {
        QString storedPassword = query.value("password").toString();
        if (storedPassword == hashPassword(password)) {
            return "success"; // 登录成功
        } else {
            return "Invalid password"; // 密码错误
        }
    }
    return "User not found"; // 用户不存在
}
