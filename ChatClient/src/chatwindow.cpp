#include "chatwindow.h"
#include <QJsonDocument>
#include <QDebug>
#include <QDateTime>
#include "../Common/config.h"
#include <QTimer>

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
}

ChatWindow::~ChatWindow()
{
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
    QJsonObject data{
        {"to", m_currentChatFriend},
        {"content", message}
    };
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Message, data)).toJson());
        // 立即在本地显示消息
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm");
        appendMessage(m_currentNickname, message, timestamp);
    } else {
        emit statusMessage("未连接到服务器，无法发送消息。");
    }
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
    emit messageReceived(sender, content, timestamp);
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
    QByteArray data = m_socket->readAll();
    qDebug() << "收到消息：" << data;

    MessageType type;
    QJsonObject msgData;
    if (MessageProtocol::parseMessage(data, type, msgData)) {
        qDebug() << "解析后的消息类型：" << MessageProtocol::messageTypeToString(type) << "(" << static_cast<int>(type) << "), 内容：" << msgData;
        
        // 处理 AcceptFriend 和 DeleteFriendRequest 消息的特殊逻辑
        if (type == MessageType::AcceptFriend) {
            handleAcceptFriendMessage(msgData);
            return;
        } else if (type == MessageType::DeleteFriendRequest) {
            handleDeleteFriendRequestMessage(msgData);
            return;
        } else if (type == MessageType::CreateGroup) {
            handleCreateGroupMessage(msgData);
            return;
        } else if (type == MessageType::GroupChat && msgData.contains("from")) {
            handleGroupChatMessage(msgData);
            return;
        }
        
        // 处理其他消息类型
        switch (type) {
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

        case MessageType::Register:
            if (msgData.value("status").toString() == "success") {
                emit statusMessage("注册成功！请使用新账户登录。");
            } else {
                emit statusMessage("注册失败：" + msgData.value("reason").toString("未知错误"));
            }
            break;

        case MessageType::Message:
            {
                QString from = msgData.value("from").toString();
                QString content = msgData.value("content").toString();
                QString timestamp = QDateTime::currentDateTime().toString("hh:mm");
                if (from == m_currentChatFriend) {
                    appendMessage(from, content, timestamp);
                } else {
                    emit statusMessage(QString("来自 %1 的新消息").arg(from));
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
                        QString content = msgObj.value("content").toString();
                        QString timestamp = QDateTime::currentDateTime().toString("hh:mm");
                        appendMessage(sender, content, timestamp);
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

        // 群聊相关消息处理
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
            // 仅处理服务器的确认响应，实际消息在handleGroupChatMessage中处理
            if (msgData.contains("status") && msgData["status"].toString() == "failed") {
                emit statusMessage("发送群聊消息失败：" + msgData["reason"].toString("未知错误"));
            }
            break;
            
        case MessageType::GroupChatHistory:
            if (msgData["status"].toString() == "success") {
                clearChatDisplay();
                QJsonArray messages = msgData["messages"].toArray();
                if (messages.isEmpty()) {
                    // 使用群聊消息信号显示空消息提示
                    emit groupChatMessageReceived("系统", "尚无群聊消息记录。", "");
                    qDebug() << "群聊历史为空，发送空消息提示";
                } else {
                    qDebug() << "收到群聊历史记录，消息数量:" << messages.size();
                    for (const QJsonValue &msgValue : messages) {
                        QJsonObject msgObj = msgValue.toObject();
                        QString sender = msgObj["from"].toString();
                        QString content = msgObj["content"].toString();
                        QString timestamp = msgObj.contains("timestamp") ? 
                            msgObj["timestamp"].toString() : QDateTime::currentDateTime().toString("hh:mm");
                        // 使用群聊消息信号而不是普通消息信号
                        emit groupChatMessageReceived(sender, content, timestamp);
                        qDebug() << "发送群聊历史消息，发送者:" << sender << "，内容:" << content;
                    }
                }
            } else {
                emit statusMessage("无法获取群聊历史记录");
            }
            break;

        default:
            qWarning() << "未处理的消息类型：" << static_cast<int>(type);
            break;
        }
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

// 添加新的处理函数
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

// 群聊相关函数实现
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
    
    // 设置群聊状态
    m_currentChatGroup = groupId;
    m_isGroupChat = true;
    qDebug() << "设置isGroupChat为true，当前值:" << m_isGroupChat;
    emit currentChatGroupChanged();
    emit isGroupChatChanged();
    
    // 清除聊天显示
    clearChatDisplay();
    
    // 获取群成员列表
    getGroupMembers(groupId);
    
    // 加载群聊历史
    loadGroupChatHistory(groupId);
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
    
    QJsonObject data;
    data["group_id"] = m_currentChatGroup.toInt();
    data["content"] = message;
    
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray request = QJsonDocument(MessageProtocol::createMessage(
            MessageType::GroupChat, data)).toJson();
        m_socket->write(request);
        m_socket->flush();
        
        // 立即在本地显示消息
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm");
        // 使用groupChatMessageReceived信号替代appendMessage，以保持一致的消息处理
        qDebug() << "发出groupChatMessageReceived信号，发送者:" << m_currentNickname << "内容:" << message;
        emit groupChatMessageReceived(m_currentNickname, message, timestamp);
    } else {
        emit statusMessage("未连接到服务器，无法发送群聊消息。");
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
        QString content = msgData["content"].toString();
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm");
        int groupId = msgData["group_id"].toInt();
        
        qDebug() << "收到群聊消息，来自:" << from << "，内容:" << content << "，群ID:" << groupId
                 << "，当前群ID:" << m_currentChatGroup << "，是否为群聊模式:" << m_isGroupChat;
        
        if (QString::number(groupId) == m_currentChatGroup) {
            // 发出群聊消息信号，而不是直接添加到消息模型
            qDebug() << "为当前选中的群聊发出消息信号";
            emit groupChatMessageReceived(from, content, timestamp);
        } else {
            QString groupName = m_groupNames.value(QString::number(groupId), "未知群聊");
            emit statusMessage(QString("来自群聊 %1 的新消息（%2：%3）").arg(groupName, from, content));
        }
    }
}