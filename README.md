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

