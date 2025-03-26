#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
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

private:
    Ui::ChatWindow *ui;
    QTcpSocket *socket;
    bool isLoggedIn = false; // 记录登录状态
    bool isLoginMode = true; // 记录当前是登录模式还是注册模式
};

#endif
