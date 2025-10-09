import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.15
import Qt5Compat.GraphicalEffects

ApplicationWindow{
    id: mainwindow
    width: 640
    height: 500
    visible: true
    color: "#3F4F73"
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
                color: "#B9E8FE"
            }

            Rectangle {
                id: cameraRecordArea
                width: 80
                height: parent.height
                visible: true
                color: "#B9E8FE"
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

       Item {
            id: recordActionArea
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.left: audioArea.right


            // 原有的录制按钮仍在 recordActionArea 内，层效果会包含所有子项
            Rectangle {
                id: recordButton
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height
                width: 80
                radius: 80
                color: isRecording ? "#FF4444" : "#FF0000"
                border.color: "#FFFFFF"
                border.width: 2
                property bool isRecording: false
                z: 2

                // 使用 x 坐标来控制位置，避免锚点冲突
                x: isRecording ? 5 : parent.width - width - 5

                // 添加位置动画
                Behavior on x {
                    NumberAnimation {
                        duration: 300
                        easing.type: Easing.OutCubic
                    }
                }

                // 添加点击事件
                MouseArea {
                    anchors.fill: recordButton
                    onClicked: {
                        recordButton.isRecording = !recordButton.isRecording
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "REC"
                    color: "#FFFFFF"
                    font.pixelSize: 18
                    font.bold: true
                }
            }

            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.antialiasing = true;

                    // 画黑色条
                    ctx.beginPath();
                    roundRect(ctx, 16, 12, width-32, height-24, 10);
                    ctx.fillStyle = "#202225";
                    ctx.fill();
                }

                function roundRect(ctx, x, y, w, h, r) {
                    ctx.moveTo(x + r, y);
                    ctx.arcTo(x + w, y,     x + w, y + h, r);
                    ctx.arcTo(x + w, y + h, x,     y + h, r);
                    ctx.arcTo(x,     y + h, x,     y,     r);
                    ctx.arcTo(x,     y,     x + w, y,     r);
                    ctx.closePath();
                }
            }


            // 停止按钮，位于recordButton下方
            Rectangle {
                id: stopButton
                anchors.verticalCenter: parent.verticalCenter
                x : parent.width - width -2
                height: 30
                width: 50
                radius: 20
                color: "#666666"
                border.color: "#FFFFFF"
                border.width: 2
                z: 1

                // 只有在录制时才显示
                opacity: recordButton.isRecording ? 1.0 : 0.0
                visible: opacity > 0

                // 添加透明度动画
                Behavior on opacity {
                    NumberAnimation {
                        duration: 300
                        easing.type: Easing.OutCubic
                    }
                }

                // 添加点击事件
                MouseArea {
                    anchors.fill: parent
                    onClicked: {

                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "STOP"
                    color: "#FFFFFF"
                    font.pixelSize: 12
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
        anchors.bottomMargin: 20
        spacing : 0

        Rectangle {
            id: leftPanel
            Layout.preferredWidth: 100
            color : "#2C2F3C"
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
