#ifndef SERVER_H
#define SERVER_H

#include <QTcpServer>
#include <QTcpSocket>
#include "../Common/messageprotocol.h"

class Server : public QObject {
    Q_OBJECT
public:
    explicit Server(QObject *parent = nullptr);
    void start();

private slots:
    void handleNewConnection();
    void handleClientData();

private:
    QTcpServer *tcpServer;
    QList<QTcpSocket*> clients;
};

#endif
