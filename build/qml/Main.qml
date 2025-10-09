import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.15
import Qt5Compat.GraphicalEffects



ApplicationWindow {
    id: mainwindow
    width: 640
    height: 480
    visible: true
    color: "gray"
    title: qsTr("bandicam")

    // 统一的分隔线样式（建议放在 optionsArea 里作为属性）
    property color sepColor: "#40454F"     // 分隔线颜色
    property int   sepWidth: 1             // 分隔线宽度
    property real  sepOpacity: 0.9         // 分隔线不透明度

    flags:  Qt.Window | Qt.FramelessWindowHint

    Item
    {
        id:titleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 30
        Rectangle { anchors.fill: parent; color: "transparent" }

        // 推荐 DragHandler
        DragHandler {
            target: null                // 不移动子项，只触发拖拽
            onActiveChanged: if (active) mainwindow.startSystemMove()
        }
    }

    Button
    {
        id:closeButton
        anchors.top:parent.top
        anchors.right: parent.right
        width:30
        height: 30

        background: Rectangle
        {
            anchors.fill:parent
            color: parent.hovered ? "#e64340" : "transparent"
            Text {
                text: "×"
                color: "#FFFFFF"
                anchors.centerIn: parent
                font.pixelSize: 18
            }
        }
        onClicked: Qt.quit()
    }

    Rectangle {
        id: optionsArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin:30
        height: 80
        radius : 1
        visible: true
        color: "#2C2F3C"
        Row {
            id: optionDevice
            spacing: 1
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.margins: 2

            Rectangle {
                id: screenRecordArea
                width: 80
                height: parent.height
                visible: true
                color: "#8A9BE3"
            }

            Rectangle {
                id: cameraRecordArea
                width: 80
                height: parent.height
                visible: true
                color: "#8A9BE3"
            }
        }

        Item {
            id: timeShowArea
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: optionDevice.right
            height: parent.height
            width: 160

            Rectangle { anchors.left: parent.left;  anchors.top: parent.top; anchors.bottom: parent.bottom; width: sepWidth; color: sepColor; opacity: sepOpacity }
            Rectangle { anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: sepWidth; color: sepColor; opacity: sepOpacity }

            Text {
                anchors.centerIn: parent
                text: "TIME"
                color: "#FFFFFF"
                font.pixelSize: 14
                font.bold: true
            }
        }
        Item
        {
            id : microPhoneArea
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: timeShowArea.right
            height: parent.height
            width: 80

            Rectangle { anchors.left: parent.left;  anchors.top: parent.top; anchors.bottom: parent.bottom; width: sepWidth; color: sepColor; opacity: sepOpacity }
            Rectangle { anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: sepWidth; color: sepColor; opacity: sepOpacity }

            Text {
                anchors.centerIn: parent
                text: "MICROPHONE"
                color: "#FFFFFF"
                font.pixelSize: 8
                font.bold: true
            }
        }

        Item
        {
            id : audioArea
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: microPhoneArea.right
            height: parent.height
            width: 80

            Rectangle { anchors.left: parent.left;  anchors.top: parent.top; anchors.bottom: parent.bottom; width: sepWidth; color: sepColor; opacity: sepOpacity }
            Rectangle { anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: sepWidth; color: sepColor; opacity: sepOpacity }

            Text {
                anchors.centerIn: parent
                text: "Audio"
                color: "#FFFFFF"
                font.pixelSize: 8
                font.bold: true
            }
        }

        Item
        {
            id:stopButton
            anchors.left: audioArea.right
            height: parent.height
            width: 80

            Rectangle { anchors.left: parent.left;  anchors.top: parent.top; anchors.bottom: parent.bottom; width: sepWidth; color: sepColor; opacity: sepOpacity }
            Rectangle { anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom; width: sepWidth; color: sepColor; opacity: sepOpacity }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 10
                text: "Stop"
                color: "#FFFFFF"
                font.pixelSize: 8
                font.bold: true
            }

        }

        // 参考：让遮罩真正生效（反向挖空：白保留、黑挖空）
        Item {
            id: recordActionArea
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.left: audioArea.right

            Canvas {
                id: actionCanvas
                anchors.fill: parent
                antialiasing: true    // 抗锯齿让圆边更平滑
                z: 1                  // 让画的圆在按钮下面；recordButton 的 z: 2

                onPaint: {
                    const ctx = getContext("2d")
                    // 清空画布
                    ctx.clearRect(0, 0, width, height)

                    // 灰色填充
                    ctx.fillStyle = "#808080"   // 你要的 gray，可改为 "gray" 或具体色值

                    // 计算圆心与半径（对准 recordButton 中心）
                    const r  = recordButton.width / 2 + 10
                    const cx = recordButton.x + recordButton.width  / 2
                    const cy = recordButton.y + recordButton.height / 2

                    // 绘制圆
                    ctx.beginPath()
                    ctx.arc(cx, cy, r, 0, Math.PI * 2)
                    ctx.closePath()
                    ctx.fill()
                }

            }

            // 原有的录制按钮仍在 recordActionArea 内，层效果会包含所有子项
            Rectangle {
                id: recordButton
                anchors.right: parent.right
                anchors.rightMargin: 30
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height
                width: 80
                radius: 80
                color: isRecording ? "#FF4444" : "#FF0000"
                border.color: "#FFFFFF"
                border.width: 2
                property bool isRecording: false
                z: 2

                Text {
                    anchors.centerIn: parent
                    text: "REC"
                    color: "#FFFFFF"
                    font.pixelSize: 18
                    font.bold: true
                }
            }
        }
    }

    RowLayout {
        anchors.top: optionsArea.bottom
        anchors.topMargin: 20
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Rectangle {
            id: leftPanel
            Layout.preferredWidth: 100
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                Layout.fillHeight: true
                anchors.topMargin: 20
                spacing: 10

                Button {
                    id: videobutton
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 50
                    Layout.alignment: Qt.AlignHCenter
                    flat: true
                    background: Rectangle {
                        color: "red"
                    }

                    onClicked: {
                        if (stackview)
                            stackview.push("videopage.qml")
                    }
                }

                Button {
                    id: settingbutton
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 50
                    Layout.alignment: Qt.AlignHCenter
                    flat: true
                    background: Rectangle {
                        color: "gray"
                    }

                    onClicked: {
                        if (stackview)
                            stackview.push("settingpage.qml")
                    }
                }

                Button {
                    id: cuttingbutton
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 50
                    Layout.alignment: Qt.AlignHCenter
                    flat: true
                    background: Rectangle {
                        color: "blue"
                    }

                    onClicked: {
                        if (stackview)
                            stackview.push("cuttingpage.qml")
                    }
                }
            }
        }

        Rectangle {
            id: rightPanel
            Layout.fillWidth: true
            Layout.fillHeight: true

            StackView {
                id: stackview
                anchors.fill: parent
                initialItem: "videopage.qml"
            }
        }
    }
}
