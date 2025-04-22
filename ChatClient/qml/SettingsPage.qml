import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    color: theme.backgroundColor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Button {
                text: "返回"
                font.pixelSize: 14
                background: Rectangle {
                    color: theme.primaryButtonColor
                    radius: 6
                    border.width: 0
                }
                contentItem: Text {
                    text: parent.text
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font: parent.font
                }
                onClicked: stackView.pop()
            }

            Text {
                text: "设置"
                font.pixelSize: 20
                font.bold: true
                color: theme.primaryTextColor
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: theme.borderColor
        }

        // 设置项
        Text {
            text: "通知设置"
            font.pixelSize: 16
            font.bold: true
            color: theme.primaryTextColor
        }

        Switch {
            text: "接收新消息通知"
            checked: true
            contentItem: Text {
                text: parent.text
                font.pixelSize: 14
                color: theme.primaryTextColor
                verticalAlignment: Text.AlignVCenter
                leftPadding: parent.indicator.width + parent.spacing
            }
        }

        Switch {
            text: "声音提醒"
            checked: true
            contentItem: Text {
                text: parent.text
                font.pixelSize: 14
                color: theme.primaryTextColor
                verticalAlignment: Text.AlignVCenter
                leftPadding: parent.indicator.width + parent.spacing
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: theme.borderColor
            Layout.topMargin: 10
        }

        Text {
            text: "主题设置"
            font.pixelSize: 16
            font.bold: true
            color: theme.primaryTextColor
        }

        // 使用Switch替换ComboBox实现主题切换
        Switch {
            id: themeSwitch
            text: "深色主题"
            checked: appSettings.darkTheme
            contentItem: Text {
                text: parent.text
                font.pixelSize: 14
                color: theme.primaryTextColor
                verticalAlignment: Text.AlignVCenter
                leftPadding: parent.indicator.width + parent.spacing
            }
            onToggled: {
                root.toggleTheme() // 调用在main.qml中定义的函数
                // 下面的提示消息可以根据实际需求保留或删除
                statusBar.text = appSettings.darkTheme ? "已切换到深色主题" : "已切换到浅色主题"
                statusTimer.restart()
            }
        }

        Text {
            text: appSettings.darkTheme ? "当前使用深色主题" : "当前使用浅色主题"
            font.pixelSize: 12
            color: theme.secondaryTextColor
            Layout.leftMargin: 10
        }

        Item {
            Layout.fillHeight: true
        }
    }
} 