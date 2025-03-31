#include "chatwindow.h"
#include "ui_chatwindow.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QMessageBox>
#include <QDebug>
#include <QScrollBar>
#include "../Common/config.h"

ChatWindow::ChatWindow(QWidget *parent) : 
    QMainWindow(parent),
    ui(new Ui::ChatWindow),
    socket(new QTcpSocket(this)),
    isLoggedIn(false),
    isLoginMode(true),
    authShadow(new QGraphicsDropShadowEffect(this)),
    chatShadow(new QGraphicsDropShadowEffect(this)),
    inputShadow(new QGraphicsDropShadowEffect(this))
{
    ui->setupUi(this);

    // 初始化网络连接
    socket->connectToHost(Config::DefaultServerIp, Config::DefaultPort);
    connect(socket, &QTcpSocket::readyRead, this, &ChatWindow::handleServerData);

    // 初始化UI状态
    ui->stackedWidget->setCurrentWidget(ui->authPage);
    ui->authStackedWidget->setCurrentWidget(ui->loginPage);
    ui->sendButton->setEnabled(false);

    // 设置阴影效果
    authShadow->setBlurRadius(10);
    ui->authContainer->setGraphicsEffect(authShadow);
    
    chatShadow->setBlurRadius(10);
    ui->chatDisplay->setGraphicsEffect(chatShadow);
    
    inputShadow->setBlurRadius(5);
    ui->messageEdit->setGraphicsEffect(inputShadow);

    // 配置聊天框样式
    ui->chatDisplay->document()->setDefaultStyleSheet(
        "body { font-family: 'Segoe UI', Arial; font-size: 14px; margin: 0; padding: 0; }"
        ".message { margin: 8px 0; padding: 10px 15px; border-radius: 18px; max-width: 70%; "
        "word-wrap: break-word; line-height: 1.4; }"
        ".self { background: #0084ff; color: white; margin-left: 30%; text-align: right; "
        "border-bottom-right-radius: 5px; }"
        ".friend { background: #e5e5ea; color: black; margin-right: 30%; text-align: left; "
        "border-bottom-left-radius: 5px; }"
        ".timestamp { color: #999; font-size: 11px; margin: 5px 10px; }"
    );
}

ChatWindow::~ChatWindow()
{
    delete ui;
}

void ChatWindow::appendMessage(const QString &sender, const QString &content)
{
    bool isSelf = (sender == currentNickname);
    QString className = isSelf ? "self" : "friend";
    QString displayName = isSelf ? "You" : sender;
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm");

    QString html = QString(
        "<div class='timestamp' style='text-align:%1'>%2</div>"
        "<div class='message %3'>"
        "<b>%4:</b><br>%5"
        "</div>"
    ).arg(isSelf ? "right" : "left")
     .arg(timestamp)
     .arg(className)
     .arg(displayName)
     .arg(content.toHtmlEscaped().replace("\n", "<br>"));

    ui->chatDisplay->append(html);
    
    // 自动滚动到底部
    QScrollBar *scrollbar = ui->chatDisplay->verticalScrollBar();
    scrollbar->setValue(scrollbar->maximum());
}

void ChatWindow::on_sendButton_clicked()
{
    if (!isLoggedIn || currentChatFriend.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please login and select a friend first!");
        return;
    }

    QString message = ui->messageEdit->text().trimmed();
    if (message.isEmpty()) return;

    // 发送消息
    QJsonObject data{
        {"to", currentChatFriend},
        {"content", message}
    };
    socket->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Chat, data)).toJson());

    // 本地显示
    appendMessage(currentNickname, message);
    ui->messageEdit->clear();
}

void ChatWindow::handleServerData()
{
    QByteArray data = socket->readAll();
    MessageType type;
    QJsonObject msgData;

    if (!MessageProtocol::parseMessage(data, type, msgData)) {
        qWarning() << "Failed to parse message:" << data;
        return;
    }

    switch (type) {
    case MessageType::Login:
        if (msgData["status"].toString() == "success") {
            isLoggedIn = true;
            currentNickname = ui->loginNicknameEdit->text();
            ui->sendButton->setEnabled(true);
            ui->stackedWidget->setCurrentWidget(ui->chatPage);
            ui->chatDisplay->append("<center>Logged in successfully</center>");
            
            // 请求好友列表
            socket->write(QJsonDocument(MessageProtocol::createMessage(
                MessageType::GetFriendList, {})).toJson());
        } else {
            QMessageBox::warning(this, "Login Failed", msgData["reason"].toString());
        }
        break;

    case MessageType::Chat:
        if (msgData["from"].toString() == currentChatFriend) {
            appendMessage(msgData["from"].toString(), msgData["content"].toString());
        }
        break;

    case MessageType::GetFriendList:
        if (msgData["status"].toString() == "success") {
            ui->friendList->clear();
            for (const QJsonValue &value : msgData["friends"].toArray()) {
                ui->friendList->addItem(value.toString());
            }
        }
        break;

    case MessageType::GetChatHistory:
        if (msgData["status"].toString() == "success") {
            ui->chatDisplay->clear();
            for (const QJsonValue &msg : msgData["messages"].toArray()) {
                QJsonObject obj = msg.toObject();
                appendMessage(obj["from"].toString(), obj["content"].toString());
            }
        }
        break;

    case MessageType::Error:
        QMessageBox::warning(this, "Error", msgData["reason"].toString());
        break;

    default:
        qDebug() << "Unhandled message type:" << static_cast<int>(type);
        break;
    }
}

// 其他UI槽函数保持不变
void ChatWindow::on_showLoginButton_clicked() {
    ui->authStackedWidget->setCurrentWidget(ui->loginPage);
    ui->authButton->setText("Sign In");
    isLoginMode = true;
}

void ChatWindow::on_showRegisterButton_clicked() {
    ui->authStackedWidget->setCurrentWidget(ui->registerPage);
    ui->authButton->setText("Sign Up");
    isLoginMode = false;
}

void ChatWindow::on_authButton_clicked() {
    QJsonObject data;
    if (isLoginMode) {
        data["nickname"] = ui->loginNicknameEdit->text();
        data["password"] = ui->loginPasswordEdit->text();
    } else {
        data["email"] = ui->emailEdit->text();
        data["nickname"] = ui->nicknameEdit->text();
        data["password"] = ui->passwordEdit->text();
    }

    MessageType msgType = isLoginMode ? MessageType::Login : MessageType::Register;
    socket->write(QJsonDocument(MessageProtocol::createMessage(msgType, data)).toJson());
}

void ChatWindow::on_friendList_itemClicked(QListWidgetItem *item)
{
    currentChatFriend = item->text();
    ui->chatDisplay->clear();
    
    QJsonObject data{{"friend", currentChatFriend}};
    socket->write(QJsonDocument(MessageProtocol::createMessage(
        MessageType::GetChatHistory, data)).toJson());
}

void ChatWindow::on_searchButton_clicked()
{
    QString query = ui->searchEdit->text().trimmed();
    if (query.isEmpty()) return;
    
    socket->write(QJsonDocument(MessageProtocol::createMessage(
        MessageType::SearchUser, {{"query", query}})).toJson());
}

void ChatWindow::on_addFriendButton_clicked()
{
    if (ui->searchEdit->text().isEmpty()) return;
    
    socket->write(QJsonDocument(MessageProtocol::createMessage(
        MessageType::AddFriend, {{"friend", ui->searchEdit->text()}})).toJson());
}

void ChatWindow::updateFriendList(const QStringList &friends)
{
    ui->friendList->clear();
    for (const QString &friendName : friends) {
        ui->friendList->addItem(friendName);
    }
}

void ChatWindow::loadChatHistory(const QString &friendName)
{
    QJsonObject data{{"friend", friendName}};
    socket->write(QJsonDocument(MessageProtocol::createMessage(
        MessageType::GetChatHistory, data)).toJson());
}