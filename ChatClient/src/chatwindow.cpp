#include "chatwindow.h"
#include "ui_chatwindow.h"
#include <QJsonDocument>
#include <QMessageBox>
#include <QDebug>
#include "../Common/config.h"

ChatWindow::ChatWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::ChatWindow) {
    ui->setupUi(this);
    socket = new QTcpSocket(this);
    qDebug() << "Connecting to server at" << Config::DefaultServerIp << ":" << Config::DefaultPort;
    socket->connectToHost(Config::DefaultServerIp, Config::DefaultPort);
    if (!socket->waitForConnected(5000)) {
        qDebug() << "Failed to connect to server:" << socket->errorString();
        QMessageBox::critical(this, "Connection Error", "Failed to connect to server: " + socket->errorString());
    }
    connect(socket, &QTcpSocket::readyRead, this, &ChatWindow::handleServerData);

    // 初始化时显示登录页面
    ui->stackedWidget->setCurrentWidget(ui->authPage);
    ui->authStackedWidget->setCurrentWidget(ui->loginPage);
    ui->authButton->setText("Sign In");
    isLoginMode = true;
    ui->showLoginButton->setChecked(true);
    ui->showRegisterButton->setChecked(false);

    // 初始化时禁用发送按钮
    ui->sendButton->setEnabled(false);

    // 添加阴影效果
    authShadow = new QGraphicsDropShadowEffect(this);
    authShadow->setBlurRadius(10);
    authShadow->setXOffset(0);
    authShadow->setYOffset(2);
    authShadow->setColor(QColor(0, 0, 0, 60)); // Subtle gray shadow
    ui->authContainer->setGraphicsEffect(authShadow);

    chatShadow = new QGraphicsDropShadowEffect(this);
    chatShadow->setBlurRadius(10);
    chatShadow->setXOffset(0);
    chatShadow->setYOffset(2);
    chatShadow->setColor(QColor(0, 0, 0, 60));
    ui->chatDisplay->setGraphicsEffect(chatShadow);

    inputShadow = new QGraphicsDropShadowEffect(this);
    inputShadow->setBlurRadius(5);
    inputShadow->setXOffset(0);
    inputShadow->setYOffset(1);
    inputShadow->setColor(QColor(0, 0, 0, 40));
    ui->messageEdit->setGraphicsEffect(inputShadow);
}

ChatWindow::~ChatWindow() {
    delete ui;
}

void ChatWindow::on_showLoginButton_clicked() {
    ui->authStackedWidget->setCurrentWidget(ui->loginPage);
    ui->authButton->setText("Sign In");
    isLoginMode = true;
    ui->showLoginButton->setChecked(true);
    ui->showRegisterButton->setChecked(false);
}

void ChatWindow::on_showRegisterButton_clicked() {
    ui->authStackedWidget->setCurrentWidget(ui->registerPage);
    ui->authButton->setText("Sign Up");
    isLoginMode = false;
    ui->showRegisterButton->setChecked(true);
    ui->showLoginButton->setChecked(false);
}

void ChatWindow::on_authButton_clicked() {
    QJsonObject data;
    if (isLoginMode) {
        data["nickname"] = ui->loginNicknameEdit->text();
        data["password"] = ui->loginPasswordEdit->text();
        QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::Login, data)).toJson();
        qDebug() << "Sending login message:" << message;
        socket->write(message);
    } else {
        data["email"] = ui->emailEdit->text();
        data["nickname"] = ui->nicknameEdit->text();
        data["password"] = ui->passwordEdit->text();
        QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::Register, data)).toJson();
        qDebug() << "Sending register message:" << message;
        socket->write(message);
    }
}

void ChatWindow::on_sendButton_clicked() {
    if (!isLoggedIn) {
        QMessageBox::warning(this, "Send Message", "Please sign in first!");
        return;
    }
    QJsonObject data;
    data["content"] = ui->messageEdit->text();
    QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::Chat, data)).toJson();
    qDebug() << "Sending chat message:" << message;
    socket->write(message);
    ui->messageEdit->clear();
}

void ChatWindow::handleServerData() {
    QByteArray data = socket->readAll();
    qDebug() << "Received data from server:" << data;
    MessageType type;
    QJsonObject msgData;
    if (MessageProtocol::parseMessage(data, type, msgData)) {
        if (type == MessageType::Register) {
            if (msgData["status"].toString() == "success") {
                QMessageBox::information(this, "Register", "Registration successful!");
                ui->authStackedWidget->setCurrentWidget(ui->loginPage);
                ui->authButton->setText("Sign In");
                isLoginMode = true;
                ui->showLoginButton->setChecked(true);
                ui->showRegisterButton->setChecked(false);
            } else {
                QMessageBox::warning(this, "Register", "Registration failed: " + msgData["reason"].toString());
            }
        } else if (type == MessageType::Login) {
            if (msgData["status"].toString() == "success") {
                isLoggedIn = true;
                ui->sendButton->setEnabled(true);
                ui->chatDisplay->append("Signed in as: " + ui->loginNicknameEdit->text());
                ui->stackedWidget->setCurrentWidget(ui->chatPage);
            } else {
                isLoggedIn = false;
                ui->sendButton->setEnabled(false);
                QMessageBox::warning(this, "Sign In", "Sign in failed: " + msgData["reason"].toString());
            }
        } else if (type == MessageType::Chat) {
            ui->chatDisplay->append(msgData["content"].toString());
        } else if (type == MessageType::Error) {
            QMessageBox::warning(this, "Error", msgData["reason"].toString());
        }
    } else {
        qDebug() << "Failed to parse server message:" << data;
    }
}
