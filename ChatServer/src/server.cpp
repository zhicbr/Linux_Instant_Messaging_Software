#include "server.h"
#include <QJsonDocument>
#include <QDebug>
#include <QCoreApplication> // 添加 QCoreApplication 头文件
#include "../Common/config.h"

Server::Server(QObject *parent) : QObject(parent) {
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &Server::handleNewConnection);
}

void Server::start() {
    if (tcpServer->listen(QHostAddress::Any, Config::DefaultPort)) {
        qDebug() << "Server started, listening on port" << Config::DefaultPort;
    } else {
        qDebug() << "Failed to start server:" << tcpServer->errorString();
        QCoreApplication::quit(); // 如果监听失败，退出程序
    }
}

void Server::handleNewConnection() {
    QTcpSocket *client = tcpServer->nextPendingConnection();
    qDebug() << "New client connected from" << client->peerAddress().toString() << ":" << client->peerPort();
    clients.append(client);
    connect(client, &QTcpSocket::readyRead, this, &Server::handleClientData);
}

void Server::handleClientData() {
    QTcpSocket *client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;
    QByteArray data = client->readAll();
    qDebug() << "Received data from" << client->peerAddress().toString() << ":" << client->peerPort() << ":" << data;
    MessageType type;
    QJsonObject msgData;
    if (MessageProtocol::parseMessage(data, type, msgData)) {
        if (type == MessageType::Login) {
            qDebug() << "Client logged in:" << msgData;
            client->write(QJsonDocument(MessageProtocol::createMessage(MessageType::Login, {{"status", "success"}})).toJson());
        } else if (type == MessageType::Chat) {
            qDebug() << "Broadcasting chat message:" << msgData;
            for (QTcpSocket *c : clients) {
                c->write(data); // 广播消息
            }
        }
    }
}
