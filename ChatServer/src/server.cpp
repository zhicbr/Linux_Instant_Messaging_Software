#include "server.h"
#include <QJsonDocument>
#include <QJsonArray> // 添加这个头文件以使用 QJsonArray
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
    db.close();
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
        switch (type) {
        case MessageType::Register: {
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
            break;
        }
        case MessageType::Login: {
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
            break;
        }
        case MessageType::Chat:
            if (!clientInfo->isLoggedIn) {
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Error, {{"reason", "Please login first"}})).toJson());
                return;
            }
            {
                QString to = msgData["to"].toString();
                QString content = msgData["content"].toString();
                saveChatMessage(clientInfo->nickname, to, content);
                QJsonObject privateMsg;
                privateMsg["from"] = clientInfo->nickname;
                privateMsg["content"] = content;
                QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::Chat, privateMsg)).toJson();
                for (ClientInfo &c : clients) {
                    if (c.isLoggedIn && c.nickname == to) {
                        c.socket->write(message);
                    }
                }
            }
            break;
        case MessageType::SearchUser:
            {
                QString query = msgData["query"].toString();
                QString nickname = searchUser(query);
                if (!nickname.isEmpty()) {
                    clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::SearchUser, {{"status", "success"}, {"nickname", nickname}})).toJson());
                } else {
                    clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::SearchUser, {{"status", "failed"}})).toJson());
                }
            }
            break;
        case MessageType::AddFriend:
            if (!clientInfo->isLoggedIn) {
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Error, {{"reason", "Please login first"}})).toJson());
                return;
            }
            {
                QString friendName = msgData["friend"].toString();
                if (addFriend(clientInfo->nickname, friendName)) {
                    clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::AddFriend, {{"status", "success"}})).toJson());
                } else {
                    clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::AddFriend, {{"status", "failed"}, {"reason", "Friend not found or already added"}})).toJson());
                }
            }
            break;
        case MessageType::GetFriendList:
            if (!clientInfo->isLoggedIn) {
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Error, {{"reason", "Please login first"}})).toJson());
                return;
            }
            {
                QStringList friends = getFriendList(clientInfo->nickname);
                QJsonArray friendArray;
                for (const QString &f : friends) {
                    friendArray.append(f);
                }
                QJsonObject response;
                response["status"] = "success";
                response["friends"] = friendArray;
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::GetFriendList, response)).toJson());
            }
            break;
        case MessageType::GetChatHistory:
            if (!clientInfo->isLoggedIn) {
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Error, {{"reason", "Please login first"}})).toJson());
                return;
            }
            {
                QString friendName = msgData["friend"].toString(); // 修复变量名，避免使用 C++ 关键字 "friend"
                QJsonObject response;
                response["status"] = "success";
                response["messages"] = getChatHistory(clientInfo->nickname, friendName);
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::GetChatHistory, response)).toJson());
            }
            break;
        case MessageType::Error:
            break; // 忽略客户端发送的错误消息
        }
    } else {
        qDebug() << "Failed to parse message:" << data;
    }
}

bool Server::initDatabase() {
    QString dbPath = QCoreApplication::applicationDirPath() + "/../users.db";
    qDebug() << "Database path:" << dbPath;
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qDebug() << "Error: Failed to open database:" << db.lastError().text();
        return false;
    }

    QSqlQuery query;
    QString createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            email TEXT PRIMARY KEY,
            nickname TEXT UNIQUE,
            password TEXT NOT NULL
        )
    )";
    if (!query.exec(createUsersTable)) {
        qDebug() << "Error: Failed to create users table:" << query.lastError().text();
        return false;
    }

    QString createFriendsTable = R"(
        CREATE TABLE IF NOT EXISTS friends (
            user_nickname TEXT,
            friend_nickname TEXT,
            PRIMARY KEY (user_nickname, friend_nickname),
            FOREIGN KEY (user_nickname) REFERENCES users(nickname),
            FOREIGN KEY (friend_nickname) REFERENCES users(nickname)
        )
    )";
    if (!query.exec(createFriendsTable)) {
        qDebug() << "Error: Failed to create friends table:" << query.lastError().text();
        return false;
    }

    QString createMessagesTable = R"(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            from_nickname TEXT,
            to_nickname TEXT,
            content TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (from_nickname) REFERENCES users(nickname),
            FOREIGN KEY (to_nickname) REFERENCES users(nickname)
        )
    )";
    if (!query.exec(createMessagesTable)) {
        qDebug() << "Error: Failed to create messages table:" << query.lastError().text();
        return false;
    }
    return true;
}

QString Server::hashPassword(const QString &password) {
    QString salt = "my_salt";
    QString saltedPassword = password + salt;
    return QString(QCryptographicHash::hash(saltedPassword.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool Server::registerUser(const QString &email, const QString &nickname, const QString &password) {
    QSqlQuery query;
    query.prepare("SELECT email, nickname FROM users WHERE email = :email OR nickname = :nickname");
    query.bindValue(":email", email);
    query.bindValue(":nickname", nickname);
    if (!query.exec() || query.next()) {
        return false;
    }
    query.prepare("INSERT INTO users (email, nickname, password) VALUES (:email, :nickname, :password)");
    query.bindValue(":email", email);
    query.bindValue(":nickname", nickname);
    query.bindValue(":password", hashPassword(password));
    return query.exec();
}

QString Server::loginUser(const QString &nickname, const QString &password, ClientInfo &client) {
    QSqlQuery query;
    query.prepare("SELECT password FROM users WHERE nickname = :nickname");
    query.bindValue(":nickname", nickname);
    if (!query.exec() || !query.next()) {
        return "User not found";
    }
    QString storedPassword = query.value("password").toString();
    if (storedPassword == hashPassword(password)) {
        return "success";
    }
    return "Invalid password";
}

bool Server::addFriend(const QString &user, const QString &friendName) {
    QSqlQuery query;
    query.prepare("SELECT nickname FROM users WHERE nickname = :friend");
    query.bindValue(":friend", friendName);
    if (!query.exec() || !query.next()) {
        return false;
    }
    query.prepare("INSERT OR IGNORE INTO friends (user_nickname, friend_nickname) VALUES (:user, :friend)");
    query.bindValue(":user", user);
    query.bindValue(":friend", friendName);
    return query.exec();
}

QStringList Server::getFriendList(const QString &user) {
    QStringList friends;
    QSqlQuery query;
    query.prepare("SELECT friend_nickname FROM friends WHERE user_nickname = :user");
    query.bindValue(":user", user);
    if (query.exec()) {
        while (query.next()) {
            friends << query.value("friend_nickname").toString();
        }
    }
    return friends;
}

bool Server::saveChatMessage(const QString &from, const QString &to, const QString &content) {
    QSqlQuery query;
    query.prepare("INSERT INTO messages (from_nickname, to_nickname, content) VALUES (:from, :to, :content)");
    query.bindValue(":from", from);
    query.bindValue(":to", to);
    query.bindValue(":content", content);
    return query.exec();
}

QJsonArray Server::getChatHistory(const QString &user1, const QString &user2) {
    QJsonArray messages;
    QSqlQuery query;
    query.prepare("SELECT from_nickname, content FROM messages WHERE (from_nickname = :user1 AND to_nickname = :user2) OR (from_nickname = :user2 AND to_nickname = :user1) ORDER BY timestamp");
    query.bindValue(":user1", user1);
    query.bindValue(":user2", user2);
    if (query.exec()) {
        while (query.next()) {
            QJsonObject msg;
            msg["from"] = query.value("from_nickname").toString();
            msg["content"] = query.value("content").toString();
            messages.append(msg);
        }
    }
    return messages;
}

QString Server::searchUser(const QString &query) {
    QSqlQuery sqlQuery;
    sqlQuery.prepare("SELECT nickname FROM users WHERE nickname = :query OR email = :query");
    sqlQuery.bindValue(":query", query);
    if (sqlQuery.exec() && sqlQuery.next()) {
        return sqlQuery.value("nickname").toString();
    }
    return QString();
}