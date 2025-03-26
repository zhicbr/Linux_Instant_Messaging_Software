#ifndef MESSAGEPROTOCOL_H
#define MESSAGEPROTOCOL_H

#include <QJsonObject>

enum class MessageType {
    Login,
    Chat,
    Register,// 新增注册消息类型
    Error
};

class MessageProtocol {
public:
    static QJsonObject createMessage(MessageType type, const QJsonObject &data);
    static bool parseMessage(const QByteArray &data, MessageType &type, QJsonObject &msgData);
};

#endif
