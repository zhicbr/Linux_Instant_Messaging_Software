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
#include <QThread>

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
    connect(clientSocket, &QTcpSocket::disconnected, this, &Server::handleClientDisconnection);
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
        qDebug() << "解析后的消息类型: " << MessageProtocol::messageTypeToString(type) << "(" << static_cast<int>(type) << "), 内容: " << msgData;
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
        case MessageType::Logout:
            if (!clientInfo->isLoggedIn) {
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Error, {{"reason", "Not logged in"}})).toJson());
                return;
            }
            handleLogout(clientInfo);
            break;
        case MessageType::Error:
            break; // 忽略客户端发送的错误消息
        case MessageType::FriendRequest:
            if (!clientInfo->isLoggedIn) {
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Error, {{"reason", "Please login first"}})).toJson());
                return;
            }
            {
                QString to = msgData["to"].toString();
                if (sendFriendRequest(clientInfo->nickname, to)) {
                    clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::FriendRequest, {{"status", "success"}})).toJson());
                } else {
                    clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::FriendRequest, {{"status", "failed"}, {"reason", "Request already sent or users are already friends"}})).toJson());
                }
            }
            break;
        case MessageType::AcceptFriend:
            qDebug() << "收到接受好友请求消息: " << MessageProtocol::messageTypeToString(type) << " - " << msgData;
            if (!clientInfo->isLoggedIn) {
                qDebug() << "未登录用户尝试接受好友请求";
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Error, {{"reason", "Please login first"}})).toJson());
                clientSocket->flush();
                return;
            }
            {
                QString from = msgData["from"].toString();
                qDebug() << "处理接受好友请求：" << from << "到" << clientInfo->nickname;
                if (acceptFriendRequest(from, clientInfo->nickname)) {
                    qDebug() << "接受好友请求成功，发送响应";
                    
                    // 发送成功响应给接受者
                    QJsonObject accepterResponse;
                    accepterResponse["status"] = "success";
                    QByteArray accepterMsg = QJsonDocument(MessageProtocol::createMessage(
                        MessageType::AcceptFriend, accepterResponse)).toJson();
                    clientSocket->write(accepterMsg);
                    clientSocket->flush();
                    qDebug() << "已发送响应给接受者：" << accepterMsg;
                    
                    // 等待一小段时间确保消息被处理
                    QThread::msleep(50);
                    
                    // 刷新接受者的好友列表
                    QStringList accepterFriends = getFriendList(clientInfo->nickname);
                    QJsonArray accepterFriendArray;
                    for (const QString &f : accepterFriends) {
                        accepterFriendArray.append(f);
                    }
                    QJsonObject friendsResponse;
                    friendsResponse["status"] = "success";
                    friendsResponse["friends"] = accepterFriendArray;
                    QByteArray friendsMsg = QJsonDocument(MessageProtocol::createMessage(
                        MessageType::GetFriendList, friendsResponse)).toJson();
                    clientSocket->write(friendsMsg);
                    clientSocket->flush();
                    qDebug() << "已发送好友列表给接受者：" << friendsResponse;
                    
                    // 等待一小段时间确保消息被处理
                    QThread::msleep(50);
                    
                    // 刷新接受者的好友请求列表
                    QStringList requests = getFriendRequests(clientInfo->nickname);
                    QJsonArray requestArray;
                    for (const QString &r : requests) {
                        requestArray.append(r);
                    }
                    QJsonObject refreshRequestsResponse;
                    refreshRequestsResponse["status"] = "success";
                    refreshRequestsResponse["requests"] = requestArray;
                    QByteArray refreshRequestsMsg = QJsonDocument(MessageProtocol::createMessage(
                        MessageType::GetFriendRequests, refreshRequestsResponse)).toJson();
                    clientSocket->write(refreshRequestsMsg);
                    clientSocket->flush();
                    qDebug() << "已发送好友请求列表给接受者：" << refreshRequestsResponse;
                    
                    // 通知发送请求的用户
                    bool notified = false;
                    for (const ClientInfo &client : clients) {
                        if (client.isLoggedIn && client.nickname == from) {
                            qDebug() << "找到请求发送者" << from << "，发送通知";
                            
                            // 发送接受通知
                            QJsonObject senderResponse;
                            senderResponse["status"] = "success";
                            senderResponse["friend"] = clientInfo->nickname;
                            QByteArray senderMsg = QJsonDocument(MessageProtocol::createMessage(
                                MessageType::AcceptFriend, senderResponse)).toJson();
                            client.socket->write(senderMsg);
                            client.socket->flush();
                            qDebug() << "已发送通知给请求发送者：" << senderMsg;
                            
                            // 等待一小段时间确保消息被处理
                            QThread::msleep(100);
                            
                            // 刷新发送者的好友列表
                            QStringList senderFriends = getFriendList(from);
                            QJsonArray senderFriendArray;
                            for (const QString &f : senderFriends) {
                                senderFriendArray.append(f);
                            }
                            QJsonObject senderFriendsResponse;
                            senderFriendsResponse["status"] = "success";
                            senderFriendsResponse["friends"] = senderFriendArray;
                            QByteArray senderFriendsMsg = QJsonDocument(MessageProtocol::createMessage(
                                MessageType::GetFriendList, senderFriendsResponse)).toJson();
                            client.socket->write(senderFriendsMsg);
                            client.socket->flush();
                            qDebug() << "已发送好友列表给请求发送者：" << senderFriendsResponse;
                            
                            notified = true;
                            break;
                        }
                    }
                    if (!notified) {
                        qDebug() << "请求发送者" << from << "不在线，无法通知";
                    }
                } else {
                    qDebug() << "接受好友请求失败：请求不存在或已处理";
                    QJsonObject errorResponse;
                    errorResponse["status"] = "failed";
                    errorResponse["reason"] = "Request not found or already processed";
                    QByteArray errorMsg = QJsonDocument(MessageProtocol::createMessage(
                        MessageType::AcceptFriend, errorResponse)).toJson();
                    clientSocket->write(errorMsg);
                    clientSocket->flush();
                    qDebug() << "已发送错误响应：" << errorMsg;
                }
            }
            break;
        case MessageType::DeleteFriend:
            if (!clientInfo->isLoggedIn) {
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Error, {{"reason", "Please login first"}})).toJson());
                return;
            }
            {
                QString friendName = msgData["friend"].toString();
                if (deleteFriend(clientInfo->nickname, friendName)) {
                    clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::DeleteFriend, {{"status", "success"}})).toJson());
                    // 通知被删除的好友
                    for (const ClientInfo &client : clients) {
                        if (client.isLoggedIn && client.nickname == friendName) {
                            client.socket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::DeleteFriend, {{"status", "success"}, {"friend", clientInfo->nickname}})).toJson());
                            break;
                        }
                    }
                } else {
                    clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::DeleteFriend, {{"status", "failed"}, {"reason", "Friend not found"}})).toJson());
                }
            }
            break;
        case MessageType::GetFriendRequests:
            if (!clientInfo->isLoggedIn) {
                qDebug() << "未登录用户尝试获取好友请求列表";
                clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Error, {{"reason", "Please login first"}})).toJson());
                return;
            }
            {
                qDebug() << "处理获取好友请求列表请求，用户：" << clientInfo->nickname;
                QStringList requests = getFriendRequests(clientInfo->nickname);
                QJsonArray requestArray;
                for (const QString &r : requests) {
                    requestArray.append(r);
                }
                QJsonObject response;
                response["status"] = "success";
                response["requests"] = requestArray;
                QByteArray responseMsg = QJsonDocument(MessageProtocol::createMessage(MessageType::GetFriendRequests, response)).toJson();
                qDebug() << "发送好友请求列表响应：" << response;
                clientSocket->write(responseMsg);
                clientSocket->flush();
            }
            break;
        case MessageType::DeleteFriendRequest: {
            qDebug() << "收到删除好友请求消息: " << MessageProtocol::messageTypeToString(type) << " - " << msgData;
            if (!clientInfo->isLoggedIn) {
                qDebug() << "未登录用户尝试删除好友请求";
                QJsonObject errorResponse;
                errorResponse["status"] = "failed";
                errorResponse["reason"] = "请先登录";
                QByteArray errorMsg = QJsonDocument(MessageProtocol::createMessage(
                    MessageType::DeleteFriendRequest, errorResponse)).toJson();
                clientInfo->socket->write(errorMsg);
                clientInfo->socket->flush();
                qDebug() << "已发送错误响应：" << errorMsg;
                break;
            }
            
            QString from = msgData["from"].toString();
            if (from.isEmpty()) {
                qDebug() << "无效的好友请求源用户";
                QJsonObject errorResponse;
                errorResponse["status"] = "failed";
                errorResponse["reason"] = "无效的好友请求";
                QByteArray errorMsg = QJsonDocument(MessageProtocol::createMessage(
                    MessageType::DeleteFriendRequest, errorResponse)).toJson();
                clientInfo->socket->write(errorMsg);
                clientInfo->socket->flush();
                qDebug() << "已发送错误响应：" << errorMsg;
                break;
            }
            
            qDebug() << "处理删除好友请求，从" << from << "到" << clientInfo->nickname;
            if (deleteFriendRequest(from, clientInfo->nickname)) {
                // 发送成功响应
                QJsonObject response;
                response["status"] = "success";
                QByteArray responseMsg = QJsonDocument(MessageProtocol::createMessage(
                    MessageType::DeleteFriendRequest, response)).toJson();
                clientInfo->socket->write(responseMsg);
                clientInfo->socket->flush();
                qDebug() << "已发送删除好友请求成功响应：" << responseMsg;
                
                // 刷新好友请求列表
                QStringList requests = getFriendRequests(clientInfo->nickname);
                QJsonArray requestArray;
                for (const QString &r : requests) {
                    requestArray.append(r);
                }
                QJsonObject refreshResponse;
                refreshResponse["status"] = "success";
                refreshResponse["requests"] = requestArray;
                QByteArray refreshMsg = QJsonDocument(MessageProtocol::createMessage(
                    MessageType::GetFriendRequests, refreshResponse)).toJson();
                clientInfo->socket->write(refreshMsg);
                clientInfo->socket->flush();
                qDebug() << "已发送刷新好友请求列表响应：" << refreshResponse;
            } else {
                // 发送失败响应
                QJsonObject errorResponse;
                errorResponse["status"] = "failed";
                errorResponse["reason"] = "删除好友请求失败";
                QByteArray errorMsg = QJsonDocument(MessageProtocol::createMessage(
                    MessageType::DeleteFriendRequest, errorResponse)).toJson();
                clientInfo->socket->write(errorMsg);
                clientInfo->socket->flush();
                qDebug() << "已发送错误响应：" << errorMsg;
            }
            break;
        }
        }
    } else {
        qDebug() << "Failed to parse message:" << data;
    }
}

void Server::handleClientDisconnection() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    for (int i = 0; i < clients.size(); ++i) {
        if (clients[i].socket == clientSocket) {
            if (clients[i].isLoggedIn) {
                handleLogout(&clients[i]);
            }
            clients.removeAt(i);
            break;
        }
    }
    clientSocket->deleteLater();
}

void Server::handleLogout(ClientInfo *clientInfo) {
    if (!clientInfo || !clientInfo->isLoggedIn) return;

    updateUserStatus(clientInfo->nickname, false);
    notifyFriendsStatusChange(clientInfo->nickname, false);
    
    // 发送登出成功消息给客户端
    QJsonObject response;
    response["status"] = "success";
    clientInfo->socket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Logout, response)).toJson());
    
    clientInfo->isLoggedIn = false;
    clientInfo->nickname.clear();
}

void Server::updateUserStatus(const QString &nickname, bool isOnline) {
    QSqlQuery query(db);
    query.prepare("UPDATE users SET is_online = ? WHERE nickname = ?");
    query.addBindValue(isOnline ? 1 : 0);
    query.addBindValue(nickname);
    if (!query.exec()) {
        qDebug() << "Error updating user status:" << query.lastError().text();
    }
}

void Server::notifyFriendsStatusChange(const QString &nickname, bool isOnline) {
    QStringList friends = getFriendList(nickname);
    QJsonObject statusMsg;
    statusMsg["friend"] = nickname;
    statusMsg["isOnline"] = isOnline;
    statusMsg["nickname"] = nickname;  // 添加nickname字段，确保客户端能正确识别
    QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::UserStatus, statusMsg)).toJson();
    
    for (const ClientInfo &client : clients) {
        if (client.isLoggedIn && friends.contains(client.nickname)) {
            client.socket->write(message);
        }
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

    // 不再删除旧表，保留数据
    // QSqlQuery dropQuery(db);
    // dropQuery.exec("DROP TABLE IF EXISTS users");
    // dropQuery.exec("DROP TABLE IF EXISTS friends");
    // dropQuery.exec("DROP TABLE IF EXISTS messages");
    // dropQuery.exec("DROP TABLE IF EXISTS friend_requests");

    QSqlQuery query;
    QString createUsersTable = R"(
        CREATE TABLE IF NOT EXISTS users (
            email TEXT PRIMARY KEY,
            nickname TEXT UNIQUE,
            password TEXT NOT NULL,
            is_online INTEGER DEFAULT 0
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

    QString createFriendRequestsTable = R"(
        CREATE TABLE IF NOT EXISTS friend_requests (
            from_nickname TEXT,
            to_nickname TEXT,
            status TEXT DEFAULT 'pending',
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (from_nickname, to_nickname),
            FOREIGN KEY (from_nickname) REFERENCES users(nickname),
            FOREIGN KEY (to_nickname) REFERENCES users(nickname)
        )
    )";
    if (!query.exec(createFriendRequestsTable)) {
        qDebug() << "Error: Failed to create friend_requests table:" << query.lastError().text();
        return false;
    }
    
    // 创建测试账号 (111-999)
    for (int i = 1; i <= 9; i++) {
        QString username = QString("%1%1%1").arg(i);
        QString email = QString("%1@test.com").arg(username);
        QString hashedPassword = hashPassword(username); // 密码与用户名相同
        
        // 使用INSERT OR IGNORE确保不会因为记录已存在而报错
        QString insertSql = QString("INSERT OR IGNORE INTO users (email, nickname, password, is_online) "
                                   "VALUES ('%1', '%2', '%3', 0)")
                            .arg(email)
                            .arg(username)
                            .arg(hashedPassword);
        
        if (query.exec(insertSql)) {
            // 检查是否真的插入了新记录
            if (query.numRowsAffected() > 0) {
                qDebug() << "已创建测试账号:" << username;
            }
        } else {
            qDebug() << "创建测试账号失败:" << username << "- 错误:" << query.lastError().text();
        }
    }
    
    return true;
}

QString Server::hashPassword(const QString &password) {
    QString salt = "my_salt";
    QString saltedPassword = password + salt;
    return QString(QCryptographicHash::hash(saltedPassword.toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool Server::registerUser(const QString &email, const QString &nickname, const QString &password) {
    QSqlQuery query(db);
    query.prepare("SELECT email, nickname FROM users WHERE email = ? OR nickname = ?");
    query.addBindValue(email);
    query.addBindValue(nickname);
    if (!query.exec() || query.next()) {
        return false;
    }
    query.prepare("INSERT INTO users (email, nickname, password, is_online) VALUES (?, ?, ?, 0)");
    query.addBindValue(email);
    query.addBindValue(nickname);
    query.addBindValue(hashPassword(password));
    return query.exec();
}

QString Server::loginUser(const QString &nickname, const QString &password, ClientInfo &client) {
    QSqlQuery query(db);
    query.prepare("SELECT password FROM users WHERE nickname = ?");
    query.addBindValue(nickname);
    if (!query.exec() || !query.next()) {
        return "User not found";
    }
    QString storedPassword = query.value("password").toString();
    if (storedPassword == hashPassword(password)) {
        updateUserStatus(nickname, true);
        notifyFriendsStatusChange(nickname, true);
        
        // 登录时检查是否有待处理的好友请求，但这里不发送给客户端
        // 客户端登录成功后会主动请求好友请求列表
        QStringList pendingRequests = getFriendRequests(nickname);
        if (!pendingRequests.isEmpty()) {
            qDebug() << "用户" << nickname << "登录成功，发现" << pendingRequests.size() << "个待处理的好友请求";
        }
        
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
    QSqlQuery sqlQuery(db);
    sqlQuery.prepare("SELECT nickname FROM users WHERE nickname LIKE ? OR email LIKE ?");
    sqlQuery.addBindValue("%" + query + "%");
    sqlQuery.addBindValue("%" + query + "%");
    if (sqlQuery.exec() && sqlQuery.next()) {
        return sqlQuery.value("nickname").toString();
    }
    return QString();
}

bool Server::sendFriendRequest(const QString &from, const QString &to) {
    QSqlQuery query(db);
    qDebug() << "处理来自" << from << "向" << to << "发送的好友请求";
    
    // 检查是否已经是好友
    query.prepare("SELECT 1 FROM friends WHERE (user_nickname = ? AND friend_nickname = ?) OR (user_nickname = ? AND friend_nickname = ?)");
    query.addBindValue(from);
    query.addBindValue(to);
    query.addBindValue(to);
    query.addBindValue(from);
    if (query.exec() && query.next()) {
        qDebug() << "已经是好友关系，无需发送请求";
        return false; // 已经是好友
    }
    
    // 检查是否已经有待处理的请求
    query.prepare("SELECT 1 FROM friend_requests WHERE from_nickname = ? AND to_nickname = ? AND status = 'pending'");
    query.addBindValue(from);
    query.addBindValue(to);
    if (query.exec() && query.next()) {
        qDebug() << "已经存在待处理的请求";
        return false; // 已经有待处理的请求
    }
    
    // 添加好友请求
    query.prepare("INSERT INTO friend_requests (from_nickname, to_nickname, status) VALUES (?, ?, 'pending')");
    query.addBindValue(from);
    query.addBindValue(to);
    if (query.exec()) {
        qDebug() << "好友请求已添加到数据库";
        
        // 尝试立即通知对方（如果在线）
        bool notified = notifyFriendRequest(to, from);
        qDebug() << "通知状态：" << (notified ? "已通知" : "未通知（对方可能不在线）");
        
        return true;
    } else {
        qDebug() << "添加好友请求失败：" << query.lastError().text();
        return false;
    }
}

bool Server::acceptFriendRequest(const QString &from, const QString &to) {
    QSqlQuery query(db);
    qDebug() << "处理接受好友请求函数：" << from << "->" << to;
    
    // 检查请求是否存在
    query.prepare("SELECT 1 FROM friend_requests WHERE from_nickname = ? AND to_nickname = ? AND status = 'pending'");
    query.addBindValue(from);
    query.addBindValue(to);
    if (!query.exec()) {
        qDebug() << "查询好友请求失败：" << query.lastError().text();
        return false;
    }
    if (!query.next()) {
        qDebug() << "未找到待处理的好友请求";
        return false;
    }
    
    // 开始事务
    qDebug() << "开始数据库事务";
    if (!db.transaction()) {
        qDebug() << "开始事务失败：" << db.lastError().text();
        return false;
    }
    
    // 删除好友请求（不再是更新状态，而是直接删除）
    query.prepare("DELETE FROM friend_requests WHERE from_nickname = ? AND to_nickname = ?");
    query.addBindValue(from);
    query.addBindValue(to);
    if (!query.exec()) {
        qDebug() << "删除好友请求失败：" << query.lastError().text();
        db.rollback();
        return false;
    }
    
    // 添加好友关系（双向）
    query.prepare("INSERT INTO friends (user_nickname, friend_nickname) VALUES (?, ?), (?, ?)");
    query.addBindValue(from);
    query.addBindValue(to);
    query.addBindValue(to);
    query.addBindValue(from);
    if (!query.exec()) {
        qDebug() << "添加好友关系失败：" << query.lastError().text();
        db.rollback();
        return false;
    }
    
    qDebug() << "提交事务";
    if (!db.commit()) {
        qDebug() << "提交事务失败：" << db.lastError().text();
        db.rollback();
        return false;
    }
    
    qDebug() << "成功处理好友请求：" << from << "和" << to << "已成为好友";
    return true;
}

bool Server::deleteFriend(const QString &user, const QString &friendName) {
    QSqlQuery query(db);
    query.prepare("DELETE FROM friends WHERE (user_nickname = ? AND friend_nickname = ?) OR (user_nickname = ? AND friend_nickname = ?)");
    query.addBindValue(user);
    query.addBindValue(friendName);
    query.addBindValue(friendName);
    query.addBindValue(user);
    return query.exec();
}

QStringList Server::getFriendRequests(const QString &user) {
    QStringList requests;
    QSqlQuery query(db);
    qDebug() << "获取用户" << user << "的好友请求列表";
    query.prepare("SELECT from_nickname FROM friend_requests WHERE to_nickname = ? AND status = 'pending'");
    query.addBindValue(user);
    if (query.exec()) {
        while (query.next()) {
            QString from = query.value("from_nickname").toString();
            requests << from;
            qDebug() << "找到来自" << from << "的好友请求";
        }
    } else {
        qDebug() << "查询好友请求失败：" << query.lastError().text();
    }
    qDebug() << "总共找到" << requests.size() << "个好友请求";
    return requests;
}

bool Server::notifyFriendRequest(const QString &to, const QString &from) {
    QJsonObject notification;
    notification["from"] = from;
    QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::FriendRequest, notification)).toJson();
    
    // 遍历所有客户端，找到目标用户并发送通知
    bool notified = false;
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->isLoggedIn && it->nickname == to) {
            qDebug() << "Sending friend request notification from" << from << "to" << to;
            it->socket->write(message);
            it->socket->flush(); // 确保消息立即发送
            notified = true;
            break;
        }
    }
    
    return notified; // 返回是否成功通知对方
}

bool Server::deleteFriendRequest(const QString &from, const QString &to)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM friend_requests WHERE from_nickname = ? AND to_nickname = ?");
    query.addBindValue(from);
    query.addBindValue(to);
    if (!query.exec()) {
        qDebug() << "删除好友请求失败：" << query.lastError().text();
        return false;
    }
    qDebug() << "成功删除好友请求，从" << from << "到" << to;
    return true;
}