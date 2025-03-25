#include "messageprotocol.h"
#include <QJsonDocument>

QJsonObject MessageProtocol::createMessage(MessageType type, const QJsonObject &data) {
    QJsonObject msg;
    msg["type"] = static_cast<int>(type);
    msg["data"] = data;
    return msg;
}

bool MessageProtocol::parseMessage(const QByteArray &data, MessageType &type, QJsonObject &outData) {
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false;
    QJsonObject obj = doc.object();
    type = static_cast<MessageType>(obj["type"].toInt());
    outData = obj["data"].toObject();
    return true;
}
