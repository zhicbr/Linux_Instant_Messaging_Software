#include "chatwindow.h"
#include <QJsonDocument>
#include <QDebug>
#include <QDateTime>
#include "../Common/config.h"
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QFileInfo>
#include <QTime>
#include <QImage>
#include <QUuid>

ChatWindow::ChatWindow(QObject *parent)
    : QObject(parent), m_socket(new QTcpSocket(this)), m_isLoggedIn(false)
{
    // 初始化网络连接
    m_socket->connectToHost(Config::DefaultServerIp, Config::DefaultPort);
    connect(m_socket, &QTcpSocket::readyRead, this, &ChatWindow::handleServerData);
    connect(m_socket, &QTcpSocket::connected, this, &ChatWindow::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &ChatWindow::onDisconnected);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &ChatWindow::onSocketError);

    // 从设置中恢复头像缓存信息
    QSettings settings;
    settings.beginGroup("AvatarCache");
    QStringList keys = settings.childKeys();
    for (const QString &nickname : keys) {
        QString path = settings.value(nickname).toString();
        // 确认文件存在
        if (QFile::exists(path)) {
            m_avatarCache[nickname] = path;
        }
    }
    settings.endGroup();

    // 初始化图片缓存
    initImageCache();
}

ChatWindow::~ChatWindow()
{
    // 保存图片缓存设置
    saveImageCacheSettings();
}

void ChatWindow::initImageCache()
{
    // 创建图片缓存目录
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_imageCachePath = appDataPath + "/image_cache/";
    QDir dir(m_imageCachePath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 从设置中恢复图片缓存信息
    QSettings settings;
    settings.beginGroup("ImageCache");
    QStringList keys = settings.childKeys();
    for (const QString &imageId : keys) {
        QString path = settings.value(imageId).toString();
        // 确认文件存在
        if (QFile::exists(path)) {
            m_imageCacheMap[imageId] = path;
        }
    }
    settings.endGroup();

    qDebug() << "图片缓存目录：" << m_imageCachePath;
    qDebug() << "已缓存图片数量：" << m_imageCacheMap.size();
}

void ChatWindow::saveImageCacheSettings()
{
    QSettings settings;
    settings.beginGroup("ImageCache");
    settings.remove(""); // 清除所有旧设置

    // 保存当前缓存映射
    for (auto it = m_imageCacheMap.constBegin(); it != m_imageCacheMap.constEnd(); ++it) {
        settings.setValue(it.key(), it.value());
    }

    settings.endGroup();
    settings.sync();
}

void ChatWindow::onConnected()
{
    qDebug() << "已连接到服务器。";
    emit statusMessage("已连接");
}

void ChatWindow::onDisconnected()
{
    qDebug() << "与服务器断开连接。";
    m_isLoggedIn = false;
    emit isLoggedInChanged();
    emit statusMessage("已断开连接");
}

void ChatWindow::onSocketError(QAbstractSocket::SocketError socketError)
{
    qWarning() << "套接字错误：" << socketError << m_socket->errorString();
    emit statusMessage("连接失败：" + m_socket->errorString());
}

void ChatWindow::login(const QString &nickname, const QString &password)
{
    if (nickname.isEmpty() || password.isEmpty()) {
        emit statusMessage("请输入用户名和密码。");
        return;
    }
    QJsonObject data{
        {"nickname", nickname},
        {"password", password}
    };
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Login, data)).toJson());
        m_currentNickname = nickname; // 临时存储，成功后确认
    } else {
        emit statusMessage("未连接到服务器。");
    }
}

void ChatWindow::registerUser(const QString &email, const QString &nickname, const QString &password)
{
    if (nickname.isEmpty() || password.isEmpty() || email.isEmpty() || !email.contains('@')) {
        emit statusMessage("请正确填写所有字段（包括有效的电子邮件）。");
        return;
    }
    QJsonObject data{
        {"email", email},
        {"nickname", nickname},
        {"password", password}
    };
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Register, data)).toJson());
    } else {
        emit statusMessage("未连接到服务器。");
    }
}

void ChatWindow::sendMessage(const QString &content)
{
    if (!m_isLoggedIn) {
        emit statusMessage("请先登录以发送消息。");
        return;
    }
    if (m_currentChatFriend.isEmpty()) {
        emit statusMessage("请选择一个好友进行聊天。");
        return;
    }
    QString message = content.trimmed();
    if (message.isEmpty()) {
        return;
    }

    // 创建文本消息的JSON内容
    QJsonObject contentJson;
    contentJson["type"] = "text";
    contentJson["text"] = message;

    // 发送消息
    sendMessageInternal(contentJson);
}

void ChatWindow::selectFriend(const QString &friendName)
{
    if (friendName != m_currentChatFriend || m_isGroupChat) {
        qDebug() << "选择好友聊天:" << friendName << "，当前是否为群聊:" << m_isGroupChat;

        // 清除群聊状态
        m_currentChatGroup.clear();
        m_isGroupChat = false;
        qDebug() << "设置isGroupChat为false，当前值:" << m_isGroupChat;
        emit isGroupChatChanged();
        emit currentChatGroupChanged();

        // 设置一对一聊天状态
        m_currentChatFriend = friendName;
        emit currentChatFriendChanged();
        clearChatDisplay();
        loadChatHistory(friendName);
    }
}

void ChatWindow::searchUser(const QString &query)
{
    if (query.isEmpty()) {
        emit statusMessage("请输入搜索查询。");
        return;
    }
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(QJsonDocument(MessageProtocol::createMessage(
            MessageType::SearchUser, {{"query", query}})).toJson());
    } else {
        emit statusMessage("未连接到服务器。");
    }
}

void ChatWindow::sendFriendRequest(const QString &friendName)
{
    if (!m_isLoggedIn) {
        emit statusMessage("请先登录。");
        return;
    }
    if (friendName.isEmpty()) {
        emit statusMessage("请输入要添加的好友昵称。");
        return;
    }
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(QJsonDocument(MessageProtocol::createMessage(
            MessageType::FriendRequest, {{"to", friendName}})).toJson());
    } else {
        emit statusMessage("未连接到服务器。");
    }
}

void ChatWindow::acceptFriendRequest(const QString &friendName)
{
    if (!m_isLoggedIn) {
        qDebug() << "尝试接受好友请求失败：未登录";
        emit statusMessage("请先登录。");
        return;
    }

    if (friendName.isEmpty()) {
        qDebug() << "好友名为空，无法接受请求";
        return;
    }

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "发送接受好友请求：" << friendName;

        // 创建消息对象
        QJsonObject data;
        data["from"] = friendName;

        // 创建完整消息
        QJsonObject message = MessageProtocol::createMessage(MessageType::AcceptFriend, data);

        // 转换为 JSON 并发送
        QByteArray jsonData = QJsonDocument(message).toJson();
        qDebug() << "发送的消息：" << jsonData;

        m_socket->write(jsonData);
        m_socket->flush();
        emit statusMessage("正在处理好友请求...");
    } else {
        qDebug() << "尝试接受好友请求失败：未连接到服务器";
        emit statusMessage("未连接到服务器。");
    }
}

void ChatWindow::deleteFriend(const QString &friendName)
{
    if (!m_isLoggedIn) {
        emit statusMessage("请先登录。");
        return;
    }
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(QJsonDocument(MessageProtocol::createMessage(
            MessageType::DeleteFriend, {{"friend", friendName}})).toJson());
    } else {
        emit statusMessage("未连接到服务器。");
    }
}

void ChatWindow::deleteFriendRequest(const QString &friendName)
{
    if (!m_isLoggedIn) {
        qDebug() << "尝试删除好友请求失败：未登录";
        emit statusMessage("请先登录。");
        return;
    }

    if (friendName.isEmpty()) {
        qDebug() << "好友名为空，无法删除请求";
        return;
    }

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "发送删除好友请求：" << friendName;

        // 创建消息对象
        QJsonObject data;
        data["from"] = friendName;

        // 创建完整消息
        QJsonObject message = MessageProtocol::createMessage(MessageType::DeleteFriendRequest, data);

        // 转换为 JSON 并发送
        QByteArray jsonData = QJsonDocument(message).toJson();
        qDebug() << "发送的消息：" << jsonData;

        m_socket->write(jsonData);
        m_socket->flush();
        emit statusMessage("正在删除好友请求...");
    } else {
        qDebug() << "尝试删除好友请求失败：未连接到服务器";
        emit statusMessage("未连接到服务器。");
    }
}

void ChatWindow::appendMessage(const QString &sender, const QString &content, const QString &timestamp)
{
    QString avatarPath = "";

    // 如果是自己的消息，使用自己的头像
    if (sender == m_currentNickname) {
        avatarPath = getCachedAvatarPath(m_currentNickname);
    } else {
        // 获取好友头像路径
        avatarPath = getCachedAvatarPath(sender);
    }

    QVariantMap message;
    message["sender"] = sender;
    message["content"] = content;
    message["timestamp"] = timestamp.isEmpty() ? QTime::currentTime().toString("hh:mm") : timestamp;
    message["avatarSource"] = avatarPath;

    emit messageReceived(message["sender"].toString(),
                         message["content"].toString(),
                         message["timestamp"].toString(),
                         message["avatarSource"].toString());
}

void ChatWindow::clearChatDisplay()
{
    emit chatDisplayCleared();
}

void ChatWindow::setStatusMessage(const QString &message)
{
    emit statusMessage(message);
}

void ChatWindow::updateFriendList(const QStringList &friends)
{
    m_friendList = friends;
    emit friendListUpdated(friends);
}

void ChatWindow::updateFriendRequests(const QStringList &requests)
{
    qDebug() << "执行updateFriendRequests，新列表:" << requests;
    m_friendRequests = requests;
    emit friendRequestsChanged();
    qDebug() << "好友请求信号已发出";
}

void ChatWindow::refreshFriendRequests()
{
    if (!m_isLoggedIn) {
        qDebug() << "未登录，不刷新好友请求";
        return;
    }
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "发送获取好友请求列表的请求";
        QJsonObject emptyData;
        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::FriendRequestList, emptyData)).toJson();
        qDebug() << "原始请求数据: " << request;
        m_socket->write(request);
        m_socket->flush(); // 确保消息立即发送
    } else {
        qDebug() << "Socket未连接，无法刷新好友请求";
    }
}

void ChatWindow::setIsLoggedIn(bool loggedIn) {
    if (m_isLoggedIn != loggedIn) {
        m_isLoggedIn = loggedIn;
        emit isLoggedInChanged();
        if (loggedIn) {
            refreshFriendRequests();
        }
    }
}

void ChatWindow::logout() {
    if (!m_isLoggedIn) return;

    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject obj;
        obj["type"] = static_cast<int>(MessageType::Logout);
        obj["nickname"] = m_currentNickname;
        QJsonDocument doc(obj);
        m_socket->write(doc.toJson());
    }

    m_isLoggedIn = false;
    m_currentNickname.clear();
    m_currentChatFriend.clear();
    m_friendList.clear();
    m_friendRequests.clear();
    m_friendOnlineStatus.clear();

    emit isLoggedInChanged();
    emit currentNicknameChanged();
    emit currentChatFriendChanged();
    emit friendListUpdated(m_friendList);
    emit friendRequestsChanged();
    emit friendOnlineStatusChanged();
    emit statusMessage("已登出");
}

void ChatWindow::handleServerData()
{
    if (m_socket->bytesAvailable() <= 0) return;

    QByteArray data = m_socket->readAll();

    // 检查是否是二进制图片数据
    if (data.size() > 16) { // 至少需要16字节来检查魔数和消息类型
        // 打印前几个字节用于调试
        QString hexData;
        for (int i = 0; i < qMin(32, data.size()); ++i) {
            hexData += QString("%1 ").arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0'));
        }
        qDebug() << "数据前32字节:" << hexData;
        qDebug() << "数据总大小:" << data.size() << "字节";

        // 检查魔数 "IMGD"
        if (data.startsWith("IMGD")) {
            qDebug() << "检测到图片数据魔数 IMGD";

            // 读取消息类型 (4字节)
            if (data.size() < 8) {
                qDebug() << "数据太短，无法读取消息类型";
                return;
            }

            qint32 messageType = 0;
            memcpy(&messageType, data.constData() + 4, sizeof(qint32));

            qDebug() << "读取到消息类型:" << messageType << "，期望类型:" << static_cast<qint32>(MessageType::BinaryImageData);

            if (messageType == static_cast<qint32>(MessageType::BinaryImageData)) {
                qDebug() << "收到二进制图片数据，大小:" << data.size() << "字节";

                // 读取图片ID长度 (4字节)
                if (data.size() < 12) {
                    qDebug() << "数据太短，无法读取图片ID长度";
                    return;
                }

                qint32 imageIdLength = 0;
                memcpy(&imageIdLength, data.constData() + 8, sizeof(qint32));
                qDebug() << "图片ID长度:" << imageIdLength << "字节";

                if (imageIdLength <= 0 || imageIdLength >= 100) {
                    qDebug() << "图片ID长度异常:" << imageIdLength;
                    return;
                }

                // 读取图片ID
                if (data.size() < 12 + imageIdLength) {
                    qDebug() << "数据太短，无法读取图片ID";
                    return;
                }

                QByteArray imageIdBytes = data.mid(12, imageIdLength);
                QString imageId = QString::fromUtf8(imageIdBytes);
                qDebug() << "读取到图片ID:" << imageId;

                // 读取图片数据长度 (4字节)
                int imageDataLengthPos = 12 + imageIdLength;
                if (data.size() < imageDataLengthPos + 4) {
                    qDebug() << "数据太短，无法读取图片数据长度";
                    return;
                }

                qint32 imageDataLength = 0;
                memcpy(&imageDataLength, data.constData() + imageDataLengthPos, sizeof(qint32));
                qDebug() << "图片数据长度:" << imageDataLength << "字节";

                if (imageDataLength <= 0) {
                    qDebug() << "图片数据长度异常:" << imageDataLength;
                    return;
                }

                // 读取图片数据
                int imageDataPos = imageDataLengthPos + 4;
                if (data.size() < imageDataPos) {
                    qDebug() << "数据太短，无法读取图片数据";
                    return;
                }

                QByteArray imageData;
                if (data.size() >= imageDataPos + imageDataLength) {
                    imageData = data.mid(imageDataPos, imageDataLength);
                } else {
                    // 如果数据不够，读取所有剩余数据
                    imageData = data.mid(imageDataPos);
                    qDebug() << "警告：实际数据小于预期，读取所有剩余数据:" << imageData.size() << "字节";
                }

                qDebug() << "实际读取的图片数据大小:" << imageData.size() << "字节";

                // 检查数据是否有效
                if (imageId.isEmpty() || imageData.isEmpty()) {
                    qDebug() << "二进制图片数据无效，图片ID为空或数据为空";
                    return;
                }

                qDebug() << "解析二进制图片数据成功，ID:" << imageId << "，大小:" << imageData.size() << "字节";

                // 保存图片到本地缓存
                QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
                QString imageCacheDir = appDataPath + "/image_cache/";
                QDir dir(imageCacheDir);
                if (!dir.exists()) {
                    bool created = dir.mkpath(".");
                    qDebug() << "创建图片缓存目录结果:" << created << ", 路径:" << dir.absolutePath();
                }

                // 创建缓存文件路径
                QString cachedPath = imageCacheDir + imageId;

                // 保存文件
                QFile file(cachedPath);
                if (file.open(QIODevice::WriteOnly)) {
                    qint64 bytesWritten = file.write(imageData);
                    file.close();

                    if (bytesWritten == imageData.size()) {
                        qDebug() << "二进制图片保存成功:" << cachedPath;

                        // 验证保存的文件
                        QImage testImage(cachedPath);
                        if (testImage.isNull()) {
                            qDebug() << "警告：保存的图片文件无法加载为有效图像";

                            // 尝试直接保存原始二进制数据
                            QFile rawFile(cachedPath + ".raw");
                            if (rawFile.open(QIODevice::WriteOnly)) {
                                rawFile.write(data);
                                rawFile.close();
                                qDebug() << "已保存原始二进制数据到:" << cachedPath + ".raw";
                            }
                        } else {
                            qDebug() << "验证图片文件成功，尺寸:" << testImage.width() << "x" << testImage.height();
                        }

                        // 更新缓存映射
                        m_imageCacheMap[imageId] = cachedPath;

                        // 保存到设置
                        QSettings settings;
                        settings.beginGroup("ImageCache");
                        settings.setValue(imageId, cachedPath);
                        settings.endGroup();
                        settings.sync();

                        // 更新所有显示该图片的消息
                        updateImageMessages(imageId, cachedPath);

                        return;
                    }
                }

                qDebug() << "无法保存二进制图片:" << file.errorString();

                // 如果保存失败，尝试重新请求图片
                downloadImage(imageId);
                return;
            }
        }
    }

    // 尝试解析JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON解析错误:" << parseError.errorString() << "，位置:" << parseError.offset;
        qDebug() << "数据大小:" << data.size() << "字节";

        // 尝试检查是否是DownloadImageResponse消息
        if (data.contains("\"type\":28") || data.contains("\"type\": 28")) {
            qDebug() << "检测到可能是DownloadImageResponse消息，尝试手动解析";

            // 检查是否包含binary_mode标记
            if (data.contains("\"binary_mode\":true") || data.contains("\"binary_mode\": true")) {
                qDebug() << "检测到二进制模式标记，等待后续二进制数据";
                return;
            }

            // 更健壮的解析方法
            // 首先找到type字段确认这是一个DownloadImageResponse
            int typePos = data.indexOf("\"type\"");
            if (typePos > 0) {
                // 查找冒号后面的数字
                int typeValueStart = data.indexOf(":", typePos) + 1;
                while (typeValueStart < data.size() && (data[typeValueStart] == ' ' || data[typeValueStart] == '\t')) {
                    typeValueStart++;
                }

                int typeValueEnd = typeValueStart;
                while (typeValueEnd < data.size() && data[typeValueEnd] >= '0' && data[typeValueEnd] <= '9') {
                    typeValueEnd++;
                }

                bool ok;
                int typeValue = data.mid(typeValueStart, typeValueEnd - typeValueStart).toInt(&ok);

                if (ok && typeValue == 28) {
                    qDebug() << "确认是DownloadImageResponse消息，继续解析";

                    // 查找data字段
                    int dataPos = data.indexOf("\"data\"");
                    if (dataPos > 0) {
                        // 查找data字段的开始大括号
                        int dataStart = data.indexOf("{", dataPos);
                        if (dataStart > 0) {
                            // 查找匹配的结束大括号
                            int braceCount = 1;
                            int dataEnd = dataStart + 1;

                            while (dataEnd < data.size() && braceCount > 0) {
                                if (data[dataEnd] == '{') braceCount++;
                                else if (data[dataEnd] == '}') braceCount--;
                                dataEnd++;
                            }

                            if (braceCount == 0) {
                                // 提取data对象的JSON
                                QByteArray dataJson = data.mid(dataStart, dataEnd - dataStart);
                                QJsonParseError dataParseError;
                                QJsonDocument dataDoc = QJsonDocument::fromJson(dataJson, &dataParseError);

                                if (dataParseError.error == QJsonParseError::NoError) {
                                    QJsonObject dataObj = dataDoc.object();

                                    // 提取imageId和status
                                    QString imageId = dataObj["imageId"].toString();
                                    QString status = dataObj["status"].toString();

                                    if (!imageId.isEmpty() && status == "success") {
                                        // 提取image_data_base64
                                        QString imageDataBase64 = dataObj["image_data_base64"].toString();

                                        if (!imageDataBase64.isEmpty()) {
                                            // 解码Base64数据
                                            QByteArray imageData = QByteArray::fromBase64(imageDataBase64.toLatin1());

                                            qDebug() << "成功解析DownloadImageResponse，图片ID:" << imageId
                                                     << "，数据大小:" << imageData.size() << "字节";

                                            // 调用处理函数
                                            handleDownloadImageResponse(dataObj);
                                            return;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 如果上面的解析失败，尝试更简单的方法
            // 尝试提取imageId
            int imageIdPos = data.indexOf("\"imageId\"");
            if (imageIdPos > 0) {
                int imageIdStart = data.indexOf("\"", imageIdPos + 10) + 1;
                int imageIdEnd = data.indexOf("\"", imageIdStart);

                if (imageIdStart > 0 && imageIdEnd > imageIdStart) {
                    QString imageId = QString::fromUtf8(data.mid(imageIdStart, imageIdEnd - imageIdStart));

                    qDebug() << "提取到图片ID:" << imageId;

                    // 尝试提取status
                    int statusPos = data.indexOf("\"status\"");
                    if (statusPos > 0) {
                        int statusStart = data.indexOf("\"", statusPos + 10) + 1;
                        int statusEnd = data.indexOf("\"", statusStart);

                        if (statusStart > 0 && statusEnd > statusStart) {
                            QString status = QString::fromUtf8(data.mid(statusStart, statusEnd - statusStart));

                            qDebug() << "提取到状态:" << status;

                            if (status == "success") {
                                // 尝试提取image_data_base64
                                int imageDataPos = data.indexOf("\"image_data_base64\"");
                                if (imageDataPos > 0) {
                                    int imageDataStart = data.indexOf("\"", imageDataPos + 20) + 1;

                                    if (imageDataStart > 0) {
                                        // 找到最后一个引号，但要小心处理
                                        int imageDataEnd = -1;

                                        // 从后向前查找引号，但要确保它不是转义的引号
                                        for (int i = data.size() - 1; i > imageDataStart; i--) {
                                            if (data[i] == '"' && (i == 0 || data[i-1] != '\\')) {
                                                imageDataEnd = i;
                                                break;
                                            }
                                        }

                                        if (imageDataEnd > imageDataStart) {
                                            QByteArray imageDataBase64 = data.mid(imageDataStart, imageDataEnd - imageDataStart);

                                            // 解码Base64数据
                                            QByteArray imageData = QByteArray::fromBase64(imageDataBase64);

                                            qDebug() << "手动解析DownloadImageResponse成功，图片ID:" << imageId
                                                     << "，数据大小:" << imageData.size() << "字节";

                                            // 创建一个模拟的msgData对象
                                            QJsonObject simulatedMsgData;
                                            simulatedMsgData["status"] = "success";
                                            simulatedMsgData["imageId"] = imageId;
                                            simulatedMsgData["image_data_base64"] = QString::fromLatin1(imageDataBase64);

                                            // 调用处理函数
                                            handleDownloadImageResponse(simulatedMsgData);
                                            return;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 如果所有解析方法都失败，尝试直接从服务器重新请求图片
            qDebug() << "无法解析DownloadImageResponse，尝试重新请求图片";

            // 尝试从数据中提取图片ID (使用不同的变量名避免重复声明)
            int fallbackImageIdPos = data.indexOf("\"imageId\"");
            if (fallbackImageIdPos > 0) {
                int fallbackImageIdStart = data.indexOf("\"", fallbackImageIdPos + 10) + 1;
                int fallbackImageIdEnd = data.indexOf("\"", fallbackImageIdStart);

                if (fallbackImageIdStart > 0 && fallbackImageIdEnd > fallbackImageIdStart) {
                    QString fallbackImageId = QString::fromUtf8(data.mid(fallbackImageIdStart, fallbackImageIdEnd - fallbackImageIdStart));

                    qDebug() << "提取到图片ID:" << fallbackImageId << "，重新请求下载";
                    downloadImage(fallbackImageId);
                }
            }
        }

        return;
    }

    QJsonObject messageObj = doc.object();
    int type = messageObj["type"].toInt();
    QJsonObject msgData = messageObj["data"].toObject();

    MessageType messageType = static_cast<MessageType>(type);
    qDebug() << "收到消息类型:" << MessageProtocol::messageTypeToString(messageType);

    switch (messageType) {
        case MessageType::Register:
            if (msgData.value("status").toString() == "success") {
                emit statusMessage("注册成功！请使用新账户登录。");
            } else {
                emit statusMessage("注册失败：" + msgData.value("reason").toString("未知错误"));
            }
            break;

        case MessageType::Login:
            if (msgData.value("status").toString() == "success") {
                m_isLoggedIn = true;
                emit isLoggedInChanged();
                emit statusMessage(QString("以 %1 身份登录").arg(m_currentNickname));
                clearChatDisplay();
                m_currentChatFriend.clear();
                emit currentChatFriendChanged();

                // 请求好友列表 - 先发送这个请求并等待一小段时间确保消息被发送
                m_socket->write(QJsonDocument(MessageProtocol::createMessage(
                    MessageType::FriendList, {})).toJson());
                m_socket->flush();

                // 等待一小段时间，确保两个请求不会合并在一起
                QTimer::singleShot(100, this, [this]() {
                    // 在短暂延迟后请求好友请求列表
                    if(m_socket && m_socket->state() == QAbstractSocket::ConnectedState && m_isLoggedIn) {
                        qDebug() << "延迟发送获取好友请求列表请求";
                        m_socket->write(QJsonDocument(MessageProtocol::createMessage(
                            MessageType::FriendRequestList, {})).toJson());
                        m_socket->flush();
                    }
                });
            } else {
                emit statusMessage("登录失败：" + msgData.value("reason").toString("未知错误"));
            }
            break;

        case MessageType::Message:
            {
                QString from = msgData.value("from").toString();
                QString contentStr = msgData.value("content").toString();
                QString timestamp = QDateTime::currentDateTime().toString("hh:mm");

                // 尝试解析JSON内容
                QJsonDocument contentDoc = QJsonDocument::fromJson(contentStr.toUtf8());

                if (!contentDoc.isNull() && contentDoc.isObject()) {
                    // 如果是JSON对象，根据类型处理
                    QJsonObject contentObj = contentDoc.object();
                    QString type = contentObj["type"].toString();

                    if (type == "image") {
                        // 图片消息
                        QString imageId = contentObj["imageId"].toString();

                        // 直接使用发送方提供的本地路径
                        if (contentObj.contains("localPath")) {
                            QString localPath = contentObj["localPath"].toString();
                            int width = contentObj["width"].toInt();
                            int height = contentObj["height"].toInt();

                            qDebug() << "接收到图片消息，直接使用本地路径:" << localPath;

                            // 检查文件是否存在
                            QFileInfo fileInfo(localPath);
                            if (fileInfo.exists() && fileInfo.isReadable()) {
                                // 文件存在且可读，直接使用
                                QJsonObject displayJson;
                                displayJson["type"] = "image";
                                displayJson["imageId"] = imageId;
                                displayJson["localPath"] = localPath;
                                displayJson["width"] = width > 0 ? width : 800;  // 默认宽度
                                displayJson["height"] = height > 0 ? height : 600;  // 默认高度

                                // 显示图片消息
                                if (from == m_currentChatFriend) {
                                    appendMessage(from, QString::fromUtf8(QJsonDocument(displayJson).toJson()), timestamp);
                                } else {
                                    emit statusMessage(QString("来自 %1 的新图片消息").arg(from));
                                }

                                // 保存到缓存映射，以便后续使用
                                m_imageCacheMap[imageId] = localPath;
                            } else {
                                qDebug() << "本地图片文件不存在或不可读:" << localPath;

                                // 先显示占位符
                                if (from == m_currentChatFriend) {
                                    appendMessage(from, "[图片加载中...]", timestamp);
                                } else {
                                    emit statusMessage(QString("来自 %1 的新图片消息").arg(from));
                                }

                                // 尝试从服务器下载（保留服务器下载逻辑）
                                downloadImage(imageId);
                            }
                        } else {
                            // 如果没有本地路径，尝试从缓存中获取
                            if (m_imageCacheMap.contains(imageId)) {
                                QString cachedPath = m_imageCacheMap[imageId];
                                QFileInfo fileInfo(cachedPath);

                                if (fileInfo.exists() && fileInfo.isReadable()) {
                                    // 文件存在且可读，获取图片尺寸
                                    QImage image(cachedPath);
                                    int width = image.width();
                                    int height = image.height();

                                    if (width > 0 && height > 0) {
                                        // 图片有效，创建JSON对象
                                        QJsonObject displayJson;
                                        displayJson["type"] = "image";
                                        displayJson["imageId"] = imageId;
                                        displayJson["localPath"] = cachedPath;
                                        displayJson["width"] = width;
                                        displayJson["height"] = height;

                                        // 如果有缓存，直接显示
                                        if (from == m_currentChatFriend) {
                                            appendMessage(from, QString::fromUtf8(QJsonDocument(displayJson).toJson()), timestamp);
                                        } else {
                                            emit statusMessage(QString("来自 %1 的新图片消息").arg(from));
                                        }
                                    } else {
                                        // 图片无效，尝试从服务器下载
                                        qDebug() << "缓存的图片无效，尝试从服务器下载:" << imageId;
                                        downloadImage(imageId);

                                        // 先显示占位符
                                        if (from == m_currentChatFriend) {
                                            appendMessage(from, "[图片加载中...]", timestamp);
                                        } else {
                                            emit statusMessage(QString("来自 %1 的新图片消息").arg(from));
                                        }
                                    }
                                } else {
                                    // 文件不存在或不可读，尝试从服务器下载
                                    qDebug() << "缓存的图片文件不存在或不可读，尝试从服务器下载:" << imageId;
                                    downloadImage(imageId);

                                    // 先显示占位符
                                    if (from == m_currentChatFriend) {
                                        appendMessage(from, "[图片加载中...]", timestamp);
                                    } else {
                                        emit statusMessage(QString("来自 %1 的新图片消息").arg(from));
                                    }
                                }
                            } else {
                                // 如果没有缓存，尝试从服务器下载
                                qDebug() << "没有本地路径和缓存，尝试从服务器下载图片:" << imageId;
                                downloadImage(imageId);

                                // 先显示占位符
                                if (from == m_currentChatFriend) {
                                    appendMessage(from, "[图片加载中...]", timestamp);
                                } else {
                                    emit statusMessage(QString("来自 %1 的新图片消息").arg(from));
                                }
                            }
                        }
                    } else if (type == "text") {
                        // 文本消息
                        QString text = contentObj["text"].toString();
                        if (from == m_currentChatFriend) {
                            appendMessage(from, text, timestamp);
                        } else {
                            emit statusMessage(QString("来自 %1 的新消息: %2").arg(from, text));
                        }
                    } else {
                        // 未知类型，显示原始内容
                        if (from == m_currentChatFriend) {
                            appendMessage(from, contentStr, timestamp);
                        } else {
                            emit statusMessage(QString("来自 %1 的新消息").arg(from));
                        }
                    }
                } else {
                    // 如果不是JSON对象，当作普通文本处理
                    if (from == m_currentChatFriend) {
                        appendMessage(from, contentStr, timestamp);
                    } else {
                        emit statusMessage(QString("来自 %1 的新消息").arg(from));
                    }
                }
            }
            break;

        case MessageType::FriendList:
            if (msgData.value("status").toString() == "success") {
                QStringList friends;
                QJsonArray friendArray = msgData.value("friends").toArray();
                for (const QJsonValue &value : friendArray) {
                    friends << value.toString();
                }
                friends.sort(Qt::CaseInsensitive);
                qDebug() << "收到好友列表：" << friends;
                if (m_friendList != friends) {
                    updateFriendList(friends);
                    qDebug() << "好友列表已更新！";
                } else {
                    qDebug() << "好友列表未变化，不更新。";
                }
            } else {
                emit statusMessage("无法获取好友列表。");
            }
            break;

        case MessageType::ChatHistory:
            if (msgData.value("status").toString() == "success") {
                clearChatDisplay();
                QJsonArray messages = msgData.value("messages").toArray();
                if (messages.isEmpty()) {
                    appendMessage("", "尚无消息记录。", "");
                } else {
                    for (const QJsonValue &msgValue : messages) {
                        QJsonObject msgObj = msgValue.toObject();
                        QString sender = msgObj.value("from").toString();
                        QString contentStr = msgObj.value("content").toString();
                        QString timestamp = QDateTime::fromString(msgObj.value("timestamp").toString(), Qt::ISODate).toString("hh:mm");

                        // 如果缓存中没有该用户的头像，则尝试获取
                        if (!m_avatarCache.contains(sender)) {
                            requestAvatar(sender);
                        }

                        // 尝试解析JSON内容
                        QJsonDocument contentDoc = QJsonDocument::fromJson(contentStr.toUtf8());

                        if (!contentDoc.isNull() && contentDoc.isObject()) {
                            // 如果是JSON对象，根据类型处理
                            QJsonObject contentObj = contentDoc.object();
                            QString type = contentObj["type"].toString();

                            if (type == "image") {
                                // 图片消息处理
                                QString imageId = contentObj["imageId"].toString();

                                // 检查是否有缓存
                                if (m_imageCacheMap.contains(imageId)) {
                                    QString cachedPath = m_imageCacheMap[imageId];
                                    QFileInfo fileInfo(cachedPath);

                                    if (fileInfo.exists() && fileInfo.isReadable()) {
                                        // 文件存在且可读，获取图片尺寸
                                        QImage image(cachedPath);
                                        int width = image.width();
                                        int height = image.height();

                                        if (width > 0 && height > 0) {
                                            // 图片有效，创建JSON对象
                                            QJsonObject displayJson;
                                            displayJson["type"] = "image";
                                            displayJson["imageId"] = imageId;
                                            displayJson["localPath"] = cachedPath;
                                            displayJson["width"] = width;
                                            displayJson["height"] = height;

                                            // 显示图片消息
                                            appendMessage(sender, QString::fromUtf8(QJsonDocument(displayJson).toJson()), timestamp);
                                        } else {
                                            // 图片无效，尝试从服务器下载
                                            downloadImage(imageId);
                                            appendMessage(sender, "[图片加载中...]", timestamp);
                                        }
                                    } else {
                                        // 文件不存在或不可读，尝试从服务器下载
                                        downloadImage(imageId);
                                        appendMessage(sender, "[图片加载中...]", timestamp);
                                    }
                                } else {
                                    // 没有缓存，尝试从服务器下载
                                    downloadImage(imageId);
                                    appendMessage(sender, "[图片加载中...]", timestamp);
                                }
                            } else if (type == "text") {
                                // 文本消息
                                QString text = contentObj["text"].toString();
                                appendMessage(sender, text, timestamp);
                            } else {
                                // 未知类型，显示原始内容
                                appendMessage(sender, contentStr, timestamp);
                            }
                        } else {
                            // 如果不是JSON对象，当作普通文本处理
                            appendMessage(sender, contentStr, timestamp);
                        }
                    }
                }
            } else {
                emit statusMessage("无法获取聊天记录。");
            }
            break;

        case MessageType::SearchUser:
            if (msgData.value("status").toString() == "success") {
                QString foundNickname = msgData.value("nickname").toString();
                emit statusMessage(QString("找到用户 '%1'，可以添加为好友。").arg(foundNickname));
            } else {
                emit statusMessage("未找到匹配的用户。");
            }
            break;

        case MessageType::AddFriend:
            if (msgData.value("status").toString() == "success") {
                emit statusMessage("成功添加好友！");
                m_socket->write(QJsonDocument(MessageProtocol::createMessage(
                    MessageType::FriendList, {})).toJson());
            } else {
                emit statusMessage("添加好友失败：" + msgData.value("reason").toString("无法添加好友"));
            }
            break;

        case MessageType::Logout:
            if (msgData.value("status").toString() == "success") {
                m_isLoggedIn = false;
                m_currentNickname.clear();
                m_currentChatFriend.clear();
                m_friendList.clear();
                emit isLoggedInChanged();
                emit currentNicknameChanged();
                emit currentChatFriendChanged();
                emit friendListUpdated(m_friendList);
                emit chatDisplayCleared();
                emit statusMessage("已成功登出");
            }
            break;

        case MessageType::FriendRequest:
            qDebug() << "收到好友请求消息：" << msgData;
            if (msgData["status"].toString() == "success") {
                emit statusMessage("好友请求已发送。");
            } else if (msgData.contains("from")) {
                // 收到新的好友请求
                QString from = msgData["from"].toString();
                qDebug() << "收到来自" << from << "的好友请求";
                emit friendRequestReceived(from);
                refreshFriendRequests();
            } else {
                emit statusMessage("发送好友请求失败：" + msgData["reason"].toString());
            }
            break;

        case MessageType::FriendRequestList:
            qDebug() << "收到好友请求列表响应：" << msgData;
            if (msgData["status"].toString() == "success") {
                QStringList requests;
                QJsonArray requestArray = msgData["requests"].toArray();
                qDebug() << "原始请求数组：" << requestArray;
                for (const QJsonValue &value : requestArray) {
                    requests << value.toString();
                    qDebug() << "添加请求：" << value.toString();
                }
                qDebug() << "更新好友请求列表：" << requests;
                if (m_friendRequests != requests) {
                    qDebug() << "旧请求列表：" << m_friendRequests;
                    updateFriendRequests(requests);
                    qDebug() << "好友请求列表已更新！";
                } else {
                    qDebug() << "好友请求列表未变化，不更新。";
                }
            } else {
                qDebug() << "获取好友请求列表失败：" << msgData["reason"].toString("未知错误");
            }
            break;

        case MessageType::FriendStatus: {
            QString friendName = msgData["nickname"].toString();
            bool isOnline = msgData["online"].toBool();
            updateFriendOnlineStatus(friendName, isOnline);
            break;
        }

        case MessageType::GroupList:
            if (msgData["status"].toString() == "success") {
                QStringList groups;
                QJsonArray groupArray = msgData["groups"].toArray();
                for (const QJsonValue &value : groupArray) {
                    groups << value.toString();
                }
                groups.sort(Qt::CaseInsensitive);
                updateGroupList(groups);
                emit statusMessage("已更新群聊列表");
            } else {
                emit statusMessage("获取群聊列表失败");
            }
            break;

        case MessageType::GroupMembers:
            if (msgData["status"].toString() == "success") {
                QStringList members;
                QJsonArray memberArray = msgData["members"].toArray();
                for (const QJsonValue &value : memberArray) {
                    members << value.toString();
                }
                members.sort(Qt::CaseInsensitive);
                updateGroupMembers(members);
            } else {
                emit statusMessage("获取群成员列表失败");
            }
            break;

        case MessageType::GroupChat:
            // 处理服务器发来的群聊消息
            if (msgData.contains("status") && msgData["status"].toString() == "failed") {
                emit statusMessage("发送群聊消息失败：" + msgData["reason"].toString("未知错误"));
            }
            // 如果消息包含group_id和content，表示这是一条群聊消息
            else if (msgData.contains("group_id") && msgData.contains("content") && msgData.contains("from")) {
                // 直接转发给handleGroupChatMessage处理
                handleGroupChatMessage(msgData);
            }
            break;

        case MessageType::GroupChatHistory:
            if (msgData["status"].toString() == "success") {
                qDebug() << "收到群聊历史记录";
                QJsonArray messages = msgData["messages"].toArray();

                if (messages.isEmpty()) {
                    qDebug() << "群聊历史记录为空，显示提示信息";
                    QString systemAvatarPath = "qrc:/images/default_avatar.png"; // 系统消息使用默认头像
                    emit groupChatMessageReceived("系统", "暂无群聊记录", QDateTime::currentDateTime().toString("hh:mm"), systemAvatarPath);
                } else {
                    for (const QJsonValue &messageVal : messages) {
                        QJsonObject message = messageVal.toObject();
                        QString sender = message["from"].toString();
                        QString contentStr = message["content"].toString();
                        QString timestamp = QDateTime::fromString(message["timestamp"].toString(), Qt::ISODate).toString("hh:mm");

                        // 如果缓存中没有该用户的头像，则尝试获取
                        if (!m_avatarCache.contains(sender)) {
                            requestAvatar(sender);
                        }

                        // 获取头像路径
                        QString avatarPath = getCachedAvatarPath(sender);

                        // 尝试解析JSON内容
                        QJsonDocument contentDoc = QJsonDocument::fromJson(contentStr.toUtf8());

                        if (!contentDoc.isNull() && contentDoc.isObject()) {
                            // 如果是JSON对象，根据类型处理
                            QJsonObject contentObj = contentDoc.object();
                            QString type = contentObj["type"].toString();

                            if (type == "image") {
                                // 图片消息处理
                                QString imageId = contentObj["imageId"].toString();

                                // 检查是否有缓存
                                if (m_imageCacheMap.contains(imageId)) {
                                    QString cachedPath = m_imageCacheMap[imageId];
                                    QFileInfo fileInfo(cachedPath);

                                    if (fileInfo.exists() && fileInfo.isReadable()) {
                                        // 文件存在且可读，获取图片尺寸
                                        QImage image(cachedPath);
                                        int width = image.width();
                                        int height = image.height();

                                        if (width > 0 && height > 0) {
                                            // 图片有效，创建JSON对象
                                            QJsonObject displayJson;
                                            displayJson["type"] = "image";
                                            displayJson["imageId"] = imageId;
                                            displayJson["localPath"] = cachedPath;
                                            displayJson["width"] = width;
                                            displayJson["height"] = height;

                                            // 显示图片消息
                                            emit groupChatMessageReceived(sender, QString::fromUtf8(QJsonDocument(displayJson).toJson()), timestamp, avatarPath);
                                        } else {
                                            // 图片无效，尝试从服务器下载
                                            downloadImage(imageId);
                                            emit groupChatMessageReceived(sender, "[图片加载中...]", timestamp, avatarPath);
                                        }
                                    } else {
                                        // 文件不存在或不可读，尝试从服务器下载
                                        downloadImage(imageId);
                                        emit groupChatMessageReceived(sender, "[图片加载中...]", timestamp, avatarPath);
                                    }
                                } else {
                                    // 没有缓存，尝试从服务器下载
                                    downloadImage(imageId);
                                    emit groupChatMessageReceived(sender, "[图片加载中...]", timestamp, avatarPath);
                                }
                            } else if (type == "text") {
                                // 文本消息
                                QString text = contentObj["text"].toString();
                                emit groupChatMessageReceived(sender, text, timestamp, avatarPath);
                            } else {
                                // 未知类型，显示原始内容
                                emit groupChatMessageReceived(sender, contentStr, timestamp, avatarPath);
                            }
                        } else {
                            // 如果不是JSON对象，当作普通文本处理
                            emit groupChatMessageReceived(sender, contentStr, timestamp, avatarPath);
                        }
                    }
                }
            }
            break;

        case MessageType::GetUserProfile:
            if (msgData["status"].toString() == "success") {
                // 更新用户资料
                m_userProfile.clear();

                // 复制关键字段到用户资料
                QStringList fields = {"nickname", "email", "signature", "gender", "birthday",
                                     "location", "phone", "register_time", "last_login_time",
                                     "friend_count", "group_count"};
                for (const QString &field : fields) {
                    if (msgData.contains(field)) {
                        m_userProfile[field] = msgData[field].toVariant();
                    }
                }

                emit userProfileChanged();

                // 如果有头像信息，请求头像
                if (msgData.contains("avatar") && !msgData["avatar"].toString().isEmpty()) {
                    requestAvatar(m_currentNickname);
                }
            } else {
                emit statusMessage("获取用户资料失败：" + msgData["reason"].toString("未知错误"));
            }
            break;

        case MessageType::UpdateUserProfile:
            if (msgData["status"].toString() == "success") {
                emit statusMessage("个人资料已更新");
                emit profileUpdateSuccess();
                // 重新请求用户资料以获取最新数据
                requestUserProfile();
            } else {
                emit statusMessage("更新个人资料失败：" + msgData["reason"].toString("未知错误"));
            }
            break;

        case MessageType::UploadAvatar:
            if (msgData["status"].toString() == "success") {
                emit statusMessage("头像已更新");
                emit avatarUploadSuccess();
                // 请求最新的头像
                requestAvatar(m_currentNickname);
            } else {
                emit statusMessage("上传头像失败：" + msgData["reason"].toString("未知错误"));
            }
            break;

        case MessageType::GetAvatar: {
            QString nickname = msgData["nickname"].toString();

            // 检查是否有头像数据
            if (msgData.contains("avatar_data")) {
                QByteArray data = QByteArray::fromBase64(msgData["avatar_data"].toString().toLatin1());
                qDebug() << "收到头像数据，用户:" << nickname << "，大小:" << data.size() << "字节";

                if (data.size() > 0) {
                    // 创建应用数据目录的avatars文件夹
                    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
                    QDir dir(appDataPath + "/avatars/");
                    if (!dir.exists()) {
                        bool created = dir.mkpath(".");
                        qDebug() << "创建头像目录结果:" << created << ", 路径:" << dir.absolutePath();
                    }

                    // 保存到应用数据目录
                    QString filePath = dir.absolutePath() + "/" + nickname + ".png";

                    // 如果文件已存在，先删除
                    QFile existingFile(filePath);
                    if (existingFile.exists()) {
                        bool removed = existingFile.remove();
                        qDebug() << "删除已存在的头像文件结果:" << removed;
                    }

                    QFile file(filePath);
                    if (file.open(QIODevice::WriteOnly)) {
                        qint64 bytesWritten = file.write(data);
                        file.close();

                        if (bytesWritten == data.size()) {
                            qDebug() << "头像保存成功:" << filePath;

                            // 设置文件权限确保可读写
                            QFile::setPermissions(filePath, QFile::ReadOwner | QFile::WriteOwner);

                            // 更新缓存
                            m_avatarCache[nickname] = filePath;

                            // 保存头像路径到设置
                            QSettings settings;
                            settings.beginGroup("AvatarCache");
                            settings.setValue(nickname, filePath);
                            settings.endGroup();
                            settings.sync(); // 确保立即写入磁盘

                            emit avatarReceived(nickname, filePath);
                            qDebug() << "已发送avatarReceived信号，路径:" << filePath;
                        } else {
                            qDebug() << "头像保存失败: 写入的字节数不匹配，预期:" << data.size() << "，实际:" << bytesWritten;
                        }
                    } else {
                        qDebug() << "无法打开文件进行写入:" << filePath << ", 错误:" << file.errorString();
                    }
                } else {
                    qDebug() << "用户" << nickname << "的头像数据为空";
                }
            } else if (msgData["status"].toString() == "error") {
                qDebug() << "获取头像失败：" << msgData["reason"].toString();
            } else {
                qDebug() << "收到的头像响应格式不正确";
            }
            break;
        }

        case MessageType::UploadImageResponse:
            handleUploadImageResponse(msgData);
            break;

        case MessageType::DownloadImageResponse:
            handleDownloadImageResponse(msgData);
            break;

        case MessageType::ChunkedImageResponse:
            handleChunkedImageResponse(msgData);
            break;

        default:
            qDebug() << "未处理的消息类型:" << type;
            break;
    }
}

void ChatWindow::loadChatHistory(const QString &friendName)
{
    if (!m_isLoggedIn) return;
    qDebug() << "请求与 " << friendName << " 的聊天记录";
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject data{{"friend", friendName}};
        m_socket->write(QJsonDocument(MessageProtocol::createMessage(
            MessageType::ChatHistory, data)).toJson());
    } else {
        emit statusMessage("未连接到服务器，无法加载聊天记录。");
    }
}

bool ChatWindow::isFriendOnline(const QString &friendName) const {
    return m_friendOnlineStatus.value(friendName, false).toBool();
}

void ChatWindow::updateFriendOnlineStatus(const QString &friendName, bool isOnline) {
    if (m_friendOnlineStatus.value(friendName, !isOnline).toBool() != isOnline) {
        m_friendOnlineStatus[friendName] = isOnline;
        emit friendOnlineStatusChanged();
    }
}

void ChatWindow::handleAcceptFriendMessage(const QJsonObject &msgData)
{
    qDebug() << "处理接受好友请求消息：" << msgData;

    if (msgData["status"].toString() == "success") {
        if (msgData.contains("friend")) {
            // 作为请求发送者收到接受通知
            QString acceptedFriend = msgData["friend"].toString();
            qDebug() << "作为请求发送者收到接受通知，新好友：" << acceptedFriend;
            emit friendRequestAccepted(acceptedFriend);
            emit statusMessage(acceptedFriend + " 接受了您的好友请求。");

            // 立即请求刷新好友列表，而不依赖服务器主动发送
            if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
                qDebug() << "主动请求刷新好友列表...";
                QJsonObject emptyData;
                QByteArray request = QJsonDocument(MessageProtocol::createMessage(
                    MessageType::FriendList, emptyData)).toJson();
                m_socket->write(request);
                m_socket->flush();
            }
        } else {
            // 作为接受者的响应
            qDebug() << "作为接受者收到成功响应";
            emit statusMessage("已接受好友请求。");

            // 刷新好友列表和好友请求列表
            if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
                qDebug() << "刷新好友列表和好友请求列表...";

                // 请求刷新好友列表
                QJsonObject emptyData;
                QByteArray friendListRequest = QJsonDocument(MessageProtocol::createMessage(
                    MessageType::FriendList, emptyData)).toJson();
                m_socket->write(friendListRequest);
                m_socket->flush();

                // 请求刷新好友请求列表
                QByteArray requestListRequest = QJsonDocument(MessageProtocol::createMessage(
                    MessageType::FriendRequestList, emptyData)).toJson();
                m_socket->write(requestListRequest);
                m_socket->flush();
            }
        }
    } else {
        qDebug() << "接受好友请求失败：" << msgData["reason"].toString();
        emit statusMessage("接受好友请求失败：" + msgData["reason"].toString());
    }
}

void ChatWindow::handleDeleteFriendRequestMessage(const QJsonObject &msgData)
{
    qDebug() << "处理删除好友请求消息：" << msgData;

    if (msgData["status"].toString() == "success") {
        emit statusMessage("已删除好友请求");

        // 刷新好友请求列表
        if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
            qDebug() << "刷新好友请求列表...";
            refreshFriendRequests();
        }
    } else {
        emit statusMessage("删除好友请求失败：" + msgData["reason"].toString("未知错误"));
    }
}

void ChatWindow::createGroup(const QString &groupName, const QStringList &members)
{
    if (!m_isLoggedIn) {
        emit statusMessage("请先登录以创建群聊。");
        return;
    }
    if (groupName.isEmpty()) {
        emit statusMessage("请输入群聊名称。");
        return;
    }
    if (members.isEmpty()) {
        emit statusMessage("请选择至少一个群聊成员。");
        return;
    }

    QJsonObject data;
    data["group_name"] = groupName;

    // 将成员列表转换为 JSON 数组
    QJsonArray membersArray;
    for (const QString &member : members) {
        membersArray.append(member);
    }
    data["members"] = membersArray;

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::CreateGroup, data)).toJson();
        m_socket->write(request);
        m_socket->flush();
        emit statusMessage("正在创建群聊...");
    } else {
        emit statusMessage("未连接到服务器，无法创建群聊。");
    }
}

void ChatWindow::refreshGroupList()
{
    if (!m_isLoggedIn) {
        emit statusMessage("请先登录以查看群聊列表。");
        return;
    }

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject emptyData;
        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::GroupList, emptyData)).toJson();
        m_socket->write(request);
        m_socket->flush();
    } else {
        emit statusMessage("未连接到服务器，无法获取群聊列表。");
    }
}

void ChatWindow::selectGroup(const QString &groupId)
{
    if (groupId.isEmpty()) {
        emit statusMessage("群聊ID不能为空。");
        return;
    }

    qDebug() << "选择群聊:" << groupId << "，群名称:" << getGroupName(groupId);

    // 清除单人聊天的状态
    m_currentChatFriend.clear();
    emit currentChatFriendChanged();

    // 设置群聊状态（先设置这些，让UI可以立即响应）
    m_currentChatGroup = groupId;
    m_isGroupChat = true;

    // 立即发出信号通知UI更新
    emit currentChatGroupChanged();
    emit isGroupChatChanged();

    // 清除聊天显示
    clearChatDisplay();

    // 确保消息能可靠地发送到服务器
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        // 先延迟一小段时间，确保状态更新到服务器
        QTimer::singleShot(50, this, [this, groupId]() {
            if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
                // 1. 请求群成员
                QJsonObject membersData;
                membersData["group_id"] = groupId.toInt();
                QByteArray membersRequest = QJsonDocument(MessageProtocol::createMessage(
                    MessageType::GroupMembers, membersData)).toJson();

                qDebug() << "发送获取群成员请求，群ID:" << groupId;
                m_socket->write(membersRequest);
                m_socket->flush();

                // 等待一小段时间确保请求不会合并
                QTimer::singleShot(50, this, [this, groupId]() {
                    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
                        // 2. 请求聊天历史
                        QJsonObject historyData;
                        historyData["group_id"] = groupId.toInt();
                        QByteArray historyRequest = QJsonDocument(MessageProtocol::createMessage(
                            MessageType::GroupChatHistory, historyData)).toJson();

                        qDebug() << "发送获取群聊历史请求，群ID:" << groupId;
                        m_socket->write(historyRequest);
                        m_socket->flush();
                    }
                });
            }
        });
    } else {
        emit statusMessage("未连接到服务器，无法加载群聊信息。");
    }
}

void ChatWindow::sendGroupMessage(const QString &content)
{
    if (!m_isLoggedIn) {
        emit statusMessage("请先登录以发送群聊消息。");
        return;
    }
    if (m_currentChatGroup.isEmpty()) {
        emit statusMessage("请选择一个群聊。");
        return;
    }
    QString message = content.trimmed();
    if (message.isEmpty()) {
        return;
    }

    qDebug() << "发送群聊消息到群" << m_currentChatGroup << "，内容:" << message << "，isGroupChat:" << m_isGroupChat;

    // 创建文本消息的JSON内容
    QJsonObject contentJson;
    contentJson["type"] = "text";
    contentJson["text"] = message;

    // 发送群聊消息
    sendGroupMessageInternal(contentJson);
}

void ChatWindow::sendMessageInternal(const QJsonObject &contentJson)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit statusMessage("未连接到服务器，无法发送消息。");
        return;
    }

    // 将内容JSON转换为字符串
    QJsonDocument contentDoc(contentJson);
    QString contentStr = QString::fromUtf8(contentDoc.toJson(QJsonDocument::Compact));

    QJsonObject data{
        {"to", m_currentChatFriend},
        {"content", contentStr}
    };

    m_socket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Message, data)).toJson());

    // 立即在本地显示消息
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm");

    // 根据消息类型显示不同内容
    if (contentJson["type"].toString() == "text") {
        appendMessage(m_currentNickname, contentJson["text"].toString(), timestamp);
    } else if (contentJson["type"].toString() == "image") {
        QString displayText = "[图片]";
        if (contentJson.contains("localPath")) {
            // 如果有本地路径，使用本地路径显示图片
            QJsonObject displayJson;
            displayJson["type"] = "image";
            displayJson["imageId"] = contentJson["imageId"].toString();
            displayJson["localPath"] = contentJson["localPath"].toString();

            // 添加图片尺寸信息
            if (contentJson.contains("width") && contentJson.contains("height")) {
                displayJson["width"] = contentJson["width"];
                displayJson["height"] = contentJson["height"];
            } else {
                // 如果没有提供尺寸，尝试从图片文件获取
                QString localPath = contentJson["localPath"].toString();
                QImage image(localPath);
                if (!image.isNull()) {
                    displayJson["width"] = image.width();
                    displayJson["height"] = image.height();
                }
            }

            appendMessage(m_currentNickname, QString::fromUtf8(QJsonDocument(displayJson).toJson()), timestamp);
        } else {
            // 否则显示占位文本
            appendMessage(m_currentNickname, displayText, timestamp);
        }
    }
}

void ChatWindow::sendGroupMessageInternal(const QJsonObject &contentJson)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit statusMessage("未连接到服务器，无法发送群聊消息。");
        return;
    }

    // 将内容JSON转换为字符串
    QJsonDocument contentDoc(contentJson);
    QString contentStr = QString::fromUtf8(contentDoc.toJson(QJsonDocument::Compact));

    QJsonObject data;
    data["group_id"] = m_currentChatGroup.toInt();
    data["content"] = contentStr;

    QByteArray request = QJsonDocument(MessageProtocol::createMessage(
        MessageType::GroupChat, data)).toJson();
    m_socket->write(request);
    m_socket->flush();

    // 立即在本地显示消息
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm");
    QString avatarPath = getCachedAvatarPath(m_currentNickname);

    // 根据消息类型显示不同内容
    if (contentJson["type"].toString() == "text") {
        qDebug() << "发出groupChatMessageReceived信号，发送者:" << m_currentNickname << "内容:" << contentJson["text"].toString();
        emit groupChatMessageReceived(m_currentNickname, contentJson["text"].toString(), timestamp, avatarPath);
    } else if (contentJson["type"].toString() == "image") {
        if (contentJson.contains("localPath")) {
            // 如果有本地路径，使用本地路径显示图片
            QJsonObject displayJson;
            displayJson["type"] = "image";
            displayJson["imageId"] = contentJson["imageId"].toString();
            displayJson["localPath"] = contentJson["localPath"].toString();

            // 添加图片尺寸信息
            if (contentJson.contains("width") && contentJson.contains("height")) {
                displayJson["width"] = contentJson["width"];
                displayJson["height"] = contentJson["height"];
            } else {
                // 如果没有提供尺寸，尝试从图片文件获取
                QString localPath = contentJson["localPath"].toString();
                QImage image(localPath);
                if (!image.isNull()) {
                    displayJson["width"] = image.width();
                    displayJson["height"] = image.height();
                }
            }

            qDebug() << "发出群聊图片消息，发送者:" << m_currentNickname;
            emit groupChatMessageReceived(m_currentNickname, QString::fromUtf8(QJsonDocument(displayJson).toJson()), timestamp, avatarPath);
        } else {
            // 否则显示占位文本
            qDebug() << "发出群聊图片消息（占位符），发送者:" << m_currentNickname;
            emit groupChatMessageReceived(m_currentNickname, "[图片]", timestamp, avatarPath);
        }
    }
}

void ChatWindow::getGroupMembers(const QString &groupId)
{
    if (!m_isLoggedIn) {
        return;
    }

    QJsonObject data;
    data["group_id"] = groupId.toInt();

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::GroupMembers, data)).toJson();
        m_socket->write(request);
        m_socket->flush();
    }
}

QString ChatWindow::getGroupName(const QString &groupId) const
{
    return m_groupNames.value(groupId, "未知群聊");
}

void ChatWindow::clearChatType()
{
    m_currentChatFriend.clear();
    m_currentChatGroup.clear();
    m_isGroupChat = false;
    m_currentGroupMembers.clear();
    emit currentChatFriendChanged();
    emit currentChatGroupChanged();
    emit isGroupChatChanged();
    emit groupMembersUpdated();
    clearChatDisplay();
}

void ChatWindow::updateGroupList(const QStringList &groups)
{
    m_groupList = groups;
    m_groupNames.clear();

    // 解析格式为 "id:name" 的群组列表，并填充 m_groupNames
    for (const QString &group : groups) {
        QStringList parts = group.split(':');
        if (parts.size() == 2) {
            m_groupNames[parts.at(0)] = parts.at(1);
        }
    }

    emit groupListUpdated();
}

void ChatWindow::updateGroupMembers(const QStringList &members)
{
    m_currentGroupMembers = members;
    emit groupMembersUpdated();
}

void ChatWindow::loadGroupChatHistory(const QString &groupId)
{
    if (!m_isLoggedIn) return;

    QJsonObject data;
    data["group_id"] = groupId.toInt();

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::GroupChatHistory, data)).toJson();
        m_socket->write(request);
        m_socket->flush();
    } else {
        emit statusMessage("未连接到服务器，无法加载群聊历史。");
    }
}

void ChatWindow::handleCreateGroupMessage(const QJsonObject &msgData)
{
    if (msgData["status"].toString() == "success") {
        QString groupName = msgData["group_name"].toString();
        emit statusMessage("成功创建群聊：" + groupName);
        emit groupCreated(groupName);

        // 刷新群聊列表
        refreshGroupList();
    } else if (msgData.contains("group_name") && msgData.contains("creator")) {
        // 被添加到新群聊的通知
        QString groupName = msgData["group_name"].toString();
        QString creator = msgData["creator"].toString();
        int groupId = msgData["group_id"].toInt();

        emit statusMessage(QString("你被 %1 添加到群聊：%2").arg(creator, groupName));

        // 刷新群聊列表
        refreshGroupList();
    } else {
        emit statusMessage("创建群聊失败：" + msgData["reason"].toString("未知错误"));
    }
}

void ChatWindow::handleGroupChatMessage(const QJsonObject &msgData)
{
    if (msgData.contains("from") && msgData.contains("content")) {
        QString from = msgData["from"].toString();
        QString contentStr = msgData["content"].toString();
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm");
        int groupId = msgData["group_id"].toInt();

        qDebug() << "收到群聊消息，来自:" << from << "，内容:" << contentStr << "，群ID:" << groupId
                 << "，当前群ID:" << m_currentChatGroup << "，是否为群聊模式:" << m_isGroupChat;

        // 尝试获取发送者的头像
        if (!m_avatarCache.contains(from)) {
            // 如果缓存中没有该用户的头像，则请求获取
            requestAvatar(from);
        }

        // 获取头像路径
        QString avatarPath = getCachedAvatarPath(from);

        // 尝试解析JSON内容
        QJsonDocument contentDoc = QJsonDocument::fromJson(contentStr.toUtf8());

        if (!contentDoc.isNull() && contentDoc.isObject()) {
            // 如果是JSON对象，根据类型处理
            QJsonObject contentObj = contentDoc.object();
            QString type = contentObj["type"].toString();

            if (type == "image") {
                // 图片消息
                QString imageId = contentObj["imageId"].toString();

                // 本地路径
                if (contentObj.contains("localPath")) {
                    QString localPath = contentObj["localPath"].toString();
                    int width = contentObj["width"].toInt();
                    int height = contentObj["height"].toInt();

                    qDebug() << "接收到群聊图片消息:" << localPath;

                    // 检查文件是否存在
                    QFileInfo fileInfo(localPath);
                    if (fileInfo.exists() && fileInfo.isReadable()) {
                        // 文件存在且可读
                        QJsonObject displayJson;
                        displayJson["type"] = "image";
                        displayJson["imageId"] = imageId;
                        displayJson["localPath"] = localPath;
                        displayJson["width"] = width > 0 ? width : 800;  // 默认宽度
                        displayJson["height"] = height > 0 ? height : 600;  // 默认高度

                        // 显示图片消息
                        if (QString::number(groupId) == m_currentChatGroup) {
                            qDebug() << "为当前选中的群聊发出图片消息信号（使用本地路径）";
                            emit groupChatMessageReceived(from, QString::fromUtf8(QJsonDocument(displayJson).toJson()), timestamp, avatarPath);
                        } else {
                            QString groupName = m_groupNames.value(QString::number(groupId), "未知群聊");
                            emit statusMessage(QString("来自群聊 %1 的新图片消息（%2）").arg(groupName, from));
                        }

                        // 保存到缓存映射，以便后续使用
                        m_imageCacheMap[imageId] = localPath;
                    } else {
                        qDebug() << "本地图片文件不存在或不可读:" << localPath;

                        // 先显示占位符
                        if (QString::number(groupId) == m_currentChatGroup) {
                            qDebug() << "为当前选中的群聊发出图片消息信号（加载中）";
                            emit groupChatMessageReceived(from, "[图片加载中...]", timestamp, avatarPath);
                        } else {
                            QString groupName = m_groupNames.value(QString::number(groupId), "未知群聊");
                            emit statusMessage(QString("来自群聊 %1 的新图片消息（%2）").arg(groupName, from));
                        }

                        // 尝试从服务器下载（保留服务器下载逻辑）
                        downloadImage(imageId);
                    }
                } else {
                    // 如果没有本地路径，尝试从缓存中获取
                    if (m_imageCacheMap.contains(imageId)) {
                        QString cachedPath = m_imageCacheMap[imageId];
                        QFileInfo fileInfo(cachedPath);

                        if (fileInfo.exists() && fileInfo.isReadable()) {
                            // 文件存在且可读，获取图片尺寸
                            QImage image(cachedPath);
                            int width = image.width();
                            int height = image.height();

                            if (width > 0 && height > 0) {
                                // 图片有效，创建JSON对象
                                QJsonObject displayJson;
                                displayJson["type"] = "image";
                                displayJson["imageId"] = imageId;
                                displayJson["localPath"] = cachedPath;
                                displayJson["width"] = width;
                                displayJson["height"] = height;

                                // 如果有缓存，直接显示
                                if (QString::number(groupId) == m_currentChatGroup) {
                                    qDebug() << "为当前选中的群聊发出图片消息信号（已缓存）";
                                    emit groupChatMessageReceived(from, QString::fromUtf8(QJsonDocument(displayJson).toJson()), timestamp, avatarPath);
                                } else {
                                    QString groupName = m_groupNames.value(QString::number(groupId), "未知群聊");
                                    emit statusMessage(QString("来自群聊 %1 的新图片消息（%2）").arg(groupName, from));
                                }
                            } else {
                                // 图片无效，尝试从服务器下载
                                qDebug() << "缓存的图片无效，尝试从服务器下载:" << imageId;
                                downloadImage(imageId);

                                // 先显示占位符
                                if (QString::number(groupId) == m_currentChatGroup) {
                                    qDebug() << "为当前选中的群聊发出图片消息信号（加载中）";
                                    emit groupChatMessageReceived(from, "[图片加载中...]", timestamp, avatarPath);
                                } else {
                                    QString groupName = m_groupNames.value(QString::number(groupId), "未知群聊");
                                    emit statusMessage(QString("来自群聊 %1 的新图片消息（%2）").arg(groupName, from));
                                }
                            }
                        } else {
                            // 文件不存在或不可读，尝试从服务器下载
                            qDebug() << "缓存的图片文件不存在或不可读，尝试从服务器下载:" << imageId;
                            downloadImage(imageId);

                            // 先显示占位符
                            if (QString::number(groupId) == m_currentChatGroup) {
                                qDebug() << "为当前选中的群聊发出图片消息信号（加载中）";
                                emit groupChatMessageReceived(from, "[图片加载中...]", timestamp, avatarPath);
                            } else {
                                QString groupName = m_groupNames.value(QString::number(groupId), "未知群聊");
                                emit statusMessage(QString("来自群聊 %1 的新图片消息（%2）").arg(groupName, from));
                            }
                        }
                    } else {
                        // 如果没有缓存，尝试从服务器下载
                        qDebug() << "没有本地路径和缓存，尝试从服务器下载图片:" << imageId;
                        downloadImage(imageId);

                        // 先显示占位符
                        if (QString::number(groupId) == m_currentChatGroup) {
                            qDebug() << "为当前选中的群聊发出图片消息信号（加载中）";
                            emit groupChatMessageReceived(from, "[图片加载中...]", timestamp, avatarPath);
                        } else {
                            QString groupName = m_groupNames.value(QString::number(groupId), "未知群聊");
                            emit statusMessage(QString("来自群聊 %1 的新图片消息（%2）").arg(groupName, from));
                        }
                    }
                }
            } else if (type == "text") {
                // 文本消息
                QString text = contentObj["text"].toString();
                if (QString::number(groupId) == m_currentChatGroup) {
                    qDebug() << "为当前选中的群聊发出文本消息信号";
                    emit groupChatMessageReceived(from, text, timestamp, avatarPath);
                } else {
                    QString groupName = m_groupNames.value(QString::number(groupId), "未知群聊");
                    emit statusMessage(QString("来自群聊 %1 的新消息（%2：%3）").arg(groupName, from, text));
                }
            } else {
                // 未知类型，显示原始内容
                if (QString::number(groupId) == m_currentChatGroup) {
                    qDebug() << "为当前选中的群聊发出未知类型消息信号";
                    emit groupChatMessageReceived(from, contentStr, timestamp, avatarPath);
                } else {
                    QString groupName = m_groupNames.value(QString::number(groupId), "未知群聊");
                    emit statusMessage(QString("来自群聊 %1 的新消息（%2）").arg(groupName, from));
                }
            }
        } else {
            // 如果不是JSON对象，当作普通文本处理
            if (QString::number(groupId) == m_currentChatGroup) {
                qDebug() << "为当前选中的群聊发出普通文本消息信号";
                emit groupChatMessageReceived(from, contentStr, timestamp, avatarPath);
            } else {
                QString groupName = m_groupNames.value(QString::number(groupId), "未知群聊");
                emit statusMessage(QString("来自群聊 %1 的新消息（%2：%3）").arg(groupName, from, contentStr));
            }
        }
    }
}

void ChatWindow::requestUserProfile() {
    if (!m_isLoggedIn) {
        emit statusMessage("请先登录以查看个人资料");
        return;
    }

    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject data;
        data["nickname"] = m_currentNickname;

        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::GetUserProfile, data)).toJson();
        m_socket->write(request);
        m_socket->flush();
    } else {
        emit statusMessage("未连接到服务器，无法获取个人资料");
    }
}

void ChatWindow::updateUserProfile(const QString &nickname, const QString &signature,
                                 const QString &gender, const QString &birthday,
                                 const QString &location, const QString &phone) {
    if (!m_isLoggedIn) {
        emit statusMessage("请先登录以更新个人资料");
        return;
    }

    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject data;
        data["nickname"] = nickname;
        data["signature"] = signature;
        data["gender"] = gender;
        data["birthday"] = birthday;
        data["location"] = location;
        data["phone"] = phone;

        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::UpdateUserProfile, data)).toJson();
        m_socket->write(request);
        m_socket->flush();
    } else {
        emit statusMessage("未连接到服务器，无法更新个人资料");
    }
}

void ChatWindow::uploadAvatar(const QString &filePath) {
    if (!m_isLoggedIn) {
        emit statusMessage("请先登录以上传头像");
        return;
    }

    // 加载图片文件
    QFile file(QUrl(filePath).toLocalFile());
    if (!file.open(QIODevice::ReadOnly)) {
        emit statusMessage("无法打开图片文件");
        return;
    }

    QByteArray fileData = file.readAll();
    file.close();

    // 检查文件大小
    if (fileData.size() > 5 * 1024 * 1024) { // 5MB最大限制
        emit statusMessage("图片文件过大，请选择小于5MB的文件");
        return;
    }

    // 转换为Base64编码以便在JSON中传输
    QString base64Data = QString::fromLatin1(fileData.toBase64());

    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject data;
        data["nickname"] = m_currentNickname;
        data["avatar_data"] = base64Data;

        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::UploadAvatar, data)).toJson();
        m_socket->write(request);
        m_socket->flush();

        // 保存上传的头像到本地，以便下次使用
        QString permanentDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/avatars/";
        QDir dir(permanentDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 复制当前选择的头像到永久存储位置
        QString permanentPath = permanentDir + m_currentNickname + ".png";

        // 确保文件可以被写入
        QFile destFile(permanentPath);
        if (destFile.exists()) {
            destFile.remove(); // 如果文件已存在，先删除
        }

        if (QFile::copy(QUrl(filePath).toLocalFile(), permanentPath)) {
            // 确保复制后的文件具有正确的权限
            QFile::setPermissions(permanentPath, QFile::ReadOwner | QFile::WriteOwner);

            // 更新缓存
            m_avatarCache[m_currentNickname] = permanentPath;

            // 保存到设置
            QSettings settings;
            settings.beginGroup("AvatarCache");
            settings.setValue(m_currentNickname, permanentPath);
            settings.endGroup();
            settings.sync();

            qDebug() << "已保存上传的头像到本地缓存:" << permanentPath;

            // 发送信号，立即更新UI
            emit avatarReceived(m_currentNickname, permanentPath);
        } else {
            qDebug() << "无法复制头像文件到本地存储:" << permanentPath;
        }

        emit statusMessage("正在上传头像...");
    } else {
        emit statusMessage("未连接到服务器，无法上传头像");
    }
}

void ChatWindow::requestAvatar(const QString &nickname) {
    // 首先尝试从设置中加载缓存路径
    QSettings settings;
    settings.beginGroup("AvatarCache");
    QString settingsPath = settings.value(nickname, "").toString();
    settings.endGroup();

    // 如果设置中有路径但缓存中没有，则更新缓存
    if (!settingsPath.isEmpty() && !m_avatarCache.contains(nickname)) {
        QFileInfo fileInfo(settingsPath);
        if (fileInfo.exists() && fileInfo.isFile() && fileInfo.size() > 0) {
            m_avatarCache[nickname] = settingsPath;
            qDebug() << "从设置中恢复头像缓存:" << settingsPath;
        }
    }

    // 检查缓存中是否存在此用户的头像
    if (m_avatarCache.contains(nickname)) {
        QString cachedPath = m_avatarCache[nickname];
        QFileInfo fileInfo(cachedPath);

        // 验证缓存的文件是否真实存在
        if (fileInfo.exists() && fileInfo.isFile() && fileInfo.size() > 0) {
            qDebug() << "使用缓存的头像:" << cachedPath;
            emit avatarReceived(nickname, cachedPath);
            return;
        } else {
            // 缓存路径无效，从缓存和设置中移除
            qDebug() << "缓存的头像文件不存在或无效:" << cachedPath;
            m_avatarCache.remove(nickname);

            // 从设置中也删除
            settings.beginGroup("AvatarCache");
            settings.remove(nickname);
            settings.endGroup();
            settings.sync();
        }
    }

    // 如果缓存中没有或缓存无效，则请求服务器的头像
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject data;
        data["nickname"] = nickname;

        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::GetAvatar, data)).toJson();

        qDebug() << "向服务器请求用户" << nickname << "的头像";
        m_socket->write(request);
        m_socket->flush(); // 确保立即发送请求
    } else {
        qDebug() << "无法请求头像，socket未连接";
    }
}

QString ChatWindow::getCachedAvatarPath(const QString &nickname) const {
    if (m_avatarCache.contains(nickname)) {
        return "file:///" + m_avatarCache[nickname];
    }
    return "qrc:/images/default_avatar.png";
}

void ChatWindow::sendImageMessage(const QString &localFilePath) {
    if (!m_isLoggedIn) {
        emit statusMessage("请先登录以发送图片。");
        return;
    }

    if (m_isGroupChat && m_currentChatGroup.isEmpty()) {
        emit statusMessage("请选择一个群聊。");
        return;
    } else if (!m_isGroupChat && m_currentChatFriend.isEmpty()) {
        emit statusMessage("请选择一个好友进行聊天。");
        return;
    }

    // 获取本地文件路径
    QString localFile = QUrl(localFilePath).toLocalFile();

    // 加载图片并压缩
    QImage image;
    if (!image.load(localFile)) {
        emit statusMessage("无法加载图片文件");
        return;
    }

    // 获取图片尺寸
    int width = image.width();
    int height = image.height();

    // 如果图片太大，进行压缩
    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);

    // 获取文件扩展名
    QFileInfo fileInfo(localFile);
    QString fileExtension = fileInfo.suffix().toLower();

    // 根据文件类型选择压缩格式
    QString format = fileExtension;
    if (format != "jpg" && format != "jpeg" && format != "png") {
        format = "jpg"; // 默认使用jpg格式
    }

    // 压缩质量 (1-100)，值越小文件越小
    int quality = 85;

    // 如果图片尺寸太大，先调整大小
    const int MAX_DIMENSION = 1920; // 最大尺寸
    if (width > MAX_DIMENSION || height > MAX_DIMENSION) {
        image = image.scaled(
            width > height ? MAX_DIMENSION : width * MAX_DIMENSION / height,
            height > width ? MAX_DIMENSION : height * MAX_DIMENSION / width,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        width = image.width();
        height = image.height();
        qDebug() << "图片已调整大小为:" << width << "x" << height;
    }

    // 保存压缩后的图片
    if (!image.save(&buffer, format.toUtf8().constData(), quality)) {
        emit statusMessage("图片压缩失败");
        return;
    }

    buffer.close();

    // 检查压缩后的文件大小
    if (imageData.size() > 2 * 1024 * 1024) { // 2MB最大限制
        // 如果还是太大，再次压缩
        quality = 60;
        imageData.clear();
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, format.toUtf8().constData(), quality);
        buffer.close();

        // 如果还是太大，再次压缩并调整大小
        if (imageData.size() > 2 * 1024 * 1024) {
            // 再次调整图片大小
            const int REDUCED_MAX_DIMENSION = 1280; // 降低最大尺寸
            if (width > REDUCED_MAX_DIMENSION || height > REDUCED_MAX_DIMENSION) {
                image = image.scaled(
                    width > height ? REDUCED_MAX_DIMENSION : width * REDUCED_MAX_DIMENSION / height,
                    height > width ? REDUCED_MAX_DIMENSION : height * REDUCED_MAX_DIMENSION / width,
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                );
                width = image.width();
                height = image.height();
                qDebug() << "图片已进一步调整大小为:" << width << "x" << height;

                // 使用更低的质量
                quality = 50;
                imageData.clear();
                buffer.open(QIODevice::WriteOnly);
                image.save(&buffer, format.toUtf8().constData(), quality);
                buffer.close();

                if (imageData.size() > 2 * 1024 * 1024) {
                    emit statusMessage("图片文件过大，即使压缩后仍超过2MB");
                    return;
                }
            } else {
                emit statusMessage("图片文件过大，即使压缩后仍超过2MB");
                return;
            }
        }
    }

    qDebug() << "原始图片大小:" << QFile(localFile).size() << "字节，压缩后:" << imageData.size() << "字节";

    // 生成一个临时ID用于跟踪上传
    QString tempId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // 保存图片信息到临时映射
    ImageUploadData uploadData;
    uploadData.localFilePath = localFile;
    uploadData.width = width;
    uploadData.height = height;
    uploadData.fileExtension = format;
    uploadData.imageData = imageData;

    // 检查图片大小，决定是否使用分块上传
    const int CHUNK_SIZE = 8 * 1024; // 8KB 每块 (减小块大小)
    const int MAX_DIRECT_SIZE = 32 * 1024; // 32KB 以下直接上传

    if (imageData.size() > MAX_DIRECT_SIZE) {
        // 使用分块上传
        uploadData.isChunked = true;
        uploadData.chunkSize = CHUNK_SIZE;
        uploadData.totalChunks = (imageData.size() + CHUNK_SIZE - 1) / CHUNK_SIZE; // 向上取整
        uploadData.currentChunk = 0;

        m_pendingImageUploads[tempId] = uploadData;

        // 开始分块上传
        sendImageChunked(tempId);
    } else {
        // 小图片直接上传
        uploadData.isChunked = false;
        m_pendingImageUploads[tempId] = uploadData;

        // 发送上传请求
        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            QJsonObject data;
            data["image_data_base64"] = QString::fromLatin1(imageData.toBase64());
            data["file_extension"] = format;
            data["temp_id"] = tempId;

            QByteArray request = QJsonDocument(MessageProtocol::createMessage(
                MessageType::UploadImageRequest, data)).toJson();

            qDebug() << "发送图片上传请求，数据大小:" << request.size() << "字节";
            m_socket->write(request);
            m_socket->flush();

            emit statusMessage("正在上传图片...");
        } else {
            emit statusMessage("未连接到服务器，无法上传图片");
            m_pendingImageUploads.remove(tempId);
        }
    }
}

QString ChatWindow::getCachedImagePath(const QString &imageId) const {
    if (m_imageCacheMap.contains(imageId)) {
        return "file:///" + m_imageCacheMap[imageId];
    }
    return "";
}

QString ChatWindow::getImageCachePath(const QString &imageId) const {
    if (m_imageCacheMap.contains(imageId)) {
        return m_imageCacheMap[imageId];
    }
    return "";
}

void ChatWindow::downloadImage(const QString &imageId) {
    // 检查缓存中是否已有此图片
    if (m_imageCacheMap.contains(imageId)) {
        QString cachedPath = m_imageCacheMap[imageId];
        QFileInfo fileInfo(cachedPath);

        // 验证缓存的文件是否真实存在
        if (fileInfo.exists() && fileInfo.isFile() && fileInfo.size() > 0) {
            qDebug() << "使用缓存的图片:" << cachedPath;
            emit imageDownloaded(imageId, cachedPath);
            return;
        } else {
            // 缓存路径无效，从缓存中移除
            qDebug() << "缓存的图片文件不存在或无效:" << cachedPath;
            m_imageCacheMap.remove(imageId);

            // 从设置中也删除
            QSettings settings;
            settings.beginGroup("ImageCache");
            settings.remove(imageId);
            settings.endGroup();
            settings.sync();
        }
    }

    // 如果缓存中没有或缓存无效，则请求服务器的图片
    if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
        QJsonObject data;
        data["imageId"] = imageId;

        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::DownloadImageRequest, data)).toJson();

        qDebug() << "向服务器请求图片ID:" << imageId;
        m_socket->write(request);
        m_socket->flush(); // 确保立即发送请求
    } else {
        qDebug() << "无法请求图片，socket未连接";
    }
}

void ChatWindow::handleUploadImageResponse(const QJsonObject &msgData) {
    QString status = msgData["status"].toString();

    if (status == "success") {
        QString imageId = msgData["imageId"].toString();
        QString tempId = msgData.value("temp_id").toString();

        qDebug() << "图片上传成功，ID:" << imageId << "，临时ID:" << tempId;

        // 检查是否有对应的临时上传数据
        if (m_pendingImageUploads.contains(tempId)) {
            ImageUploadData uploadData = m_pendingImageUploads.take(tempId);

            // 保存图片到本地缓存
            QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QString imageCacheDir = appDataPath + "/image_cache/";
            QDir dir(imageCacheDir);
            if (!dir.exists()) {
                dir.mkpath(".");
            }

            // 获取文件扩展名
            QFileInfo fileInfo(uploadData.localFilePath);
            QString fileExtension = fileInfo.suffix().toLower();

            // 创建缓存文件路径
            QString cachedPath = imageCacheDir + imageId;

            // 复制文件到缓存
            if (QFile::copy(uploadData.localFilePath, cachedPath)) {
                // 确保复制后的文件具有正确的权限
                QFile::setPermissions(cachedPath, QFile::ReadOwner | QFile::WriteOwner);

                // 更新缓存映射
                m_imageCacheMap[imageId] = cachedPath;

                // 保存到设置
                QSettings settings;
                settings.beginGroup("ImageCache");
                settings.setValue(imageId, cachedPath);
                settings.endGroup();
                settings.sync();

                qDebug() << "图片已缓存到:" << cachedPath;

                // 发送信号通知上传成功
                emit imageUploadSuccess(imageId);

                // 创建图片消息内容
                QJsonObject imageContent;
                imageContent["type"] = "image";
                imageContent["imageId"] = imageId;
                imageContent["width"] = uploadData.width;
                imageContent["height"] = uploadData.height;
                imageContent["localPath"] = cachedPath;

                // 发送消息
                if (m_isGroupChat) {
                    sendGroupMessageInternal(imageContent);
                } else {
                    sendMessageInternal(imageContent);
                }
            } else {
                qDebug() << "无法复制图片到缓存:" << cachedPath;
                emit statusMessage("图片缓存失败，但消息已发送");

                // 即使缓存失败，也发送消息
                QJsonObject imageContent;
                imageContent["type"] = "image";
                imageContent["imageId"] = imageId;
                imageContent["width"] = uploadData.width;
                imageContent["height"] = uploadData.height;

                // 发送消息
                if (m_isGroupChat) {
                    sendGroupMessageInternal(imageContent);
                } else {
                    sendMessageInternal(imageContent);
                }
            }
        } else {
            qDebug() << "找不到对应的临时上传数据:" << tempId;
        }
    } else {
        QString reason = msgData["reason"].toString("未知错误");
        qDebug() << "图片上传失败:" << reason;
        emit statusMessage("图片上传失败: " + reason);
    }
}

void ChatWindow::handleDownloadImageResponse(const QJsonObject &msgData) {
    QString status = msgData["status"].toString();
    QString imageId = msgData["imageId"].toString();

    qDebug() << "收到图片下载响应，状态:" << status << "，图片ID:" << imageId;

    if (status == "success") {
        QString imageDataBase64 = msgData["image_data_base64"].toString();

        if (imageDataBase64.isEmpty()) {
            qDebug() << "图片数据为空，重新请求图片:" << imageId;
            downloadImage(imageId);
            return;
        }

        QByteArray imageData = QByteArray::fromBase64(imageDataBase64.toLatin1());

        if (imageData.isEmpty()) {
            qDebug() << "Base64解码后图片数据为空，重新请求图片:" << imageId;
            downloadImage(imageId);
            return;
        }

        qDebug() << "图片数据大小:" << imageData.size() << "字节";

        // 保存图片到本地缓存
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QString imageCacheDir = appDataPath + "/image_cache/";
        QDir dir(imageCacheDir);
        if (!dir.exists()) {
            bool created = dir.mkpath(".");
            qDebug() << "创建图片缓存目录结果:" << created << ", 路径:" << dir.absolutePath();

            if (!created) {
                qDebug() << "无法创建图片缓存目录，尝试使用临时目录";
                imageCacheDir = QDir::tempPath() + "/ModernChat/image_cache/";
                dir = QDir(imageCacheDir);
                if (!dir.exists()) {
                    created = dir.mkpath(".");
                    qDebug() << "创建临时图片缓存目录结果:" << created << ", 路径:" << dir.absolutePath();

                    if (!created) {
                        qDebug() << "无法创建临时图片缓存目录，图片将无法保存";
                        emit statusMessage("图片下载失败: 无法创建缓存目录");
                        return;
                    }
                }
            }
        }

        // 创建缓存文件路径
        QString cachedPath = imageCacheDir + imageId;

        qDebug() << "准备保存图片到:" << cachedPath;

        // 保存文件
        QFile file(cachedPath);
        if (file.open(QIODevice::WriteOnly)) {
            qint64 bytesWritten = file.write(imageData);
            file.close();

            if (bytesWritten == imageData.size()) {
                qDebug() << "图片保存成功:" << cachedPath;

                // 设置文件权限确保可读写
                QFile::setPermissions(cachedPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);

                // 更新缓存映射
                m_imageCacheMap[imageId] = cachedPath;

                // 保存到设置
                QSettings settings;
                settings.beginGroup("ImageCache");
                settings.setValue(imageId, cachedPath);
                settings.endGroup();
                settings.sync();

                qDebug() << "下载的图片已缓存到:" << cachedPath;

                // 发送信号通知下载成功
                emit imageDownloaded(imageId, cachedPath);

                // 更新所有显示该图片的消息
                updateImageMessages(imageId, cachedPath);

                // 验证文件是否真的存在且可读
                QFile checkFile(cachedPath);
                if (checkFile.exists() && checkFile.open(QIODevice::ReadOnly)) {
                    QByteArray checkData = checkFile.readAll();
                    checkFile.close();

                    if (checkData.size() == imageData.size()) {
                        qDebug() << "图片文件验证成功，大小:" << checkData.size() << "字节";
                    } else {
                        qDebug() << "警告：图片文件验证大小不匹配，预期:" << imageData.size() << "，实际:" << checkData.size();
                    }
                } else {
                    qDebug() << "警告：无法验证图片文件，可能无法正常显示";
                }
            } else {
                qDebug() << "图片保存失败: 写入的字节数不匹配，预期:" << imageData.size() << "，实际:" << bytesWritten;
                emit statusMessage("图片下载失败: 写入文件错误");

                // 尝试使用临时文件
                QString tempPath = QDir::tempPath() + "/" + imageId;
                QFile tempFile(tempPath);
                if (tempFile.open(QIODevice::WriteOnly)) {
                    qint64 tempBytesWritten = tempFile.write(imageData);
                    tempFile.close();

                    if (tempBytesWritten == imageData.size()) {
                        qDebug() << "图片保存到临时文件成功:" << tempPath;

                        // 更新缓存映射
                        m_imageCacheMap[imageId] = tempPath;

                        // 更新所有显示该图片的消息
                        updateImageMessages(imageId, tempPath);
                    }
                }
            }
        } else {
            qDebug() << "无法打开文件进行写入:" << cachedPath << ", 错误:" << file.errorString();
            emit statusMessage("图片下载失败: 无法保存文件");

            // 尝试使用临时文件
            QString tempPath = QDir::tempPath() + "/" + imageId;
            QFile tempFile(tempPath);
            if (tempFile.open(QIODevice::WriteOnly)) {
                qint64 tempBytesWritten = tempFile.write(imageData);
                tempFile.close();

                if (tempBytesWritten == imageData.size()) {
                    qDebug() << "图片保存到临时文件成功:" << tempPath;

                    // 更新缓存映射
                    m_imageCacheMap[imageId] = tempPath;

                    // 更新所有显示该图片的消息
                    updateImageMessages(imageId, tempPath);
                }
            }
        }
    } else {
        QString reason = msgData["reason"].toString("未知错误");
        qDebug() << "图片下载失败:" << reason;
        emit statusMessage("图片下载失败: " + reason);

        // 如果失败，稍后重试一次
        QTimer::singleShot(2000, this, [this, imageId]() {
            qDebug() << "重新尝试下载图片:" << imageId;
            downloadImage(imageId);
        });
    }
}

// 更新所有显示该图片的消息
void ChatWindow::updateImageMessages(const QString &imageId, const QString &localPath) {
    // 检查文件是否存在
    QFileInfo fileInfo(localPath);
    if (!fileInfo.exists() || !fileInfo.isReadable()) {
        qDebug() << "警告：图片文件不存在或不可读:" << localPath;

        // 尝试重新下载
        QTimer::singleShot(1000, this, [this, imageId]() {
            qDebug() << "图片文件不存在，重新请求下载:" << imageId;
            downloadImage(imageId);
        });

        return;
    }

    // 获取图片尺寸
    QImage image(localPath);
    int width = image.width();
    int height = image.height();

    if (width <= 0 || height <= 0) {
        qDebug() << "警告：无法获取图片尺寸，可能是无效图片:" << localPath;

        // 尝试重新下载
        QTimer::singleShot(1000, this, [this, imageId]() {
            qDebug() << "图片无效，重新请求下载:" << imageId;
            downloadImage(imageId);
        });

        return;
    }

    qDebug() << "通知UI更新图片，ID:" << imageId << "，本地路径:" << localPath << "，尺寸:" << width << "x" << height;

    // 创建包含图片信息的JSON对象
    QJsonObject imageJson;
    imageJson["type"] = "image";
    imageJson["imageId"] = imageId;
    imageJson["localPath"] = localPath;
    imageJson["width"] = width;
    imageJson["height"] = height;

    // 发送信号通知UI更新图片
    emit imageMessageUpdated(imageId, QString::fromUtf8(QJsonDocument(imageJson).toJson()));

    // 查找并更新所有包含"[图片加载中...]"的消息
    emit updatePlaceholderMessages(imageId, QString::fromUtf8(QJsonDocument(imageJson).toJson()));

    // 延迟一段时间后再次发送更新信号，确保UI能够正确更新
    QTimer::singleShot(500, this, [this, imageId, localPath, width, height]() {
        QJsonObject delayedImageJson;
        delayedImageJson["type"] = "image";
        delayedImageJson["imageId"] = imageId;
        delayedImageJson["localPath"] = localPath;
        delayedImageJson["width"] = width;
        delayedImageJson["height"] = height;

        qDebug() << "延迟发送图片更新信号，ID:" << imageId;
        emit imageMessageUpdated(imageId, QString::fromUtf8(QJsonDocument(delayedImageJson).toJson()));

        // 再次尝试更新占位符消息
        emit updatePlaceholderMessages(imageId, QString::fromUtf8(QJsonDocument(delayedImageJson).toJson()));
    });
}

// 开始分块上传图片
void ChatWindow::sendImageChunked(const QString &tempId) {
    if (!m_pendingImageUploads.contains(tempId)) {
        qDebug() << "找不到待上传的图片:" << tempId;
        return;
    }

    ImageUploadData &uploadData = m_pendingImageUploads[tempId];

    // 发送开始分块上传消息
    QJsonObject startData;
    startData["temp_id"] = tempId;
    startData["total_chunks"] = uploadData.totalChunks;
    startData["file_extension"] = uploadData.fileExtension;
    startData["total_size"] = uploadData.imageData.size();
    startData["width"] = uploadData.width;
    startData["height"] = uploadData.height;

    QByteArray request = QJsonDocument(MessageProtocol::createMessage(
        MessageType::ChunkedImageStart, startData)).toJson();

    qDebug() << "开始分块上传图片，总块数:" << uploadData.totalChunks
             << "，总大小:" << uploadData.imageData.size() << "字节";

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(request);
        m_socket->flush();

        emit statusMessage("正在上传图片...(0/" + QString::number(uploadData.totalChunks) + ")");

        // 发送第一个数据块
        sendNextImageChunk(tempId);
    } else {
        emit statusMessage("未连接到服务器，无法上传图片");
        m_pendingImageUploads.remove(tempId);
    }
}

// 发送下一个图片数据块
void ChatWindow::sendNextImageChunk(const QString &tempId) {
    if (!m_pendingImageUploads.contains(tempId)) {
        qDebug() << "找不到待上传的图片:" << tempId;
        return;
    }

    ImageUploadData &uploadData = m_pendingImageUploads[tempId];

    // 检查是否已发送完所有块
    if (uploadData.currentChunk >= uploadData.totalChunks) {
        // 发送结束消息
        QJsonObject endData;
        endData["temp_id"] = tempId;
        endData["total_chunks"] = uploadData.totalChunks;

        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::ChunkedImageEnd, endData)).toJson();

        qDebug() << "图片分块上传完成，发送结束消息";

        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            m_socket->write(request);
            m_socket->flush();

            emit statusMessage("图片上传完成，等待服务器处理...");
        }
        return;
    }

    // 计算当前块的起始位置和大小
    int startPos = uploadData.currentChunk * uploadData.chunkSize;
    int chunkSize = qMin(uploadData.chunkSize, uploadData.imageData.size() - startPos);

    // 提取当前块的数据
    QByteArray chunkData = uploadData.imageData.mid(startPos, chunkSize);

    // 发送数据块
    QJsonObject chunkObj;
    chunkObj["temp_id"] = tempId;
    chunkObj["chunk_index"] = uploadData.currentChunk;
    chunkObj["chunk_data"] = QString::fromLatin1(chunkData.toBase64());

    QByteArray request = QJsonDocument(MessageProtocol::createMessage(
        MessageType::ChunkedImageChunk, chunkObj)).toJson();

    qDebug() << "发送图片数据块:" << uploadData.currentChunk + 1 << "/" << uploadData.totalChunks
             << "，大小:" << chunkData.size() << "字节";

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(request);
        m_socket->flush();

        // 更新状态消息
        emit statusMessage("正在上传图片...(" +
                          QString::number(uploadData.currentChunk + 1) + "/" +
                          QString::number(uploadData.totalChunks) + ")");

        // 增加当前块索引
        uploadData.currentChunk++;

        // 添加延迟，避免发送过快导致服务器处理不过来
        // 使用QTimer延迟50毫秒后发送下一个块
        QTimer::singleShot(50, this, [this, tempId]() {
            sendNextImageChunk(tempId);
        });
    } else {
        emit statusMessage("未连接到服务器，无法上传图片");
        m_pendingImageUploads.remove(tempId);
    }
}

// 处理分块图片上传响应
void ChatWindow::handleChunkedImageResponse(const QJsonObject &msgData) {
    QString tempId = msgData["temp_id"].toString();
    QString status = msgData["status"].toString();

    if (status == "success") {
        QString imageId = msgData["image_id"].toString();
        qDebug() << "分块图片上传成功，ID:" << imageId;

        // 检查是否有待处理的上传
        if (m_pendingImageUploads.contains(tempId)) {
            ImageUploadData uploadData = m_pendingImageUploads.take(tempId);

            // 保存图片到本地缓存
            QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QString imageCacheDir = appDataPath + "/image_cache/";
            QDir dir(imageCacheDir);
            if (!dir.exists()) {
                dir.mkpath(".");
            }

            // 创建缓存文件路径
            QString cachedPath = imageCacheDir + imageId;

            // 保存文件
            QFile file(cachedPath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(uploadData.imageData);
                file.close();

                // 更新缓存映射
                m_imageCacheMap[imageId] = cachedPath;

                // 保存到设置
                QSettings settings;
                settings.beginGroup("ImageCache");
                settings.setValue(imageId, cachedPath);
                settings.endGroup();
                settings.sync();

                qDebug() << "上传的图片已缓存到:" << cachedPath;

                // 创建图片消息内容
                QJsonObject imageContent;
                imageContent["type"] = "image";
                imageContent["imageId"] = imageId;
                imageContent["width"] = uploadData.width;
                imageContent["height"] = uploadData.height;
                imageContent["localPath"] = cachedPath;

                // 发送消息
                if (m_isGroupChat) {
                    sendGroupMessageInternal(imageContent);
                } else {
                    sendMessageInternal(imageContent);
                }

                emit imageUploadSuccess(imageId);
                emit statusMessage("图片发送成功");

                // 更新所有显示该图片的消息
                updateImageMessages(imageId, cachedPath);
            } else {
                qDebug() << "无法保存上传的图片:" << file.errorString();

                // 即使缓存失败，也发送消息
                QJsonObject imageContent;
                imageContent["type"] = "image";
                imageContent["imageId"] = imageId;
                imageContent["width"] = uploadData.width;
                imageContent["height"] = uploadData.height;

                // 发送消息
                if (m_isGroupChat) {
                    sendGroupMessageInternal(imageContent);
                } else {
                    sendMessageInternal(imageContent);
                }

                emit imageUploadSuccess(imageId);
                emit statusMessage("图片发送成功，但本地缓存失败");
            }
        }
    } else {
        QString reason = msgData["reason"].toString("未知错误");
        qDebug() << "分块图片上传失败:" << reason;
        emit statusMessage("图片上传失败: " + reason);

        // 清理待处理的上传
        if (m_pendingImageUploads.contains(tempId)) {
            m_pendingImageUploads.remove(tempId);
        }
    }
}