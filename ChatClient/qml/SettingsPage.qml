import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Rectangle {
    color: "#FFFFFF"

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
                    color: "#0069d9"
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
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#E9ECEF"
        }

        // 设置项
        Text {
            text: "通知设置"
            font.pixelSize: 16
            font.bold: true
            color: "#495057"
        }

        Switch {
            text: "接收新消息通知"
            checked: true
        }

        Switch {
            text: "声音提醒"
            checked: true
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#E9ECEF"
            Layout.topMargin: 10
        }

        Text {
            text: "主题设置"
            font.pixelSize: 16
            font.bold: true
            color: "#495057"
        }

        ComboBox {
            model: ["浅色主题", "深色主题"]
            currentIndex: 0
            Layout.fillWidth: true
        }

        Item {
            Layout.fillHeight: true
        }
    }
} 