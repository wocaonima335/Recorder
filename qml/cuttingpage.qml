import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: cuttingpage
    property bool showPreview: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Text {
            text: "VIDEO EDITOR"
            font.pixelSize: 24
            font.bold: true
            color: "#FFFFFF"
            font.family: "Segoe UI"
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#3A4050"
        }

        // 视频列表占位符
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#2B2E36"
            radius: 6
            border.color: "#3A4050"
            border.width: 1

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 15
                
                Image {
                    source: "icons/video-gray.png" // 假设有通用图标
                    Layout.alignment: Qt.AlignHCenter
                    opacity: 0.3
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: 64
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    text: "No videos selected"
                    color: "#606570"
                    font.pixelSize: 16
                    Layout.alignment: Qt.AlignHCenter
                }
                
                Button {
                    text: "Import Video"
                    Layout.alignment: Qt.AlignHCenter
                    // 简单的按钮样式示例
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.down ? "#3A80D0" : "#4A9EFF"
                        radius: 4
                    }
                }
            }
        }

        // 底部工具栏
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            
            Button { text: "Cut"; Layout.preferredWidth: 80 }
            Button { text: "Merge"; Layout.preferredWidth: 80 }
            Item { Layout.fillWidth: true }
            Button { text: "Export"; Layout.preferredWidth: 100; highlighted: true }
        }
    }
}
