import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: root
    visible: true
    width: 950
    height: 650
    title: "Modern Chat"

    Rectangle {
        anchors.fill: parent
        color: "#F8F9FA"
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
                    }
                    TabButton {
                        text: "注册"
                        width: implicitWidth
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

    Connections {
        target: chatWindow
        function onIsLoggedInChanged() {
            if (chatWindow.isLoggedIn) {
                stackView.replace(chatPage);
            } else {
                stackView.replace(authPage);
            }
        }
        function onStatusMessage(message) {
            statusBar.text = message;
            statusTimer.restart();
        }
    }

    footer: Rectangle {
        height: 30
        color: "#E9ECEF"
        Text {
            id: statusBar
            anchors.centerIn: parent
            color: "#6C757D"
        }
        Timer {
            id: statusTimer
            interval: 3000
            onTriggered: statusBar.text = ""
        }
    }
}