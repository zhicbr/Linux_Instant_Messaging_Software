import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.settings 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 950
    height: 650
    title: "Modern Chat"

    // 添加应用程序设置对象，用于存储主题设置
    Settings {
        id: appSettings
        property bool darkTheme: false
    }

    // 全局主题颜色
    QtObject {
        id: theme
        // 背景色
        property color backgroundColor: appSettings.darkTheme ? "#1e1e2e" : "#F8F9FA"
        property color sidebarColor: appSettings.darkTheme ? "#262636" : "#E9ECEF"
        property color borderColor: appSettings.darkTheme ? "#373744" : "#DEE2E6"
        
        // 文本颜色
        property color primaryTextColor: appSettings.darkTheme ? "#CDD6F4" : "#343A40"
        property color secondaryTextColor: appSettings.darkTheme ? "#A6ADC8" : "#6C757D"
        
        // 按钮颜色
        property color primaryButtonColor: appSettings.darkTheme ? "#6272A4" : "#0069d9"
        property color primaryButtonPressedColor: appSettings.darkTheme ? "#536188" : "#0062cc"
        property color secondaryButtonColor: appSettings.darkTheme ? "#45475A" : "#6C757D"
        property color secondaryButtonPressedColor: appSettings.darkTheme ? "#383845" : "#5A6268"
        property color dangerButtonColor: appSettings.darkTheme ? "#F38BA8" : "#dc3545"
        property color dangerButtonPressedColor: appSettings.darkTheme ? "#D37089" : "#c82333"
        property color successButtonColor: appSettings.darkTheme ? "#A6E3A1" : "#17a2b8"
        property color successButtonPressedColor: appSettings.darkTheme ? "#8BC078" : "#138496"
        
        // 输入框颜色
        property color inputBackgroundColor: appSettings.darkTheme ? "#313244" : "#FFFFFF"
        property color inputBorderColor: appSettings.darkTheme ? "#586185" : "#CED4DA"
        property color inputFocusBorderColor: appSettings.darkTheme ? "#A6ADC8" : "#80BDFF"
        
        // 消息气泡
        property color selfMessageBubbleColor: appSettings.darkTheme ? "#7F7FD5" : "#007BFF"
        property color otherMessageBubbleColor: appSettings.darkTheme ? "#313244" : "#E9ECEF"
        property color selfMessageTextColor: appSettings.darkTheme ? "#FFFFFF" : "#FFFFFF"
        property color otherMessageTextColor: appSettings.darkTheme ? "#CDD6F4" : "#212529"
        
        // 状态栏
        property color statusBarColor: appSettings.darkTheme ? "#1A1B26" : "#E9ECEF"
        property color statusBarTextColor: appSettings.darkTheme ? "#A6ADC8" : "#6C757D"
    }

    Rectangle {
        anchors.fill: parent
        color: theme.backgroundColor
    }

    StackView {
        id: stackView
        anchors.fill: parent
        initialItem: chatWindow.isLoggedIn ? chatPage : authPage
    }

    Component {
        id: authPage
        Rectangle {
            color: "transparent"
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    text: "欢迎使用 Modern Chat"
                    font.pixelSize: 28
                    font.bold: true
                    color: "#343A40"
                    Layout.alignment: Qt.AlignHCenter
                }

                TabBar {
                    id: authTabBar
                    Layout.alignment: Qt.AlignHCenter
                    background: Rectangle { color: "transparent" }

                    TabButton {
                        text: "登录"
                        width: implicitWidth
                        contentItem: Text {
                            text: parent.text
                            color: parent.checked ? "#0069d9" : "#6C757D"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 15
                            font.bold: parent.checked
                        }
                        background: Rectangle {
                            color: parent.checked ? "#E7F1FF" : "transparent"
                            Rectangle {
                                visible: parent.parent.checked
                                color: "#0069d9"
                                height: 2
                                width: parent.width
                                anchors.bottom: parent.bottom
                            }
                        }
                    }
                    TabButton {
                        text: "注册"
                        width: implicitWidth
                        contentItem: Text {
                            text: parent.text
                            color: parent.checked ? "#0069d9" : "#6C757D"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 15
                            font.bold: parent.checked
                        }
                        background: Rectangle {
                            color: parent.checked ? "#E7F1FF" : "transparent"
                            Rectangle {
                                visible: parent.parent.checked
                                color: "#0069d9"
                                height: 2
                                width: parent.width
                                anchors.bottom: parent.bottom
                            }
                        }
                    }
                }

                StackLayout {
                    currentIndex: authTabBar.currentIndex
                    Layout.preferredWidth: 420
                    Layout.preferredHeight: 300

                    LoginPage {}
                    RegisterPage {}
                }
            }
        }
    }

    Component {
        id: chatPage
        ChatPage {}
    }

    Component {
        id: settingsPage
        SettingsPage {}
    }

    Component {
        id: groupChatPage
        GroupChatPage {}
    }

    Component {
        id: profilePage
        ProfilePage {}
    }

    Connections {
        target: chatWindow
        function onIsLoggedInChanged() {
            if (chatWindow.isLoggedIn) {
                stackView.clear();
                stackView.push(chatPage);
            } else {
                stackView.clear();
                stackView.push(authPage);
            }
        }
        function onStatusMessage(message) {
            statusBar.text = message;
            statusTimer.restart();
        }
    }

    footer: Rectangle {
        height: 30
        color: theme.statusBarColor
        Text {
            id: statusBar
            anchors.centerIn: parent
            color: theme.statusBarTextColor
        }
        Timer {
            id: statusTimer
            interval: 3000
            onTriggered: statusBar.text = ""
        }
    }

    // 为全局提供切换主题的函数
    function toggleTheme() {
        appSettings.darkTheme = !appSettings.darkTheme
    }
}