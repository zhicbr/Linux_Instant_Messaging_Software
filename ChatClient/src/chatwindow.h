#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QMainWindow>
#include <QTcpSocket> // 添加 QTcpSocket 头文件
#include "../Common/messageprotocol.h"

namespace Ui { class ChatWindow; }

class ChatWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow();

private slots:
    void on_sendButton_clicked();
    void on_loginButton_clicked();
    void handleServerData();

private:
    Ui::ChatWindow *ui;
    QTcpSocket *socket;
};

#endif
