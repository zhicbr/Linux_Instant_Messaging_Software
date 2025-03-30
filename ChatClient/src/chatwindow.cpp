#include "chatwindow.h"
#include "ui_chatwindow.h"
#include <QJsonDocument>
#include <QJsonArray>
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

    ui->stackedWidget->setCurrentWidget(ui->authPage);
    ui->authStackedWidget->setCurrentWidget(ui->loginPage);
    ui->authButton->setText("Sign In");
    isLoginMode = true;
    ui->showLoginButton->setChecked(true);
    ui->showRegisterButton->setChecked(false);

    ui->sendButton->setEnabled(false);

    authShadow = new QGraphicsDropShadowEffect(this);
    authShadow->setBlurRadius(10);
    authShadow->setXOffset(0);
    authShadow->setYOffset(2);
    authShadow->setColor(QColor(0, 0, 0, 60));
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
    if (currentChatFriend.isEmpty()) {
        QMessageBox::warning(this, "Send Message", "Please select a friend to chat with!");
        return;
    }
    QJsonObject data;
    data["to"] = currentChatFriend;
    data["content"] = ui->messageEdit->text();
    QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::Chat, data)).toJson();
    qDebug() << "Sending private message:" << message;
    socket->write(message);
    ui->chatDisplay->append("You: " + ui->messageEdit->text());
    ui->messageEdit->clear();
}

void ChatWindow::on_searchButton_clicked() {
    QString searchText = ui->searchEdit->text();
    if (searchText.isEmpty()) {
        QMessageBox::warning(this, "Search", "Please enter a nickname or email to search!");
        return;
    }
    QJsonObject data;
    data["query"] = searchText;
    QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::SearchUser, data)).toJson();
    qDebug() << "Sending search request:" << message;
    socket->write(message);
}

void ChatWindow::on_addFriendButton_clicked() {
    QString friendName = ui->searchEdit->text();
    if (friendName.isEmpty()) {
        QMessageBox::warning(this, "Add Friend", "Please search for a user first!");
        return;
    }
    QJsonObject data;
    data["friend"] = friendName;
    QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::AddFriend, data)).toJson();
    qDebug() << "Sending add friend request:" << message;
    socket->write(message);
}

void ChatWindow::on_friendList_itemClicked(QListWidgetItem *item) {
    currentChatFriend = item->text();
    ui->chatDisplay->clear();
    loadChatHistory(currentChatFriend);
}

void ChatWindow::updateFriendList(const QStringList &friends) {
    ui->friendList->clear();
    for (const QString &friendName : friends) {
        ui->friendList->addItem(friendName);
    }
}

void ChatWindow::loadChatHistory(const QString &friendName) {
    QJsonObject data;
    data["friend"] = friendName;
    QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::GetChatHistory, data)).toJson();
    qDebug() << "Requesting chat history with:" << friendName;
    socket->write(message);
}

void ChatWindow::handleServerData() {
    QByteArray data = socket->readAll();
    qDebug() << "Received data from server:" << data;
    MessageType type;
    QJsonObject msgData;
    if (MessageProtocol::parseMessage(data, type, msgData)) {
        switch (type) {
        case MessageType::Register:
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
            break;
        case MessageType::Login:
            if (msgData["status"].toString() == "success") {
                isLoggedIn = true;
                currentNickname = ui->loginNicknameEdit->text();
                ui->sendButton->setEnabled(true);
                ui->chatDisplay->append("Signed in as: " + currentNickname);
                ui->stackedWidget->setCurrentWidget(ui->chatPage);
                QJsonObject data;
                QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::GetFriendList, data)).toJson();
                socket->write(message);
            } else {
                isLoggedIn = false;
                ui->sendButton->setEnabled(false);
                QMessageBox::warning(this, "Sign In", "Sign in failed: " + msgData["reason"].toString());
            }
            break;
        case MessageType::Chat:
            if (msgData["from"].toString() == currentChatFriend) {
                ui->chatDisplay->append(msgData["from"].toString() + ": " + msgData["content"].toString());
            }
            break;
        case MessageType::SearchUser:
            if (msgData["status"].toString() == "success") {
                ui->searchEdit->setText(msgData["nickname"].toString());
                QMessageBox::information(this, "Search", "User found: " + msgData["nickname"].toString());
            } else {
                QMessageBox::warning(this, "Search", "User not found!");
            }
            break;
        case MessageType::AddFriend:
            if (msgData["status"].toString() == "success") {
                QMessageBox::information(this, "Add Friend", "Friend added successfully!");
                QJsonObject data;
                QByteArray message = QJsonDocument(MessageProtocol::createMessage(MessageType::GetFriendList, data)).toJson();
                socket->write(message);
            } else {
                QMessageBox::warning(this, "Add Friend", "Failed to add friend: " + msgData["reason"].toString());
            }
            break;
        case MessageType::GetFriendList:
            if (msgData["status"].toString() == "success") {
                QJsonArray friendArray = msgData["friends"].toArray();
                QStringList friends;
                for (const QJsonValue &value : friendArray) {
                    friends.append(value.toString());
                }
                updateFriendList(friends);
            }
            break;
        case MessageType::GetChatHistory:
            if (msgData["status"].toString() == "success") {
                for (const QJsonValue &msg : msgData["messages"].toArray()) {
                    QJsonObject obj = msg.toObject();
                    QString sender = obj["from"].toString();
                    QString content = obj["content"].toString();
                    ui->chatDisplay->append(sender + ": " + content);
                }
            }
            break;
        case MessageType::Error:
            QMessageBox::warning(this, "Error", msgData["reason"].toString());
            break;
        }
    } else {
        qDebug() << "Failed to parse server message:" << data;
    }
}