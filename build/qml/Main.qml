import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.15


ApplicationWindow{
    id: mainwindow
    width: 640
    height: 500
    visible: true
    color: "#3F4F73"
    title: qsTr("bandicam")

    property color sepColor: "#40454F"     // 分隔线颜色
    property int   sepWidth: 1             // 分隔线宽度
    property real  sepOpacity: 0.9         // 分隔线不透明度

    flags:  Qt.Window | Qt.FramelessWindowHint

    // 添加录制相关信号
    signal startRecording()
    signal stopRecording()

    Item
    {
        id:titleBar
        anchors.left: parent.left
        anchors.leftMargin:10
        anchors.right: parent.right
        anchors.top: parent.top
        height: 30
        Rectangle { anchors.fill: parent; color: "transparent" }

        // 推荐 DragHandler
        DragHandler {
            target: null                // 不移动子项，只触发拖拽
            onActiveChanged: if (active) mainwindow.startSystemMove()
        }
        Text
        {
            anchors.left :parent.left
            height:20
            text: "X Record"
            font.pixelSize: 20
            font.bold: true
            color: "#FFFFFF"
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
                id:showText
                anchors.centerIn: parent
                font.pointSize: 30
                font.family: "Consolas, Monaco, 'Courier New', monospace" // 等宽字体
                color: palette.text
                // 可选：添加数字显示效果
                style: Text.Outline
                styleColor: "white"
            }

            Timer {
                running: true; interval: 100; repeat: true
                onTriggered: {
                    var totalSeconds = 60000 / 1000
                    var m = Math.floor(totalSeconds / 60)
                    var s = Math.floor(totalSeconds % 60)
                    var ms = Math.floor((totalSeconds % 1) * 10)
                    showText.text= `${m.toString().padStart(2, '0')}:${s.toString().padStart(2, '0')}.${ms}`
                }
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
                height: 85
                width: 85
                radius: 85
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
                        
                        // 发送录制信号
                        if (recordButton.isRecording) {
                            mainwindow.startRecording()
                        } else {
                            mainwindow.stopRecording()
                        }
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
                x : parent.width - width - 24
                height: 40
                width: 40
                color: "transparent"
                z: 1

                Image {
                    id: pauseIcon
                    source: "file:///e:/myProgram/Recorder/record/icons/Pause.png"
                    anchors.centerIn:parent
                    fillMode:Image.PreserveAspectFit
                    width:parent.width * 0.8
                    height:parent.height * 0.8
                    visible:parent.opacity > 0
                    opacity : parent.opacity
                }

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
            Layout.preferredWidth: 150
            color : "#2C2F3C"
            Layout.fillHeight: true

            // 使用 settinglist.qml 中的 ListView
            Loader {
                id: settingListLoader
                anchors.fill: parent
                source: "settinglist.qml"

                onLoaded: {
                    // 可以在这里处理加载完成后的逻辑
                    if (item) {
                        // 连接点击事件
                        item.itemClicked.connect(function(index, title) {
                            handleSettingItemClick(index, title)
                        })
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
    
    // 处理设置列表项点击事件
    function handleSettingItemClick(index, title) {
        console.log("点击了:", title, "索引:", index)
        
        switch(index) {
            case 0: // 主页
                if (stackview)
                    stackview.push("videopage.qml")
                break
            case 1: // 设置
                if (stackview)
                    stackview.push("settingpage.qml")
                break
            case 2: // 视频
                if (stackview)
                    stackview.push("cuttingpage.qml")
                break
            default:
                console.log("未知的菜单项")
        }
    }
}
