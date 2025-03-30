#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QGraphicsDropShadowEffect>
#include <QListWidgetItem>
#include "../Common/messageprotocol.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ChatWindow; }
QT_END_NAMESPACE

class ChatWindow : public QMainWindow {
    Q_OBJECT

public:
    ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow();

private slots:
    void on_showLoginButton_clicked();
    void on_showRegisterButton_clicked();
    void on_authButton_clicked();
    void on_sendButton_clicked();
    void handleServerData();
    void on_searchButton_clicked();
    void on_addFriendButton_clicked();
    void on_friendList_itemClicked(QListWidgetItem *item);

private:
    Ui::ChatWindow *ui;
    QTcpSocket *socket;
    bool isLoggedIn = false;
    bool isLoginMode = true;
    QString currentNickname; // 当前登录用户昵称
    QString currentChatFriend; // 当前聊天对象
    QGraphicsDropShadowEffect *authShadow;
    QGraphicsDropShadowEffect *chatShadow;
    QGraphicsDropShadowEffect *inputShadow;

    void updateFriendList(const QStringList &friends);
    void loadChatHistory(const QString &friendName);
};

#endif // CHATWINDOW_H