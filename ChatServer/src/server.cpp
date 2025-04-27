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
    // 初始化线程池
    m_threadPool = new ThreadPool(this);
    m_threadPool->init(QThread::idealThreadCount());
    qDebug() << "线程池已初始化，线程数：" << m_threadPool->size();

    // 初始化数据库访问信号量
    m_dbSemaphore = new Semaphore(this);
    m_dbSemaphore->create("db_semaphore", 1);

    // 初始化数据库文件锁
    m_dbFileLock = new FileLock(this);

    // 初始化聊天历史读写锁
    m_chatHistoryLock = new ReadWriteLock(this);
    m_chatHistoryLock->init();

    // 初始化消息队列
    m_messageQueue = new ThreadMessageQueue(this);

    // 初始化共享内存
    m_sharedMemory = new SharedMemory(this);
    m_sharedMemory->create("/tmp/chat_server", 1024 * 1024); // 1MB

    // 初始化进程管理器
    m_processManager = new ProcessManager(this);

    // 设置信号处理函数
    ProcessManager::setupSignalHandlers();

    // 初始化图片存储路径
    m_imageStoragePath = QCoreApplication::applicationDirPath() + "/../chat_images/";
    QDir().mkpath(m_imageStoragePath);
    qDebug() << "图片存储路径：" << m_imageStoragePath;

    // 初始化TCP服务器
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &Server::handleNewConnection);

    // 初始化数据库
    if (!initDatabase()) {
        qDebug() << "Failed to initialize database";
        QCoreApplication::quit();
    }
}

Server::~Server() {
    // 关闭数据库
    db.close();

    // 终止所有子进程
    m_processManager->terminateAllChildProcesses();

    // 清理共享内存
    m_sharedMemory->detach();

    // 清理信号量
    m_dbSemaphore->close();

    // 清理文件锁
    m_dbFileLock->close();

    // 清理读写锁
    m_chatHistoryLock->destroy();
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

    // 使用互斥锁保护clients列表
    {
        QMutexLocker locker(&m_clientsMutex);
        ClientInfo clientInfo;
        clientInfo.socket = clientSocket;
        clientInfo.isLoggedIn = false;
        clients.append(clientInfo);
    }

    connect(clientSocket, &QTcpSocket::readyRead, this, &Server::handleClientData);
    connect(clientSocket, &QTcpSocket::disconnected, this, &Server::handleClientDisconnection);

    // 尝试使用进程管理器创建一个子进程来处理连接
    // 注意：这里只是演示进程控制，实际上这种场景可能不需要创建子进程
    static int connectionCount = 0;
    if (++connectionCount % 5 == 0) { // 每5个连接创建一个子进程
        QString clientAddress = clientSocket->peerAddress().toString();
        pid_t pid = m_processManager->startChildProcess("connection_handler",
                                                      QStringList() << clientAddress
                                                                   << QString::number(clientSocket->peerPort()));
        if (pid > 0) {
            qDebug() << "Started child process for connection" << connectionCount
                     << "with PID:" << pid;
        }
    }
}

void Server::handleClientData() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    // 读取所有可用数据
    QByteArray data = clientSocket->readAll();

    // 检查数据是否完整
    if (data.isEmpty()) {
        qDebug() << "收到空数据包";
        return;
    }

    // 尝试解析JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON解析错误:" << parseError.errorString() << "，位置:" << parseError.offset;
        qDebug() << "数据大小:" << data.size() << "字节";

        // 尝试查找消息类型，特别处理分块图片上传
        if (data.contains("\"type\":30") || data.contains("\"type\": 30")) {
            // 这可能是一个ChunkedImageChunk消息
            // 尝试提取tempId和chunk_index
            int tempIdPos = data.indexOf("\"temp_id\"");
            int chunkIndexPos = data.indexOf("\"chunk_index\"");
            int chunkDataPos = data.indexOf("\"chunk_data\"");

            if (tempIdPos > 0 && chunkIndexPos > 0 && chunkDataPos > 0) {
                // 尝试提取tempId
                int tempIdStart = data.indexOf("\"", tempIdPos + 10) + 1;
                int tempIdEnd = data.indexOf("\"", tempIdStart);
                QString tempId = QString::fromUtf8(data.mid(tempIdStart, tempIdEnd - tempIdStart));

                // 尝试提取chunk_index
                int chunkIndexStart = chunkIndexPos + 14;
                int chunkIndexEnd = data.indexOf(",", chunkIndexStart);
                if (chunkIndexEnd < 0) chunkIndexEnd = data.indexOf("}", chunkIndexStart);
                QString chunkIndexStr = QString::fromUtf8(data.mid(chunkIndexStart, chunkIndexEnd - chunkIndexStart));
                int chunkIndex = chunkIndexStr.trimmed().toInt();

                // 尝试提取chunk_data (Base64)
                int chunkDataStart = data.indexOf("\"", chunkDataPos + 13) + 1;
                int chunkDataEnd = data.lastIndexOf("\"");
                QByteArray chunkDataBase64 = data.mid(chunkDataStart, chunkDataEnd - chunkDataStart);

                // 解码Base64数据
                QByteArray chunkData = QByteArray::fromBase64(chunkDataBase64);

                // 检查是否存在对应的分块上传记录
                if (m_pendingChunkedImages.contains(tempId)) {
                    // 获取分块上传记录
                    ChunkedImageData &chunkedData = m_pendingChunkedImages[tempId];

                    // 添加到图片数据中
                    chunkedData.imageData.append(chunkData);
                    chunkedData.receivedChunks++;

                    qDebug() << "手动解析接收到图片数据块:" << chunkIndex + 1 << "/" << chunkedData.totalChunks
                             << "，大小:" << chunkData.size() << "字节";

                    return;
                }
            }
        }

        // 如果数据太大，可能需要分片处理
        if (data.size() > 1024 * 1024) { // 大于1MB的数据
            qDebug() << "数据过大，可能需要分片处理";
        }
        return;
    }

    // 使用线程池处理客户端请求
    m_threadPool->addTask([this, clientSocket, data]() {
        this->processClientData(clientSocket, data);
    });
}

void Server::sendResponseToClient(QTcpSocket *clientSocket, const QByteArray &response) {
    // 这个方法在主线程中执行，可以安全地操作QTcpSocket
    if (clientSocket && clientSocket->isOpen()) {
        clientSocket->write(response);
        clientSocket->flush();
    }
}

QSqlDatabase Server::getThreadLocalDatabase() {
    // 获取当前线程ID作为连接名
    QString connectionName = QString("connection_%1").arg((quintptr)QThread::currentThreadId());

    // 检查当前线程是否已有数据库连接
    if (QSqlDatabase::contains(connectionName)) {
        QSqlDatabase db = QSqlDatabase::database(connectionName);

        // 检查连接是否有效
        if (!db.isOpen()) {
            qDebug() << "数据库连接已存在但未打开，尝试重新打开";
            if (!db.open()) {
                qDebug() << "重新打开数据库连接失败:" << db.lastError().text();
            } else {
                qDebug() << "成功重新打开数据库连接";
            }
        }

        return db;
    }

    // 为当前线程创建新的数据库连接
    QString dbPath = QCoreApplication::applicationDirPath() + "/../users.db";

    // 使用互斥锁保护数据库连接创建过程
    static QMutex connectionMutex;
    QMutexLocker locker(&connectionMutex);

    QSqlDatabase threadDb;
    if (QSqlDatabase::contains(connectionName)) {
        // 双重检查，避免在获取锁的过程中其他线程已创建连接
        threadDb = QSqlDatabase::database(connectionName);
    } else {
        threadDb = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        threadDb.setDatabaseName(dbPath);
    }

    if (!threadDb.isOpen() && !threadDb.open()) {
        qDebug() << "Error: Failed to open database for thread:" << threadDb.lastError().text();
    } else {
        qDebug() << "Successfully opened database connection for thread" << QThread::currentThreadId();
    }

    return threadDb;
}

void Server::processClientData(QTcpSocket *clientSocket, const QByteArray &data) {
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();

    // 解析消息
    MessageType type;
    QJsonObject msgData;

    if (!MessageProtocol::parseMessage(data, type, msgData)) {
        qDebug() << "解析消息失败!";
        return;
    }

    qDebug() << "收到消息类型:" << static_cast<int>(type) << " - " << MessageProtocol::messageTypeToString(type);

    // 获取对应的客户端信息
    ClientInfo *clientInfo = nullptr;
    {
        QMutexLocker locker(&m_clientsMutex); // 保护clients列表的互斥锁
        for (ClientInfo &info : clients) {
            if (info.socket == clientSocket) {
                clientInfo = &info;
                break;
            }
        }
    }

    // 根据消息类型处理
    switch (type) {
    case MessageType::Register: {
        QString email = msgData["email"].toString();
        QString nickname = msgData["nickname"].toString();
        QString password = msgData["password"].toString();
        qDebug() << "Register request: email=" << email << ", nickname=" << nickname;
        if (email.isEmpty() || nickname.isEmpty() || password.isEmpty()) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Register, {{"status", "failed"}, {"reason", "Empty email, nickname, or password"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        if (registerUser(email, nickname, password)) {
            qDebug() << "Registration successful for" << nickname;
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Register, {{"status", "success"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
        } else {
            qDebug() << "Registration failed for" << nickname;
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Register, {{"status", "failed"}, {"reason", "Email or nickname already exists"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
        }
        break;
    }
    case MessageType::Login: {
        QString nickname = msgData["nickname"].toString();
        QString password = msgData["password"].toString();
        qDebug() << "Login request: nickname=" << nickname;
        if (nickname.isEmpty() || password.isEmpty()) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Login, {{"status", "failed"}, {"reason", "Empty nickname or password"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        QString loginResult = loginUser(nickname, password, *clientInfo);
        if (loginResult == "success") {
            qDebug() << "Login successful for" << nickname;
            clientInfo->isLoggedIn = true;
            clientInfo->nickname = nickname;
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Login, {{"status", "success"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
        } else {
            qDebug() << "Login failed for" << nickname << ":" << loginResult;
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Login, {{"status", "failed"}, {"reason", loginResult}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
        }
        break;
    }
    case MessageType::Message:
        if (!clientInfo->isLoggedIn) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        {
            QString to = msgData["to"].toString();
            QString content = msgData["content"].toString();

            // 获取线程本地数据库连接
            QSqlDatabase threadDb = getThreadLocalDatabase();

            // 确保数据库连接有效
            if (!threadDb.isOpen()) {
                qDebug() << "数据库连接未打开，尝试重新打开";
                if (!threadDb.open()) {
                    qDebug() << "无法打开数据库连接:" << threadDb.lastError().text();
                    QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Database error"}})).toJson();
                    QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                             Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
                    return;
                }
            }

            // 检查content是否已经是JSON格式
            QJsonDocument contentDoc = QJsonDocument::fromJson(content.toUtf8());
            QString finalContent;

            if (contentDoc.isNull() || !contentDoc.isObject()) {
                // 如果不是有效的JSON，则将其包装为文本消息JSON
                QJsonObject textMsg;
                textMsg["type"] = "text";
                textMsg["text"] = content;
                finalContent = QJsonDocument(textMsg).toJson(QJsonDocument::Compact);
            } else {
                // 已经是JSON格式，直接使用
                finalContent = content;
            }

            // 保存消息到数据库
            QSqlQuery query(threadDb);
            query.prepare("INSERT INTO messages (from_nickname, to_nickname, content) VALUES (?, ?, ?)");
            query.addBindValue(clientInfo->nickname);
            query.addBindValue(to);
            query.addBindValue(finalContent);

            bool saveSuccess = query.exec();
            if (!saveSuccess) {
                qDebug() << "保存消息失败:" << query.lastError().text();
                QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Failed to save message"}})).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
                return;
            }

            // 发送消息给目标用户
            QJsonObject privateMsg;
            privateMsg["from"] = clientInfo->nickname;
            privateMsg["content"] = content;
            QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, privateMsg)).toJson();

            // 在主线程中发送消息给目标用户
            QMutexLocker locker(&m_clientsMutex);
            for (ClientInfo &c : clients) {
                if (c.isLoggedIn && c.nickname == to) {
                    QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                             Q_ARG(QTcpSocket*, c.socket), Q_ARG(QByteArray, message));
                    break;
                }
            }
        }
        break;
    case MessageType::SearchUser:
        {
            QString query = msgData["query"].toString();
            QString nickname = searchUser(query);
            if (!nickname.isEmpty()) {
                QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::SearchUser, {{"status", "success"}, {"nickname", nickname}})).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            } else {
                QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::SearchUser, {{"status", "failed"}})).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            }
        }
        break;
    case MessageType::AddFriend:
        if (!clientInfo->isLoggedIn) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        {
            QString friendName = msgData["friend"].toString();
            if (addFriend(clientInfo->nickname, friendName)) {
                QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::AddFriend, {{"status", "success"}})).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            } else {
                QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::AddFriend, {{"status", "failed"}, {"reason", "Friend not found or already added"}})).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            }
        }
        break;
    case MessageType::FriendList:
        if (!clientInfo->isLoggedIn) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
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
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(MessageType::FriendList, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        }
        break;
    case MessageType::ChatHistory:
        if (!clientInfo->isLoggedIn) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        {
            QString friendName = msgData["friend"].toString(); // 修复变量名，避免使用 C++ 关键字 "friend"
            QJsonObject response;
            response["status"] = "success";
            response["messages"] = getChatHistory(clientInfo->nickname, friendName);
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(MessageType::ChatHistory, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        }
        break;
    case MessageType::Logout:
        if (!clientInfo->isLoggedIn) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Not logged in"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        handleLogout(clientInfo);
        break;
    case MessageType::FriendRequest:
        if (!clientInfo->isLoggedIn) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        {
            QString to = msgData["to"].toString();
            if (sendFriendRequest(clientInfo->nickname, to)) {
                QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::FriendRequest, {{"status", "success"}})).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            } else {
                QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::FriendRequest, {{"status", "failed"}, {"reason", "Request already sent or users are already friends"}})).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            }
        }
        break;
    case MessageType::AcceptFriend:
        qDebug() << "收到接受好友请求消息: " << MessageProtocol::messageTypeToString(type) << " - " << msgData;
        if (!clientInfo->isLoggedIn) {
            qDebug() << "未登录用户尝试接受好友请求";
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
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
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, accepterMsg));
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
                    MessageType::FriendList, friendsResponse)).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, friendsMsg));
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
                    MessageType::FriendRequestList, refreshRequestsResponse)).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, refreshRequestsMsg));
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
                        QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                                 Q_ARG(QTcpSocket*, client.socket), Q_ARG(QByteArray, senderMsg));
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
                            MessageType::FriendList, senderFriendsResponse)).toJson();
                        QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                                 Q_ARG(QTcpSocket*, client.socket), Q_ARG(QByteArray, senderFriendsMsg));
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
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, errorMsg));
                qDebug() << "已发送错误响应：" << errorMsg;
            }
        }
        break;
    case MessageType::DeleteFriend:
        if (!clientInfo->isLoggedIn) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        {
            QString friendName = msgData["friend"].toString();
            if (deleteFriend(clientInfo->nickname, friendName)) {
                QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::DeleteFriend, {{"status", "success"}})).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));

                // 通知被删除的好友
                QMutexLocker locker(&m_clientsMutex);
                for (const ClientInfo &client : clients) {
                    if (client.isLoggedIn && client.nickname == friendName) {
                        QByteArray notifyMsg = QJsonDocument(MessageProtocol::createMessage(MessageType::DeleteFriend, {{"status", "success"}, {"friend", clientInfo->nickname}})).toJson();
                        QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                                 Q_ARG(QTcpSocket*, client.socket), Q_ARG(QByteArray, notifyMsg));
                        break;
                    }
                }
            } else {
                QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::DeleteFriend, {{"status", "failed"}, {"reason", "Friend not found"}})).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            }
        }
        break;
    case MessageType::FriendRequestList:
        if (!clientInfo->isLoggedIn) {
            qDebug() << "未登录用户尝试获取好友请求列表";
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
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
            QByteArray responseMsg = QJsonDocument(MessageProtocol::createMessage(MessageType::FriendRequestList, response)).toJson();
            qDebug() << "发送好友请求列表响应：" << response;
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseMsg));
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
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientInfo->socket), Q_ARG(QByteArray, errorMsg));
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
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientInfo->socket), Q_ARG(QByteArray, errorMsg));
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
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientInfo->socket), Q_ARG(QByteArray, responseMsg));
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
                MessageType::FriendRequestList, refreshResponse)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientInfo->socket), Q_ARG(QByteArray, refreshMsg));
            qDebug() << "已发送刷新好友请求列表响应：" << refreshResponse;
        } else {
            // 发送失败响应
            QJsonObject errorResponse;
            errorResponse["status"] = "failed";
            errorResponse["reason"] = "删除好友请求失败";
            QByteArray errorMsg = QJsonDocument(MessageProtocol::createMessage(
                MessageType::DeleteFriendRequest, errorResponse)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientInfo->socket), Q_ARG(QByteArray, errorMsg));
            qDebug() << "已发送错误响应：" << errorMsg;
        }
        break;
    }
    case MessageType::CreateGroup:
        if (!clientInfo->isLoggedIn) {
            clientSocket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson());
            return;
        }
        {
            QString groupName = msgData["group_name"].toString();
            QJsonArray membersArray = msgData["members"].toArray();
            QStringList members;

            // 将JSON数组转换为字符串列表
            for (const QJsonValue &value : membersArray) {
                members << value.toString();
            }

            // 确保创建者也在群聊中
            if (!members.contains(clientInfo->nickname)) {
                members << clientInfo->nickname;
            }

            qDebug() << "创建群聊请求：" << groupName << "，成员：" << members.join(", ");

            if (createGroup(clientInfo->nickname, groupName, members)) {
                QJsonObject response;
                response["status"] = "success";
                response["group_name"] = groupName;
                QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(MessageType::CreateGroup, response)).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            } else {
                QJsonObject response;
                response["status"] = "failed";
                response["reason"] = "创建群聊失败";
                QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(MessageType::CreateGroup, response)).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            }
        }
        break;
    case MessageType::GroupList:
        if (!clientInfo->isLoggedIn) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        {
            QStringList groups = getGroupList(clientInfo->nickname);
            QJsonArray groupArray;
            for (const QString &g : groups) {
                groupArray.append(g);
            }
            QJsonObject response;
            response["status"] = "success";
            response["groups"] = groupArray;
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(MessageType::GroupList, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));

            // 打印调试信息
            qDebug() << "发送群聊列表给用户" << clientInfo->nickname << "，群聊数量：" << groups.size();
            for (const QString &g : groups) {
                qDebug() << "  群聊：" << g;
            }
        }
        break;
    case MessageType::GroupMembers:
        if (!clientInfo->isLoggedIn) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        {
            int groupId = msgData["group_id"].toInt();
            QStringList members = getGroupMembers(groupId);
            QJsonArray memberArray;
            for (const QString &m : members) {
                memberArray.append(m);
            }
            QJsonObject response;
            response["status"] = "success";
            response["group_id"] = groupId;
            response["members"] = memberArray;
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(MessageType::GroupMembers, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        }
        break;
    case MessageType::GroupChat:
        if (!clientInfo->isLoggedIn) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        {
            int groupId = msgData["group_id"].toInt();
            QString content = msgData["content"].toString();

            // 获取线程本地数据库连接
            QSqlDatabase threadDb = getThreadLocalDatabase();

            // 确保数据库连接有效
            if (!threadDb.isOpen()) {
                qDebug() << "数据库连接未打开，尝试重新打开";
                if (!threadDb.open()) {
                    qDebug() << "无法打开数据库连接:" << threadDb.lastError().text();
                    QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::GroupChat, {{"status", "failed"}, {"reason", "Database error"}})).toJson();
                    QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                             Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
                    return;
                }
            }

            // 检查content是否已经是JSON格式
            QJsonDocument contentDoc = QJsonDocument::fromJson(content.toUtf8());
            QString finalContent;

            if (contentDoc.isNull() || !contentDoc.isObject()) {
                // 如果不是有效的JSON，则将其包装为文本消息JSON
                QJsonObject textMsg;
                textMsg["type"] = "text";
                textMsg["text"] = content;
                finalContent = QJsonDocument(textMsg).toJson(QJsonDocument::Compact);
            } else {
                // 已经是JSON格式，直接使用
                finalContent = content;
            }

            // 保存消息到数据库
            QSqlQuery query(threadDb);
            query.prepare("INSERT INTO group_messages (group_id, from_nickname, content) VALUES (?, ?, ?)");
            query.addBindValue(groupId);
            query.addBindValue(clientInfo->nickname);
            query.addBindValue(finalContent);

            bool saveSuccess = query.exec();
            if (saveSuccess) {
                // 通知其他群成员
                notifyGroupMessage(groupId, clientInfo->nickname, content);

                // 发送成功响应给发送者
                QJsonObject response;
                response["status"] = "success";
                QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(MessageType::GroupChat, response)).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            } else {
                qDebug() << "保存群聊消息失败:" << query.lastError().text();
                QJsonObject response;
                response["status"] = "failed";
                response["reason"] = "发送群消息失败: " + query.lastError().text();
                QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(MessageType::GroupChat, response)).toJson();
                QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                         Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            }
        }
        break;
    case MessageType::GroupChatHistory:
        if (!clientInfo->isLoggedIn) {
            QByteArray response = QJsonDocument(MessageProtocol::createMessage(MessageType::Message, {{"status", "failed"}, {"reason", "Please login first"}})).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, response));
            return;
        }
        {
            int groupId = msgData["group_id"].toInt();
            QJsonArray chatHistory = getGroupChatHistory(groupId);
            QJsonObject response;
            response["status"] = "success";
            response["group_id"] = groupId;
            response["messages"] = chatHistory;
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(MessageType::GroupChatHistory, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        }
        break;
    case MessageType::GetUserProfile: {
        if (!clientInfo || !clientInfo->isLoggedIn) {
            QJsonObject response;
            response["status"] = "error";
            response["reason"] = "Not logged in";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::GetUserProfile, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }

        QString nickname = msgData.value("nickname").toString();
        if (nickname.isEmpty()) {
            nickname = clientInfo->nickname; // 默认查询自己的资料
        }

        QJsonObject userProfile = getUserProfile(nickname);
        if (!userProfile.isEmpty()) {
            userProfile["status"] = "success";
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::GetUserProfile, userProfile)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        } else {
            QJsonObject response;
            response["status"] = "error";
            response["reason"] = "User not found";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::GetUserProfile, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        }
        break;
    }
    case MessageType::UpdateUserProfile: {
        if (!clientInfo || !clientInfo->isLoggedIn) {
            QJsonObject response;
            response["status"] = "error";
            response["reason"] = "Not logged in";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UpdateUserProfile, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }

        // 确保用户只能更新自己的资料
        QString nickname = msgData.value("nickname").toString();
        if (nickname != clientInfo->nickname) {
            QJsonObject response;
            response["status"] = "error";
            response["reason"] = "Cannot update profile of other users";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UpdateUserProfile, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }

        // 更新用户资料
        if (updateUserProfile(nickname, msgData)) {
            QJsonObject response;
            response["status"] = "success";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UpdateUserProfile, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        } else {
            QJsonObject response;
            response["status"] = "error";
            response["reason"] = "Failed to update profile";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UpdateUserProfile, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        }
        break;
    }
    case MessageType::UploadAvatar: {
        if (!clientInfo || !clientInfo->isLoggedIn) {
            QJsonObject response;
            response["status"] = "error";
            response["reason"] = "Not logged in";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UploadAvatar, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }

        // 确保用户只能更新自己的头像
        QString nickname = msgData.value("nickname").toString();
        if (nickname != clientInfo->nickname) {
            QJsonObject response;
            response["status"] = "error";
            response["reason"] = "Cannot upload avatar for other users";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UploadAvatar, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }

        // 获取Base64编码的头像数据
        QByteArray avatarData = QByteArray::fromBase64(msgData.value("avatar_data").toString().toLatin1());
        if (avatarData.isEmpty()) {
            QJsonObject response;
            response["status"] = "error";
            response["reason"] = "Invalid avatar data";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UploadAvatar, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }

        // 保存头像
        if (saveAvatar(nickname, avatarData)) {
            QJsonObject response;
            response["status"] = "success";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UploadAvatar, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        } else {
            QJsonObject response;
            response["status"] = "error";
            response["reason"] = "Failed to save avatar";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UploadAvatar, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        }
        break;
    }
    case MessageType::GetAvatar: {
        QString nickname = msgData.value("nickname").toString();
        if (nickname.isEmpty()) {
            QJsonObject response;
            response["status"] = "error";
            response["reason"] = "No nickname specified";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::GetAvatar, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }

        QByteArray avatarData = getAvatar(nickname);
        if (!avatarData.isEmpty()) {
            QJsonObject response;
            response["status"] = "success";
            response["nickname"] = nickname;
            response["avatar_data"] = QString::fromLatin1(avatarData.toBase64());

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::GetAvatar, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        } else {
            QJsonObject response;
            response["status"] = "error";
            response["reason"] = "Avatar not found";

            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::GetAvatar, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        }
        break;
    }
    case MessageType::UploadImageRequest: {
        if (!clientInfo->isLoggedIn) {
            QJsonObject response;
            response["status"] = "failed";
            response["reason"] = "Please login first";
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UploadImageResponse, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }

        // 从请求中获取图片数据和文件扩展名
        QString imageDataBase64 = msgData["image_data_base64"].toString();
        QString fileExtension = msgData["file_extension"].toString();
        QString tempId = msgData["temp_id"].toString();

        if (imageDataBase64.isEmpty() || fileExtension.isEmpty()) {
            QJsonObject response;
            response["status"] = "failed";
            response["reason"] = "Invalid image data or file extension";
            if (!tempId.isEmpty()) {
                response["temp_id"] = tempId;
            }
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UploadImageResponse, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }

        // 解码Base64数据
        QByteArray imageData = QByteArray::fromBase64(imageDataBase64.toLatin1());

        // 生成唯一的图片ID
        QString imageId = generateUniqueImageId(fileExtension);

        // 保存图片
        if (saveImage(imageId, imageData)) {
            QJsonObject response;
            response["status"] = "success";
            response["imageId"] = imageId;
            if (!tempId.isEmpty()) {
                response["temp_id"] = tempId;
            }
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UploadImageResponse, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            qDebug() << "图片上传成功，ID:" << imageId << "，临时ID:" << tempId;
        } else {
            QJsonObject response;
            response["status"] = "failed";
            response["reason"] = "Error saving image";
            if (!tempId.isEmpty()) {
                response["temp_id"] = tempId;
            }
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UploadImageResponse, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            qDebug() << "图片上传失败，临时ID:" << tempId;
        }
        break;
    }

    case MessageType::ChunkedImageStart: {
        if (!clientInfo->isLoggedIn) {
            QJsonObject response;
            response["status"] = "failed";
            response["reason"] = "Please login first";
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::ChunkedImageResponse, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }
        handleChunkedImageStart(clientSocket, msgData, clientInfo);
        break;
    }

    case MessageType::ChunkedImageChunk: {
        if (!clientInfo->isLoggedIn) {
            QJsonObject response;
            response["status"] = "failed";
            response["reason"] = "Please login first";
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::ChunkedImageResponse, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }
        handleChunkedImageChunk(clientSocket, msgData, clientInfo);
        break;
    }

    case MessageType::ChunkedImageEnd: {
        if (!clientInfo->isLoggedIn) {
            QJsonObject response;
            response["status"] = "failed";
            response["reason"] = "Please login first";
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::ChunkedImageResponse, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }
        handleChunkedImageEnd(clientSocket, msgData, clientInfo);
        break;
    }
    case MessageType::DownloadImageRequest: {
        if (!clientInfo->isLoggedIn) {
            QJsonObject response;
            response["status"] = "failed";
            response["reason"] = "Please login first";
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::DownloadImageResponse, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }

        // 从请求中获取图片ID
        QString imageId = msgData["imageId"].toString();

        if (imageId.isEmpty()) {
            QJsonObject response;
            response["status"] = "failed";
            response["reason"] = "Invalid image ID";
            response["imageId"] = imageId; // 返回原始imageId，方便客户端匹配
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::DownloadImageResponse, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            return;
        }

        // 获取图片数据
        QByteArray imageData = getImage(imageId);

        if (!imageData.isEmpty()) {
            // 直接使用二进制格式，不发送JSON预告
            // 格式: [4字节魔数][4字节消息类型][4字节图片ID长度][图片ID][4字节图片数据长度][图片数据]
            QByteArray binaryPacket;

            // 魔数: "IMGD" (Image Data)
            binaryPacket.append("IMGD", 4);

            // 消息类型
            qint32 messageType = static_cast<qint32>(MessageType::BinaryImageData);
            binaryPacket.append(reinterpret_cast<const char*>(&messageType), sizeof(messageType));

            // 图片ID长度
            qint32 imageIdLength = imageId.toUtf8().length();
            binaryPacket.append(reinterpret_cast<const char*>(&imageIdLength), sizeof(imageIdLength));

            // 图片ID
            binaryPacket.append(imageId.toUtf8());

            // 图片数据长度
            qint32 imageDataLength = imageData.size();
            binaryPacket.append(reinterpret_cast<const char*>(&imageDataLength), sizeof(imageDataLength));

            // 图片数据
            binaryPacket.append(imageData);

            // 打印前几个字节用于调试
            QString hexData;
            for (int i = 0; i < qMin(32, binaryPacket.size()); ++i) {
                hexData += QString("%1 ").arg(static_cast<unsigned char>(binaryPacket[i]), 2, 16, QChar('0'));
            }
            qDebug() << "二进制数据包前32字节:" << hexData;
            qDebug() << "二进制数据包总大小:" << binaryPacket.size() << "字节，图片ID长度:" << imageIdLength
                     << "，图片数据长度:" << imageDataLength;

            // 直接发送二进制数据包，不使用QueuedConnection
            clientSocket->write(binaryPacket);
            clientSocket->flush();

            // 确保数据发送完成
            clientSocket->waitForBytesWritten(5000);

            qDebug() << "图片下载成功，ID:" << imageId << "，大小:" << imageData.size() << "字节，使用二进制模式发送";
        } else {
            QJsonObject response;
            response["status"] = "failed";
            response["reason"] = "Image not found or read error";
            response["imageId"] = imageId; // 返回原始imageId，方便客户端匹配
            QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
                MessageType::DownloadImageResponse, response)).toJson();
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
            qDebug() << "图片下载失败，ID:" << imageId;
        }
        break;
    }
    default:
        qDebug() << "未处理的消息类型:" << static_cast<int>(type);
        break;
    }
}

void Server::handleClientDisconnection() {
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket) return;

    // 使用互斥锁保护clients列表
    {
        QMutexLocker locker(&m_clientsMutex);
        for (int i = 0; i < clients.size(); ++i) {
            if (clients[i].socket == clientSocket) {
                if (clients[i].isLoggedIn) {
                    // 在锁内调用handleLogout可能会导致死锁，所以先保存信息
                    QString nickname = clients[i].nickname;
                    bool isLoggedIn = clients[i].isLoggedIn;

                    // 先从列表中移除
                    clients.removeAt(i);

                    // 释放锁后再处理登出逻辑
                    locker.unlock();

                    if (isLoggedIn) {
                        // 更新用户状态
                        updateUserStatus(nickname, false);
                        notifyFriendsStatusChange(nickname, false);
                    }

                    break;
                } else {
                    // 如果未登录，直接移除
                    clients.removeAt(i);
                    break;
                }
            }
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
    QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(MessageType::Logout, response)).toJson();
    QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                             Q_ARG(QTcpSocket*, clientInfo->socket), Q_ARG(QByteArray, responseData));

    clientInfo->isLoggedIn = false;
    clientInfo->nickname.clear();
}

void Server::updateUserStatus(const QString &nickname, bool isOnline) {
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    if (isOnline) {
        // 用户上线，只更新在线状态
        query.prepare("UPDATE users SET is_online = 1 WHERE nickname = ?");
        query.addBindValue(nickname);
    } else {
        // 用户下线，更新在线状态和最后登录时间
        QDateTime currentTime = QDateTime::currentDateTime();
        query.prepare("UPDATE users SET is_online = 0, last_login_time = ? WHERE nickname = ?");
        query.addBindValue(currentTime.toString(Qt::ISODate));
        query.addBindValue(nickname);
    }

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
    QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::FriendStatus, statusMsg)).toJson();

    QMutexLocker locker(&m_clientsMutex);
    for (const ClientInfo &client : clients) {
        if (client.isLoggedIn && friends.contains(client.nickname)) {
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, client.socket), Q_ARG(QByteArray, message));
        }
    }
}

bool Server::initDatabase() {
    QString dbPath = QCoreApplication::applicationDirPath() + "/../users.db";
    qDebug() << "Database path:" << dbPath;

    // 使用文件锁保护数据库文件
    m_dbFileLock->open(dbPath);
    if (!m_dbFileLock->lock(FileLock::WriteLock)) {
        qDebug() << "Error: Failed to acquire database file lock";
        return false;
    }

    // 使用信号量控制数据库访问
    if (!m_dbSemaphore->wait()) {
        qDebug() << "Error: Failed to acquire database semaphore";
        m_dbFileLock->unlock();
        return false;
    }

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);
    if (!db.open()) {
        qDebug() << "Error: Failed to open database:" << db.lastError().text();
        m_dbSemaphore->post();
        m_dbFileLock->unlock();
        return false;
    }

    // 确保图片存储目录存在
    QDir imageDir(m_imageStoragePath);
    if (!imageDir.exists()) {
        if (!imageDir.mkpath(".")) {
            qDebug() << "Error: Failed to create image storage directory:" << m_imageStoragePath;
        } else {
            qDebug() << "Created image storage directory:" << m_imageStoragePath;
        }
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
            is_online INTEGER DEFAULT 0,
            signature TEXT DEFAULT '',
            avatar TEXT DEFAULT '',
            gender TEXT DEFAULT '',
            birthday TEXT DEFAULT '',
            location TEXT DEFAULT '',
            phone TEXT DEFAULT '',
            register_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            last_login_time DATETIME,
            friend_count INTEGER DEFAULT 0,
            group_count INTEGER DEFAULT 0,
            reserved1 TEXT DEFAULT '',
            reserved2 TEXT DEFAULT ''
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

    // 创建群聊表
    QString createGroupsTable = R"(
        CREATE TABLE IF NOT EXISTS groups (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            creator_nickname TEXT,
            create_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (creator_nickname) REFERENCES users(nickname)
        )
    )";
    if (!query.exec(createGroupsTable)) {
        qDebug() << "Error: Failed to create groups table:" << query.lastError().text();
        return false;
    }

    // 创建群成员表
    QString createGroupMembersTable = R"(
        CREATE TABLE IF NOT EXISTS group_members (
            group_id INTEGER,
            member_nickname TEXT,
            join_time DATETIME DEFAULT CURRENT_TIMESTAMP,
            PRIMARY KEY (group_id, member_nickname),
            FOREIGN KEY (group_id) REFERENCES groups(id),
            FOREIGN KEY (member_nickname) REFERENCES users(nickname)
        )
    )";
    if (!query.exec(createGroupMembersTable)) {
        qDebug() << "Error: Failed to create group_members table:" << query.lastError().text();
        return false;
    }

    // 创建群消息表
    QString createGroupMessagesTable = R"(
        CREATE TABLE IF NOT EXISTS group_messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            group_id INTEGER,
            from_nickname TEXT,
            content TEXT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (group_id) REFERENCES groups(id),
            FOREIGN KEY (from_nickname) REFERENCES users(nickname)
        )
    )";
    if (!query.exec(createGroupMessagesTable)) {
        qDebug() << "Error: Failed to create group_messages table:" << query.lastError().text();
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

    // 释放数据库信号量和文件锁
    m_dbSemaphore->post();
    m_dbFileLock->unlock();

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

    // 使用当前时间作为注册时间
    QDateTime currentTime = QDateTime::currentDateTime();

    query.prepare("INSERT INTO users (email, nickname, password, is_online, register_time, last_login_time) "
                  "VALUES (?, ?, ?, 0, ?, ?)");
    query.addBindValue(email);
    query.addBindValue(nickname);
    query.addBindValue(hashPassword(password));
    query.addBindValue(currentTime.toString(Qt::ISODate));
    query.addBindValue(currentTime.toString(Qt::ISODate)); // 初始登录时间与注册时间相同
    return query.exec();
}

QString Server::loginUser(const QString &nickname, const QString &password, ClientInfo &client) {
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    query.prepare("SELECT password FROM users WHERE nickname = ?");
    query.addBindValue(nickname);
    if (!query.exec() || !query.next()) {
        return "User not found";
    }
    QString storedPassword = query.value("password").toString();
    if (storedPassword == hashPassword(password)) {
        // 更新用户在线状态
        updateUserStatus(nickname, true);

        // 更新最后登录时间
        QDateTime currentTime = QDateTime::currentDateTime();
        QSqlQuery updateQuery(threadDb);
        updateQuery.prepare("UPDATE users SET last_login_time = ? WHERE nickname = ?");
        updateQuery.addBindValue(currentTime.toString(Qt::ISODate));
        updateQuery.addBindValue(nickname);
        if (!updateQuery.exec()) {
            qDebug() << "Error updating last login time:" << updateQuery.lastError().text();
        }

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
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    query.prepare("SELECT nickname FROM users WHERE nickname = :friend");
    query.bindValue(":friend", friendName);
    if (!query.exec() || !query.next()) {
        return false;
    }

    // 开始事务以确保数据一致性
    threadDb.transaction();

    // 添加好友关系
    query.prepare("INSERT OR IGNORE INTO friends (user_nickname, friend_nickname) VALUES (:user, :friend)");
    query.bindValue(":user", user);
    query.bindValue(":friend", friendName);
    bool success = query.exec();

    if (success && query.numRowsAffected() > 0) {
        // 更新用户的好友数量
        query.prepare("UPDATE users SET friend_count = friend_count + 1 WHERE nickname = :user");
        query.bindValue(":user", user);
        success = query.exec() && success;

        // 更新好友的好友数量
        query.prepare("UPDATE users SET friend_count = friend_count + 1 WHERE nickname = :friend");
        query.bindValue(":friend", friendName);
        success = query.exec() && success;
    }

    // 根据操作结果提交或回滚事务
    if (success) {
        threadDb.commit();
    } else {
        threadDb.rollback();
        qDebug() << "添加好友失败:" << query.lastError().text();
    }

    return success;
}

QStringList Server::getFriendList(const QString &user) {
    QStringList friends;

    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

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
    // 使用写锁保护聊天历史的写入
    WriteLocker locker(m_chatHistoryLock);
    if (!locker.isLocked()) {
        qDebug() << "Failed to acquire write lock for chat history";
        return false;
    }

    // 使用数据库信号量控制数据库访问
    if (!m_dbSemaphore->wait()) {
        qDebug() << "Failed to acquire database semaphore";
        return false;
    }

    // 检查content是否已经是JSON格式
    QJsonDocument contentDoc = QJsonDocument::fromJson(content.toUtf8());
    QString finalContent;

    if (contentDoc.isNull() || !contentDoc.isObject()) {
        // 如果不是有效的JSON，则将其包装为文本消息JSON
        QJsonObject textMsg;
        textMsg["type"] = "text";
        textMsg["text"] = content;
        finalContent = QJsonDocument(textMsg).toJson(QJsonDocument::Compact);
    } else {
        // 已经是JSON格式，直接使用
        finalContent = content;
    }

    QSqlQuery query;
    query.prepare("INSERT INTO messages (from_nickname, to_nickname, content) VALUES (:from, :to, :content)");
    query.bindValue(":from", from);
    query.bindValue(":to", to);
    query.bindValue(":content", finalContent);
    bool result = query.exec();

    // 释放数据库信号量
    m_dbSemaphore->post();

    // 将消息添加到共享内存，以便其他进程可以访问
    if (result) {
        QJsonObject msgObj;
        msgObj["from"] = from;
        msgObj["to"] = to;
        msgObj["content"] = finalContent;
        msgObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        QJsonDocument doc(msgObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

        // 写入共享内存
        m_sharedMemory->write(jsonData);

        // 将消息添加到消息队列，以便其他线程可以处理
        m_messageQueue->enqueue(jsonData);
    }

    return result;
}

QJsonArray Server::getChatHistory(const QString &user1, const QString &user2) {
    QJsonArray messages;

    // 使用读写锁保护聊天历史的读取
    ReadLocker locker(m_chatHistoryLock);
    if (!locker.isLocked()) {
        qDebug() << "Failed to acquire read lock for chat history";
        return messages;
    }

    // 使用数据库信号量控制数据库访问
    if (!m_dbSemaphore->wait()) {
        qDebug() << "Failed to acquire database semaphore";
        return messages;
    }

    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();

    // 确保数据库连接有效
    if (!threadDb.isOpen()) {
        qDebug() << "数据库连接未打开，尝试重新打开";
        if (!threadDb.open()) {
            qDebug() << "无法打开数据库连接:" << threadDb.lastError().text();
            m_dbSemaphore->post();
            return messages;
        }
    }

    QSqlQuery query(threadDb);
    query.prepare("SELECT from_nickname, content, timestamp FROM messages WHERE (from_nickname = :user1 AND to_nickname = :user2) OR (from_nickname = :user2 AND to_nickname = :user1) ORDER BY timestamp");
    query.bindValue(":user1", user1);
    query.bindValue(":user2", user2);
    if (query.exec()) {
        while (query.next()) {
            QJsonObject msg;
            msg["from"] = query.value("from_nickname").toString();

            // 获取content，可能是JSON字符串
            QString contentStr = query.value("content").toString();
            QJsonDocument contentDoc = QJsonDocument::fromJson(contentStr.toUtf8());

            if (!contentDoc.isNull() && contentDoc.isObject()) {
                // 如果是有效的JSON对象，直接使用
                msg["content"] = contentDoc.object();
            } else {
                // 如果不是有效的JSON，则将其包装为文本消息JSON
                QJsonObject textMsg;
                textMsg["type"] = "text";
                textMsg["text"] = contentStr;
                msg["content"] = textMsg;
            }

            // 添加时间戳
            msg["timestamp"] = query.value("timestamp").toString();

            messages.append(msg);
        }
    } else {
        qDebug() << "获取聊天历史失败:" << query.lastError().text();
    }

    // 释放数据库信号量
    m_dbSemaphore->post();

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
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

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
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

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
    if (!threadDb.transaction()) {
        qDebug() << "开始事务失败：" << threadDb.lastError().text();
        return false;
    }

    // 删除好友请求（不再是更新状态，而是直接删除）
    query.prepare("DELETE FROM friend_requests WHERE from_nickname = ? AND to_nickname = ?");
    query.addBindValue(from);
    query.addBindValue(to);
    if (!query.exec()) {
        qDebug() << "删除好友请求失败：" << query.lastError().text();
        threadDb.rollback();
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
        threadDb.rollback();
        return false;
    }

    qDebug() << "提交事务";
    if (!threadDb.commit()) {
        qDebug() << "提交事务失败：" << threadDb.lastError().text();
        threadDb.rollback();
        return false;
    }

    qDebug() << "成功处理好友请求：" << from << "和" << to << "已成为好友";
    return true;
}

bool Server::deleteFriend(const QString &user, const QString &friendName) {
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    query.prepare("DELETE FROM friends WHERE (user_nickname = ? AND friend_nickname = ?) OR (user_nickname = ? AND friend_nickname = ?)");
    query.addBindValue(user);
    query.addBindValue(friendName);
    query.addBindValue(friendName);
    query.addBindValue(user);
    return query.exec();
}

QStringList Server::getFriendRequests(const QString &user) {
    QStringList requests;

    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

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
    QMutexLocker locker(&m_clientsMutex);
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if (it->isLoggedIn && it->nickname == to) {
            qDebug() << "Sending friend request notification from" << from << "to" << to;
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, it->socket), Q_ARG(QByteArray, message));
            notified = true;
            break;
        }
    }

    return notified; // 返回是否成功通知对方
}

bool Server::deleteFriendRequest(const QString &from, const QString &to)
{
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

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

bool Server::createGroup(const QString &creator, const QString &groupName, const QStringList &members) {
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    // 开始事务
    if (!threadDb.transaction()) {
        qDebug() << "创建群聊事务开始失败：" << threadDb.lastError().text();
        return false;
    }

    // 创建群聊
    query.prepare("INSERT INTO groups (name, creator_nickname) VALUES (?, ?)");
    query.addBindValue(groupName);
    query.addBindValue(creator);

    if (!query.exec()) {
        qDebug() << "创建群聊失败：" << query.lastError().text();
        threadDb.rollback();
        return false;
    }

    // 获取新创建的群聊ID
    int groupId = query.lastInsertId().toInt();
    if (groupId <= 0) {
        qDebug() << "获取群聊ID失败";
        threadDb.rollback();
        return false;
    }

    // 添加创建者为群成员
    query.prepare("INSERT INTO group_members (group_id, member_nickname) VALUES (?, ?)");
    query.addBindValue(groupId);
    query.addBindValue(creator);

    if (!query.exec()) {
        qDebug() << "添加创建者到群成员失败：" << query.lastError().text();
        threadDb.rollback();
        return false;
    }

    // 添加其他成员
    bool allMembersAdded = true;
    for (const QString &member : members) {
        // 跳过创建者（已添加）
        if (member == creator) continue;

        query.prepare("INSERT INTO group_members (group_id, member_nickname) VALUES (?, ?)");
        query.addBindValue(groupId);
        query.addBindValue(member);

        if (!query.exec()) {
            qDebug() << "添加成员" << member << "到群聊失败：" << query.lastError().text();
            allMembersAdded = false;
            break;
        }

        // 通知在线成员已被加入群聊
        notifyGroupCreation(member, groupId, groupName, creator);
    }

    if (!allMembersAdded) {
        qDebug() << "添加群成员失败，回滚事务";
        threadDb.rollback();
        return false;
    }

    // 提交事务
    if (!threadDb.commit()) {
        qDebug() << "创建群聊提交事务失败：" << threadDb.lastError().text();
        threadDb.rollback();
        return false;
    }

    qDebug() << "成功创建群聊：" << groupName << "，ID：" << groupId << "，创建者：" << creator;
    return true;
}

QStringList Server::getGroupList(const QString &user) {
    QStringList groups;

    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    query.prepare("SELECT g.id, g.name FROM groups g "
                 "JOIN group_members gm ON g.id = gm.group_id "
                 "WHERE gm.member_nickname = ?");
    query.addBindValue(user);

    if (query.exec()) {
        while (query.next()) {
            QString groupInfo = QString("%1:%2").arg(query.value("id").toString(), query.value("name").toString());
            groups << groupInfo;
        }
    } else {
        qDebug() << "获取用户群聊列表失败：" << query.lastError().text();
    }

    return groups;
}

QStringList Server::getGroupMembers(int groupId) {
    QStringList members;

    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    query.prepare("SELECT member_nickname FROM group_members WHERE group_id = ?");
    query.addBindValue(groupId);

    if (query.exec()) {
        while (query.next()) {
            members << query.value("member_nickname").toString();
        }
    } else {
        qDebug() << "获取群成员列表失败：" << query.lastError().text();
    }

    return members;
}

bool Server::saveGroupChatMessage(int groupId, const QString &from, const QString &content) {
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    // 检查content是否已经是JSON格式
    QJsonDocument contentDoc = QJsonDocument::fromJson(content.toUtf8());
    QString finalContent;

    if (contentDoc.isNull() || !contentDoc.isObject()) {
        // 如果不是有效的JSON，则将其包装为文本消息JSON
        QJsonObject textMsg;
        textMsg["type"] = "text";
        textMsg["text"] = content;
        finalContent = QJsonDocument(textMsg).toJson(QJsonDocument::Compact);
    } else {
        // 已经是JSON格式，直接使用
        finalContent = content;
    }

    query.prepare("INSERT INTO group_messages (group_id, from_nickname, content) VALUES (?, ?, ?)");
    query.addBindValue(groupId);
    query.addBindValue(from);
    query.addBindValue(finalContent);

    if (!query.exec()) {
        qDebug() << "保存群聊消息失败：" << query.lastError().text();
        return false;
    }

    return true;
}

QJsonArray Server::getGroupChatHistory(int groupId) {
    QJsonArray messages;

    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();

    // 确保数据库连接有效
    if (!threadDb.isOpen()) {
        qDebug() << "数据库连接未打开，尝试重新打开";
        if (!threadDb.open()) {
            qDebug() << "无法打开数据库连接:" << threadDb.lastError().text();
            return messages;
        }
    }

    QSqlQuery query(threadDb);

    query.prepare("SELECT from_nickname, content, timestamp FROM group_messages "
                 "WHERE group_id = ? ORDER BY timestamp");
    query.addBindValue(groupId);

    if (query.exec()) {
        while (query.next()) {
            QJsonObject msg;
            msg["from"] = query.value("from_nickname").toString();

            // 获取content，可能是JSON字符串
            QString contentStr = query.value("content").toString();
            QJsonDocument contentDoc = QJsonDocument::fromJson(contentStr.toUtf8());

            if (!contentDoc.isNull() && contentDoc.isObject()) {
                // 如果是有效的JSON对象，直接使用
                msg["content"] = contentDoc.object();
            } else {
                // 如果不是有效的JSON，则将其包装为文本消息JSON
                QJsonObject textMsg;
                textMsg["type"] = "text";
                textMsg["text"] = contentStr;
                msg["content"] = textMsg;
            }

            msg["timestamp"] = query.value("timestamp").toString();
            messages.append(msg);
        }
    } else {
        qDebug() << "获取群聊历史记录失败：" << query.lastError().text();
    }

    return messages;
}

bool Server::notifyGroupMessage(int groupId, const QString &from, const QString &content) {
    // 获取群成员
    QStringList members = getGroupMembers(groupId);
    bool anyNotified = false;

    // 检查content是否已经是JSON格式
    QJsonDocument contentDoc = QJsonDocument::fromJson(content.toUtf8());
    QJsonValue contentValue;

    if (!contentDoc.isNull() && contentDoc.isObject()) {
        // 如果是有效的JSON对象，直接使用
        contentValue = contentDoc.object();
    } else {
        // 如果不是有效的JSON，则将其包装为文本消息JSON
        QJsonObject textMsg;
        textMsg["type"] = "text";
        textMsg["text"] = content;
        contentValue = textMsg;
    }

    QJsonObject msgData;
    msgData["group_id"] = groupId;
    msgData["from"] = from;
    msgData["content"] = contentValue;

    QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::GroupChat, msgData)).toJson();

    // 遍历所有在线客户端，发送消息给在线群成员
    QMutexLocker locker(&m_clientsMutex);
    for (const ClientInfo &client : clients) {
        if (client.isLoggedIn && members.contains(client.nickname) && client.nickname != from) {
            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, client.socket), Q_ARG(QByteArray, message));
            anyNotified = true;
        }
    }

    return anyNotified;
}

bool Server::notifyGroupCreation(const QString &member, int groupId, const QString &groupName, const QString &creator) {
    // 查找在线的群成员
    bool notified = false;

    QMutexLocker locker(&m_clientsMutex);
    for (const ClientInfo &client : clients) {
        if (client.isLoggedIn && client.nickname == member) {
            QJsonObject notification;
            notification["group_id"] = groupId;
            notification["group_name"] = groupName;
            notification["creator"] = creator;

            QByteArray message = QJsonDocument(MessageProtocol::createMessage(
                MessageType::CreateGroup, notification)).toJson();

            QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                     Q_ARG(QTcpSocket*, client.socket), Q_ARG(QByteArray, message));
            qDebug() << "已通知用户" << member << "被加入群聊" << groupName;
            notified = true;
        }
    }

    return notified;
}

QJsonObject Server::getUserProfile(const QString &nickname) {
    QJsonObject profile;

    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    query.prepare("SELECT email, nickname, signature, avatar, gender, birthday, location, phone, "
                 "register_time, last_login_time, friend_count, group_count "
                 "FROM users WHERE nickname = ?");
    query.addBindValue(nickname);

    if (query.exec() && query.next()) {
        profile["email"] = query.value("email").toString();
        profile["nickname"] = query.value("nickname").toString();
        profile["signature"] = query.value("signature").toString();
        profile["avatar"] = query.value("avatar").toString();
        profile["gender"] = query.value("gender").toString();
        profile["birthday"] = query.value("birthday").toString();
        profile["location"] = query.value("location").toString();
        profile["phone"] = query.value("phone").toString();
        profile["register_time"] = query.value("register_time").toString();
        profile["last_login_time"] = query.value("last_login_time").toString();
        profile["friend_count"] = query.value("friend_count").toInt();
        profile["group_count"] = query.value("group_count").toInt();
    } else {
        qDebug() << "获取用户资料失败：" << query.lastError().text();
    }

    return profile;
}

bool Server::updateUserProfile(const QString &nickname, const QJsonObject &profileData) {
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    // 构建更新语句
    QString updateQuery = "UPDATE users SET ";
    QStringList updateFields;
    QVariantList values;

    // 检查并添加各个字段
    if (profileData.contains("signature")) {
        updateFields << "signature = ?";
        values << profileData["signature"].toString();
    }

    if (profileData.contains("gender")) {
        updateFields << "gender = ?";
        values << profileData["gender"].toString();
    }

    if (profileData.contains("birthday")) {
        updateFields << "birthday = ?";
        values << profileData["birthday"].toString();
    }

    if (profileData.contains("location")) {
        updateFields << "location = ?";
        values << profileData["location"].toString();
    }

    if (profileData.contains("phone")) {
        updateFields << "phone = ?";
        values << profileData["phone"].toString();
    }

    // 如果没有要更新的字段，直接返回成功
    if (updateFields.isEmpty()) {
        return true;
    }

    // 完成更新语句
    updateQuery += updateFields.join(", ") + " WHERE nickname = ?";
    values << nickname;

    // 准备并执行查询
    query.prepare(updateQuery);
    for (const QVariant &value : values) {
        query.addBindValue(value);
    }

    if (!query.exec()) {
        qDebug() << "更新用户资料失败：" << query.lastError().text();
        return false;
    }

    return true;
}

bool Server::saveAvatar(const QString &nickname, const QByteArray &avatarData) {
    // 创建用户头像目录
    QString avatarDir = QCoreApplication::applicationDirPath() + "/../avatars/";
    QDir dir(avatarDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 生成头像文件名（使用用户昵称作为文件名）
    QString avatarFileName = nickname + ".png";
    QString avatarPath = avatarDir + avatarFileName;

    // 保存头像文件
    QFile file(avatarPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "无法打开头像文件进行写入：" << file.errorString();
        return false;
    }

    if (file.write(avatarData) == -1) {
        qDebug() << "写入头像文件失败：" << file.errorString();
        file.close();
        return false;
    }

    file.close();

    // 更新数据库中的头像路径
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    query.prepare("UPDATE users SET avatar = ? WHERE nickname = ?");
    query.addBindValue(avatarFileName);
    query.addBindValue(nickname);

    if (!query.exec()) {
        qDebug() << "更新用户头像路径失败：" << query.lastError().text();
        return false;
    }

    return true;
}

QByteArray Server::getAvatar(const QString &nickname) {
    // 首先从数据库获取头像文件名
    // 获取线程本地数据库连接
    QSqlDatabase threadDb = getThreadLocalDatabase();
    QSqlQuery query(threadDb);

    query.prepare("SELECT avatar FROM users WHERE nickname = ?");
    query.addBindValue(nickname);

    if (!query.exec() || !query.next()) {
        qDebug() << "获取用户头像信息失败：" << query.lastError().text();
        return QByteArray();
    }

    QString avatarFileName = query.value("avatar").toString();
    if (avatarFileName.isEmpty()) {
        qDebug() << "用户" << nickname << "没有设置头像";
        return QByteArray();
    }

    // 读取头像文件
    QString avatarPath = QCoreApplication::applicationDirPath() + "/../avatars/" + avatarFileName;
    QFile file(avatarPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开头像文件：" << file.errorString();
        return QByteArray();
    }

    QByteArray avatarData = file.readAll();
    file.close();

    return avatarData;
}

QString Server::generateUniqueImageId(const QString &fileExtension) {
    // 生成唯一的图片ID，使用UUID加上原始扩展名
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    return uuid + "." + fileExtension;
}

bool Server::saveImage(const QString &imageId, const QByteArray &imageData) {
    // 确保图片存储目录存在
    QDir dir(m_imageStoragePath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qDebug() << "Error: Failed to create image storage directory:" << m_imageStoragePath;
            return false;
        }
    }

    // 保存图片文件
    QString imagePath = m_imageStoragePath + imageId;
    QFile file(imagePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Error: Failed to open image file for writing:" << file.errorString();
        return false;
    }

    if (file.write(imageData) == -1) {
        qDebug() << "Error: Failed to write image data:" << file.errorString();
        file.close();
        return false;
    }

    file.close();
    qDebug() << "Image saved successfully:" << imagePath;
    return true;
}

QByteArray Server::getImage(const QString &imageId) {
    // 读取图片文件
    QString imagePath = m_imageStoragePath + imageId;
    QFile file(imagePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        qDebug() << "Error: Failed to open image file for reading:" << file.errorString();
        return QByteArray();
    }

    QByteArray imageData = file.readAll();
    file.close();
    return imageData;
}

// 处理分块图片上传开始请求
void Server::handleChunkedImageStart(QTcpSocket *clientSocket, const QJsonObject &msgData, ClientInfo *clientInfo) {
    QString tempId = msgData["temp_id"].toString();
    QString fileExtension = msgData["file_extension"].toString();
    int totalChunks = msgData["total_chunks"].toInt();
    int totalSize = msgData["total_size"].toInt();
    int width = msgData["width"].toInt();
    int height = msgData["height"].toInt();

    qDebug() << "开始接收分块图片上传，临时ID:" << tempId
             << "，总块数:" << totalChunks
             << "，总大小:" << totalSize << "字节";

    // 验证参数
    if (tempId.isEmpty() || fileExtension.isEmpty() || totalChunks <= 0 || totalSize <= 0) {
        QJsonObject response;
        response["status"] = "failed";
        response["reason"] = "Invalid parameters";
        response["temp_id"] = tempId;

        QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
            MessageType::ChunkedImageResponse, response)).toJson();
        QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                 Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        return;
    }

    // 创建新的分块图片数据结构
    ChunkedImageData chunkedData;
    chunkedData.tempId = tempId;
    chunkedData.fileExtension = fileExtension;
    chunkedData.totalChunks = totalChunks;
    chunkedData.receivedChunks = 0;
    chunkedData.imageData.reserve(totalSize); // 预分配内存
    chunkedData.width = width;
    chunkedData.height = height;

    // 存储到待处理映射中
    m_pendingChunkedImages[tempId] = chunkedData;

    qDebug() << "分块图片上传初始化成功，等待接收数据块";
}

// 处理分块图片上传数据块
void Server::handleChunkedImageChunk(QTcpSocket *clientSocket, const QJsonObject &msgData, ClientInfo *clientInfo) {
    QString tempId = msgData["temp_id"].toString();
    int chunkIndex = msgData["chunk_index"].toInt();
    QString chunkDataBase64 = msgData["chunk_data"].toString();

    // 验证参数
    if (tempId.isEmpty() || chunkDataBase64.isEmpty() || chunkIndex < 0) {
        QJsonObject response;
        response["status"] = "failed";
        response["reason"] = "Invalid chunk data";
        response["temp_id"] = tempId;

        QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
            MessageType::ChunkedImageResponse, response)).toJson();
        QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                 Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        return;
    }

    // 检查是否存在对应的分块上传记录
    if (!m_pendingChunkedImages.contains(tempId)) {
        QJsonObject response;
        response["status"] = "failed";
        response["reason"] = "No active upload found for this ID";
        response["temp_id"] = tempId;

        QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
            MessageType::ChunkedImageResponse, response)).toJson();
        QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                 Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        return;
    }

    // 获取分块上传记录
    ChunkedImageData &chunkedData = m_pendingChunkedImages[tempId];

    // 解码数据块
    QByteArray chunkData = QByteArray::fromBase64(chunkDataBase64.toLatin1());

    // 添加到图片数据中
    chunkedData.imageData.append(chunkData);
    chunkedData.receivedChunks++;

    qDebug() << "接收到图片数据块:" << chunkIndex + 1 << "/" << chunkedData.totalChunks
             << "，大小:" << chunkData.size() << "字节";
}

// 处理分块图片上传结束请求
void Server::handleChunkedImageEnd(QTcpSocket *clientSocket, const QJsonObject &msgData, ClientInfo *clientInfo) {
    QString tempId = msgData["temp_id"].toString();

    // 验证参数
    if (tempId.isEmpty()) {
        QJsonObject response;
        response["status"] = "failed";
        response["reason"] = "Invalid parameters";
        response["temp_id"] = tempId;

        QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
            MessageType::ChunkedImageResponse, response)).toJson();
        QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                 Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        return;
    }

    // 检查是否存在对应的分块上传记录
    if (!m_pendingChunkedImages.contains(tempId)) {
        QJsonObject response;
        response["status"] = "failed";
        response["reason"] = "No active upload found for this ID";
        response["temp_id"] = tempId;

        QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
            MessageType::ChunkedImageResponse, response)).toJson();
        QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                 Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));
        return;
    }

    // 获取分块上传记录
    ChunkedImageData chunkedData = m_pendingChunkedImages.take(tempId);

    // 检查是否接收到足够的数据块
    // 允许一定的容错，只要接收到了大部分数据块就认为是成功的
    if (chunkedData.receivedChunks < chunkedData.totalChunks * 0.9) { // 至少接收到90%的数据块
        QJsonObject response;
        response["status"] = "failed";
        response["reason"] = QString("Incomplete upload: received %1 of %2 chunks")
                            .arg(chunkedData.receivedChunks)
                            .arg(chunkedData.totalChunks);
        response["temp_id"] = tempId;

        QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
            MessageType::ChunkedImageResponse, response)).toJson();
        QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                 Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));

        qDebug() << "分块图片上传失败，接收到的块数不足:" << chunkedData.receivedChunks
                 << "/" << chunkedData.totalChunks;
        return;
    }

    // 如果接收到的块数不等于总块数，但超过了90%，记录日志但继续处理
    if (chunkedData.receivedChunks != chunkedData.totalChunks) {
        qDebug() << "警告：接收到的块数与总块数不一致，但已超过90%，继续处理:"
                 << chunkedData.receivedChunks << "/" << chunkedData.totalChunks;
    }

    // 生成唯一的图片ID
    QString imageId = generateUniqueImageId(chunkedData.fileExtension);

    // 保存图片
    if (saveImage(imageId, chunkedData.imageData)) {
        QJsonObject response;
        response["status"] = "success";
        response["image_id"] = imageId;
        response["temp_id"] = tempId;

        QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
            MessageType::ChunkedImageResponse, response)).toJson();
        QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                 Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));

        qDebug() << "分块图片上传成功，ID:" << imageId
                 << "，临时ID:" << tempId
                 << "，总大小:" << chunkedData.imageData.size() << "字节";
    } else {
        QJsonObject response;
        response["status"] = "failed";
        response["reason"] = "Failed to save image";
        response["temp_id"] = tempId;

        QByteArray responseData = QJsonDocument(MessageProtocol::createMessage(
            MessageType::ChunkedImageResponse, response)).toJson();
        QMetaObject::invokeMethod(this, "sendResponseToClient", Qt::QueuedConnection,
                                 Q_ARG(QTcpSocket*, clientSocket), Q_ARG(QByteArray, responseData));

        qDebug() << "分块图片上传失败，临时ID:" << tempId;
    }
}