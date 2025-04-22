

## 当前

使用Code3里的project1-1





## 构建编译运行



服务端
构建编译

```
build_dir=~/Code3/project1-1/ChatServer/build
mkdir -p "$build_dir" && \
(cd "$build_dir" && rm -rf ./* && cmake .. -DCMAKE_PREFIX_PATH=/home/chenbaorui/Qt/6.8.2/gcc_64 && make)
```



运行

```
cd ~/Code3/project1-1/ChatServer/build
./ChatServer
```

客户端
构建编译

```
build_dir=~/Code3/project1-1/ChatClient/build
mkdir -p "$build_dir" && \
(cd "$build_dir" && rm -rf ./* && cmake .. -DCMAKE_PREFIX_PATH=/home/chenbaorui/Qt/6.8.2/gcc_64 && make)
```

运行

```
cd ~/Code3/project1-1/ChatClient/build
./ChatClient
```



## 添加功能



添加一个新功能（以登出功能为例）需要修改的地方：

1. **消息协议层**（Common/messageprotocol.h）：

```cpp
enum class MessageType {
    // ... 现有消息类型 ...
    Logout,    // 添加新的消息类型
    // ... 其他消息类型 ...
};
```

2. **服务器端**（ChatServer/src/）：

   - **server.h**：

   ```cpp
   class Server : public QObject {
   private:
       // 添加新的处理函数声明
       void handleLogout(ClientInfo *clientInfo);
       // 可能需要的辅助函数声明
       void updateUserStatus(const QString &nickname, bool isOnline);
       void notifyFriendsStatusChange(const QString &nickname, bool isOnline);
   };
   ```

   - **server.cpp**：

   ```cpp
   void Server::handleClientData() {
       // 在消息处理switch中添加新的case
       case MessageType::Logout:
           if (!clientInfo->isLoggedIn) {
               // 错误处理
               return;
           }
           handleLogout(clientInfo);
           break;
   }
   
   // 实现新的处理函数
   void Server::handleLogout(ClientInfo *clientInfo) {
       // 实现具体的登出逻辑
   }
   ```

3. **客户端**（ChatClient/src/）：

   - **chatwindow.h**：

   ```cpp
   class ChatWindow : public QObject {
   public:
       // 添加新的Q_INVOKABLE方法供QML调用
       Q_INVOKABLE void logout();
       
   private:
       // 可能需要的私有方法或属性
       void setIsLoggedIn(bool loggedIn);
   };
   ```

   - **chatwindow.cpp**：

   ```cpp
   void ChatWindow::logout() {
       // 实现登出逻辑
       // 发送登出消息给服务器
       // 清理本地状态
   }
   
   void ChatWindow::handleServerData() {
       // 在消息处理switch中添加新的case
       case MessageType::Logout:
           // 处理服务器的登出响应
           break;
   }
   ```

4. **客户端界面**（ChatClient/qml/）：

   - **相关页面.qml**（如ChatPage.qml）：

   ```qml
   Button {
       text: "登出"
       onClicked: chatWindow.logout()
       // 按钮样式等
   }
   ```

   - **main.qml**（如果涉及页面切换）：

   ```qml
   // 可能需要添加页面切换逻辑
   StackView {
       // 处理页面切换
   }
   ```

5. **数据库**（如果需要）：

```sql
-- 如果需要修改数据库结构
ALTER TABLE users ADD COLUMN is_online INTEGER DEFAULT 0;
```

总结添加新功能的步骤：

1. **协议层修改**：
   - 在 `messageprotocol.h` 添加新的消息类型
   - 定义消息格式和参数

2. **服务器端修改**：
   - 添加新功能的处理函数声明（server.h）
   - 实现处理函数（server.cpp）
   - 在消息处理逻辑中添加新消息类型的处理
   - 必要时修改数据库结构或添加新的数据库操作

3. **客户端修改**：
   - 添加新功能的方法声明（chatwindow.h）
   - 实现新功能方法（chatwindow.cpp）
   - 添加服务器响应处理逻辑
   - 更新客户端状态管理

4. **界面修改**：
   - 添加新的UI元素（按钮、输入框等）
   - 实现UI交互逻辑
   - 必要时添加新的页面或组件
   - 更新页面导航逻辑

5. **测试流程**：
   - 编译并测试服务器端修改
   - 编译并测试客户端修改
   - 测试完整的功能流程
   - 处理错误情况和边界条件

记住：

- 保持代码结构清晰，遵循现有的代码风格
- 确保错误处理完善
- 注意状态同步（客户端状态和服务器状态）
- 考虑并发和多用户场景
- 添加必要的日志输出便于调试

这样的模式可以应用于添加其他功能，如添加好友、发送消息等，只需要根据具体功能调整相应的实现细节。



## 添加页面

resources.qrc 添加        <file>qml/SettingsPage.qml</file>

```
<RCC>
    <qresource prefix="/">
        <file>qml/main.qml</file>
        <file>qml/LoginPage.qml</file>
        <file>qml/RegisterPage.qml</file>
        <file>qml/ChatPage.qml</file>
        <file>qml/MessageBubble.qml</file>
        <file>qml/FriendListItem.qml</file>
        <file>qml/SettingsPage.qml</file>
    </qresource>
</RCC>
```

## 注意
不要用c++的关键字作为参数名


## 数据库

由于创建表时有"如果这张表不存在才创建"的逻辑，所以，如果项目里有user.db这个文件，表就一直存在，哪怕后来更新了表结构，也无法改变。
所以，改动表结构，或者预设置数据，都要先清除db文件。



## Bug解决记录

### 1. 离线好友请求不显示问题

**问题描述**：
当用户A向离线用户B发送好友请求，然后用户B登录后，无法看到来自用户A的好友请求。

**原因分析**：
1. **消息合并问题**：客户端登录成功后会连续发送两个请求：获取好友列表(GetFriendList)和获取好友请求列表(GetFriendRequests)。由于这两个请求被连续发送且没有任何延迟，它们在网络传输过程中被合并成了一个完整消息包。

2. **消息解析失败**：服务器在接收到这个合并的消息包后，只能成功解析第一个请求(GetFriendList)，而无法解析第二个请求(GetFriendRequests)，导致服务器报错"Failed to parse message"，从而无法返回好友请求列表。

3. **日志显示**：在服务器日志中可以清楚地看到该问题：
   ```
   Received data: {"data": {}, "type": 6}\n{"data": {}, "type": 14}
   Failed to parse message: {"data": {}, "type": 6}\n{"data": {}, "type": 14}
   ```
   其中type=6是GetFriendList请求，type=14是GetFriendRequests请求。

**解决方案**：
1. 使用`QTimer::singleShot`添加短暂延迟，确保两个请求是分开发送的：
   ```cpp
   // 先发送获取好友列表请求
   m_socket->write(QJsonDocument(MessageProtocol::createMessage(
       MessageType::GetFriendList, {})).toJson());
   m_socket->flush();
   
   // 延迟100毫秒后发送获取好友请求列表请求
   QTimer::singleShot(100, this, [this]() {
       if(m_socket && m_socket->state() == QAbstractSocket::ConnectedState && m_isLoggedIn) {
           m_socket->write(QJsonDocument(MessageProtocol::createMessage(
               MessageType::GetFriendRequests, {})).toJson());
           m_socket->flush();
       }
   });
   ```

2. 确保`socket->flush()`被调用，促使消息立即发送，避免缓冲区积累。

**教训**：
1. 在发送连续请求时，考虑TCP协议的特性，多个紧挨着的小数据包可能会合并成一个大数据包传输，需要适当添加延迟。
2. 服务器端消息解析逻辑需要更加健壮，能够处理合并的消息或提供明确的错误提示。
3. 完善日志系统对于此类问题的排查至关重要，从日志中能直观地看出消息被合并的情况。

### 2. 群聊消息不显示问题

**问题描述**：
在群聊页面中，无论是发送新消息还是加载历史消息，群聊消息都不会显示在聊天框中。

**原因分析**：
1. **信号连接不匹配**：在`GroupChatPage.qml`中，`onGroupChatMessageReceived`函数包含了条件判断`if (chatWindow.isGroupChat)`，但在某些情况下`isGroupChat`属性可能未正确设置。

2. **状态切换不完整**：在`selectFriend`函数中没有正确重置`m_isGroupChat`属性，导致从群聊切换到一对一聊天后，再次进入群聊时状态混乱。

3. **消息类型混用**：群聊历史记录处理中使用了普通的`appendMessage`函数而不是专用的群聊消息信号，导致消息无法正确路由到群聊界面。

**解决方案**：
1. 修改`GroupChatPage.qml`中的信号处理函数，移除不必要的条件判断：
   ```qml
   function onGroupChatMessageReceived(sender, content, timestamp) {
       // 移除条件判断，确保消息总是显示
       console.log("收到群聊消息信号，发送者:", sender, "，当前是否为群聊:", chatWindow.isGroupChat);
       messageModel.append({
           "sender": sender,
           "content": content,
           "timestamp": timestamp
       });
       console.log("添加群聊消息到列表");
   }
   ```

2. 完善`selectFriend`函数，确保在切换到一对一聊天时正确重置群聊状态：
   ```cpp
   void ChatWindow::selectFriend(const QString &friendName)
   {
       if (friendName != m_currentChatFriend || m_isGroupChat) {
           // 清除群聊状态
           m_currentChatGroup.clear();
           m_isGroupChat = false;
           emit isGroupChatChanged();
           emit currentChatGroupChanged();
           
           // 设置一对一聊天状态
           m_currentChatFriend = friendName;
           emit currentChatFriendChanged();
           clearChatDisplay();
           loadChatHistory(friendName);
       }
   }
   ```

3. 修改群聊历史记录处理，使用专用的群聊消息信号：
   ```cpp
   case MessageType::GroupChatHistory:
       if (msgData["status"].toString() == "success") {
           clearChatDisplay();
           QJsonArray messages = msgData["messages"].toArray();
           if (messages.isEmpty()) {
               // 使用群聊消息信号显示空消息提示
               emit groupChatMessageReceived("系统", "尚无群聊消息记录。", "");
           } else {
               for (const QJsonValue &msgValue : messages) {
                   QJsonObject msgObj = msgValue.toObject();
                   QString sender = msgObj["from"].toString();
                   QString content = msgObj["content"].toString();
                   QString timestamp = msgObj.contains("timestamp") ? 
                       msgObj["timestamp"].toString() : QDateTime::currentDateTime().toString("hh:mm");
                   // 使用群聊消息信号而不是普通消息信号
                   emit groupChatMessageReceived(sender, content, timestamp);
               }
           }
       }
   ```

**教训**：
1. 在实现多种聊天模式（如一对一聊天和群聊）时，需要确保状态切换的完整性和一致性。
2. 针对不同类型的消息，应使用专门的信号和处理逻辑，避免混用导致路由错误。
3. 添加适当的调试日志对于排查UI显示问题非常重要，特别是在信号-槽机制的调试中。
4. 在QML和C++交互中，需要确保属性变化正确触发相应的信号以更新UI状态。





