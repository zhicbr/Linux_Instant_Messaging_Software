#include "chatwindow.h"
#include "ui_chatwindow.h"
#include <QJsonDocument> // 添加 QJsonDocument 头文件
#include "../Common/config.h"

ChatWindow::ChatWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::ChatWindow) {
    ui->setupUi(this);
    socket = new QTcpSocket(this);
    socket->connectToHost(Config::DefaultServerIp, Config::DefaultPort);
    connect(socket, &QTcpSocket::readyRead, this, &ChatWindow::handleServerData);
}

ChatWindow::~ChatWindow() {
    delete ui;
}

void ChatWindow::on_loginButton_clicked() {
    QJsonObject data;
    data["username"] = ui->usernameEdit->text();
    socket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Login, data)).toJson());
}

void ChatWindow::on_sendButton_clicked() {
    QJsonObject data;
    data["content"] = ui->messageEdit->text();
    socket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Chat, data)).toJson());
    ui->messageEdit->clear();
}

void ChatWindow::handleServerData() {
    QByteArray data = socket->readAll();
    MessageType type;
    QJsonObject msgData;
    if (MessageProtocol::parseMessage(data, type, msgData)) {
        if (type == MessageType::Login) {
            ui->chatDisplay->append("Logged in as: " + ui->usernameEdit->text());
        } else if (type == MessageType::Chat) {
            ui->chatDisplay->append(msgData["content"].toString());
        }
    }
}
