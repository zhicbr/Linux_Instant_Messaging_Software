#include "chatwindow.h"
#include <QJsonDocument>
#include <QDebug>
#include <QDateTime>
#include "../Common/config.h"

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
        m_socket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Chat, data)).toJson());
        // 立即在本地显示消息
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm");
        appendMessage(m_currentNickname, message, timestamp);
    } else {
        emit statusMessage("未连接到服务器，无法发送消息。");
    }
}

void ChatWindow::selectFriend(const QString &friendName)
{
    if (friendName != m_currentChatFriend) {
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

void ChatWindow::addFriend(const QString &friendName)
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
            MessageType::AddFriend, {{"friend", friendName}})).toJson());
    } else {
        emit statusMessage("未连接到服务器。");
    }
}

QStringList ChatWindow::getFriendList() const
{
    return m_friendList;
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

void ChatWindow::handleServerData()
{
    QByteArray data = m_socket->readAll();
    MessageType type;
    QJsonObject msgData;

    if (!MessageProtocol::parseMessage(data, type, msgData)) {
        qWarning() << "无法解析消息：" << data;
        return;
    }

    qDebug() << "收到消息类型：" << static_cast<int>(type) << "数据：" << msgData;

    switch (type) {
    case MessageType::Login:
        if (msgData.value("status").toString() == "success") {
            m_isLoggedIn = true;
            emit isLoggedInChanged();
            emit statusMessage(QString("以 %1 身份登录").arg(m_currentNickname));
            clearChatDisplay();
            m_currentChatFriend.clear();
            emit currentChatFriendChanged();
            // 请求好友列表
            m_socket->write(QJsonDocument(MessageProtocol::createMessage(
                MessageType::GetFriendList, {})).toJson());
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

    case MessageType::Chat:
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

    case MessageType::GetFriendList:
        if (msgData.value("status").toString() == "success") {
            QStringList friends;
            QJsonArray friendArray = msgData.value("friends").toArray();
            for (const QJsonValue &value : friendArray) {
                friends << value.toString();
            }
            friends.sort(Qt::CaseInsensitive);
            updateFriendList(friends);
        } else {
            emit statusMessage("无法获取好友列表。");
        }
        break;

    case MessageType::GetChatHistory:
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
                MessageType::GetFriendList, {})).toJson());
        } else {
            emit statusMessage("添加好友失败：" + msgData.value("reason").toString("无法添加好友"));
        }
        break;

    case MessageType::Error:
        emit statusMessage("服务器错误：" + msgData.value("reason").toString("发生未知错误"));
        break;

    default:
        qWarning() << "未处理的消息类型：" << static_cast<int>(type);
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
            MessageType::GetChatHistory, data)).toJson());
    } else {
        emit statusMessage("未连接到服务器，无法加载聊天记录。");
    }
}