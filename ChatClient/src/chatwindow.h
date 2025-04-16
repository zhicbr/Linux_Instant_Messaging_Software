#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QObject>
#include <QTcpSocket>
#include <QJsonArray>
#include "../Common/messageprotocol.h"

class ChatWindow : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY isLoggedInChanged)
    Q_PROPERTY(QString currentNickname READ currentNickname NOTIFY currentNicknameChanged)
    Q_PROPERTY(QString currentChatFriend READ currentChatFriend NOTIFY currentChatFriendChanged)

public:
    explicit ChatWindow(QObject *parent = nullptr);
    ~ChatWindow();

    bool isLoggedIn() const { return m_isLoggedIn; }
    QString currentNickname() const { return m_currentNickname; }
    QString currentChatFriend() const { return m_currentChatFriend; }

    // QML 可调用的方法
    Q_INVOKABLE void login(const QString &nickname, const QString &password);
    Q_INVOKABLE void registerUser(const QString &email, const QString &nickname, const QString &password);
    Q_INVOKABLE void sendMessage(const QString &content);
    Q_INVOKABLE void selectFriend(const QString &friendName);
    Q_INVOKABLE void searchUser(const QString &query);
    Q_INVOKABLE void addFriend(const QString &friendName);
    Q_INVOKABLE QStringList getFriendList() const;

    // 更新 QML 界面的方法
    Q_INVOKABLE void appendMessage(const QString &sender, const QString &content, const QString &timestamp);
    Q_INVOKABLE void clearChatDisplay();
    Q_INVOKABLE void setStatusMessage(const QString &message);
    Q_INVOKABLE void updateFriendList(const QStringList &friends);

signals:
    void isLoggedInChanged();
    void currentNicknameChanged();
    void currentChatFriendChanged();
    void messageReceived(const QString &sender, const QString &content, const QString &timestamp);
    void statusMessage(const QString &message);
    void friendListUpdated(const QStringList &friends);
    void chatDisplayCleared();

private slots:
    void handleServerData();
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket *m_socket;
    bool m_isLoggedIn = false;
    QString m_currentNickname;
    QString m_currentChatFriend;
    QStringList m_friendList;

    void loadChatHistory(const QString &friendName);
};

#endif // CHATWINDOW_H