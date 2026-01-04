import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Item {
    id: settingpage
    property bool showPreview: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Text {
            text: "SETTINGS"
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

        // 设置项示例
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 15

            // 选项 1
            RowLayout {
                spacing: 10
                Text {
                    text: "Start recording on launch"
                    color: "#E0E0E0"
                    font.pixelSize: 14
                    Layout.fillWidth: true
                }
                Switch {
                    checked: false
                    // 自定义 Switch 样式 (可选)
                }
            }

            // 选项 2
            RowLayout {
                spacing: 10
                Text {
                    text: "Show FPS overlay"
                    color: "#E0E0E0"
                    font.pixelSize: 14
                    Layout.fillWidth: true
                }
                Switch {
                    checked: true
                }
            }
            
            // 选项 3
            RowLayout {
                spacing: 10
                Text {
                    text: "Output Format"
                    color: "#E0E0E0"
                    font.pixelSize: 14
                }
                Item { Layout.fillWidth: true } // Spacer
                ComboBox {
                    model: ["MP4", "AVI", "MKV"]
                    Layout.preferredWidth: 100
                }
            }
        }

        Item { Layout.fillHeight: true } // 底部填充
    }
}
