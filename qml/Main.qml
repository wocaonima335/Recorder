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
    property bool  screenSelected: true    // true：屏幕；false：摄像头

    flags:  Qt.Window | Qt.FramelessWindowHint

    // 添加录制相关信号
    signal startRecording()
    signal stopRecording()
    signal pauseRecording()
    signal readyRecording()
    // 用户点击不同源的信号
    signal openScreen()
    signal openCamera()

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
                opacity: mainwindow.screenSelected ? 1 : (screenMouse.containsMouse ? 1 : 0.6)
                MouseArea {
                    id: screenMouse
                    hoverEnabled: true
                    anchors.fill: parent
                    opacity: 1

                    onClicked: {
                        mainwindow.screenSelected = true
                        mainwindow.openScreen()
                    }
                }

                Image
                {
                    id:screenIcon
                    anchors.centerIn:parent
                    source:"icons/screen.png"
                    width : 40
                    height: 40
                    fillMode: Image.PreserveAspectCrop
                }
            }

            Rectangle {
                id: cameraRecordArea
                width: 80
                height: parent.height
                visible: true
                color: "#B9E8FE"
                opacity: mainwindow.screenSelected ? (cameraMouse.containsMouse ? 1 : 0.6) : 1

                MouseArea {
                    id: cameraMouse
                    hoverEnabled: true
                    anchors.fill: parent
                    opacity: 1

                    onClicked: {
                        mainwindow.screenSelected = false
                        mainwindow.openCamera()
                    }
                }

                Image {
                    id: cameraIcon
                    anchors.centerIn: parent
                    source: "icons/camera.png"
                    width: 30
                    height: 30
                    fillMode: Image.PreserveAspectCrop
                }
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
                text: recorder.captureTimeText
                style: Text.Outline
                styleColor: "white"
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

            Image
            {
                id:microPhoneIcon
                anchors.centerIn:parent
                source:"icons/microphone.png"
                width : 30
                height: 30
                fillMode: Image.PreserveAspectCrop
            }

            Text {
                anchors.bottom: parent.bottom
                anchors.bottomMargin : 10
                anchors.horizontalCenter:parent.horizontalCenter
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

            Image
            {
                id:audioIcon
                anchors.centerIn:parent
                source:"icons/audio.png"
                width : 30
                height: 30
                fillMode: Image.PreserveAspectCrop
            }

            Text {
                anchors.bottom: parent.bottom
                anchors.bottomMargin : 10
                anchors.horizontalCenter:parent.horizontalCenter
                text: "Audio"
                color: "#FFFFFF"
                font.pixelSize: 10
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

                property bool isPause: false

                Image {
                    id: pauseIcon
                    source: "icons/Pause.png"
                    anchors.centerIn:parent
                    fillMode:Image.PreserveAspectFit
                    width:parent.width * 0.8
                    height:parent.height * 0.8
                    visible:parent.opacity > 0
                    opacity : parent.opacity
                }

                SequentialAnimation on opacity {
                         id: blinkAnimation
                         running: stopButton.isPause && stopButton.opacity > 0
                         loops: Animation.Infinite

                         NumberAnimation {
                             from: 1.0
                             to: 0.3
                             duration: 500
                             easing.type: Easing.InOutQuad
                         }
                         NumberAnimation {
                             from: 0.3
                             to: 1.0
                             duration: 500
                             easing.type: Easing.InOutQuad
                         }
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
                        stopButton.isPause = !stopButton.isPause
                        if(stopButton.isPause)
                        {
                            mainwindow.pauseRecording()
                        }
                        else
                        {
                            mainwindow.readyRecording()
                        }
                    }
                }

            }
        }
    }

    ScreenTitle {
        id: screenTitle
        anchors.top: optionsArea.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        title: "1080 X 720 : Screen"
    }

    RowLayout {

        id:contentArea
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
