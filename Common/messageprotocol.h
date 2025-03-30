#ifndef MESSAGEPROTOCOL_H
#define MESSAGEPROTOCOL_H

#include <QJsonObject>

enum class MessageType {
    Login,
    Chat,
    Register,
    Error,
    SearchUser,
    AddFriend,
    GetFriendList,
    GetChatHistory
};

class MessageProtocol {
public:
    static QJsonObject createMessage(MessageType type, const QJsonObject &data);
    static bool parseMessage(const QByteArray &data, MessageType &type, QJsonObject &msgData);
};

#endif
