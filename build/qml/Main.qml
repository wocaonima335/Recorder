import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Effects
import GLPreview 1.0

ApplicationWindow {
    id: mainwindow
    width: 640
    height: 500
    visible: true
    color: "transparent" // Set transparent to let backgroundRectangle show rounded corners
    title: qsTr("bandicam")

    property color sepColor: "#40454F"
    property int sepWidth: 1
    property real sepOpacity: 0.9
    property bool screenSelected: true

    flags: Qt.Window | Qt.FramelessWindowHint

    // 信号定义
    signal startRecording
    signal stopRecording
    signal pauseRecording
    signal readyRecording
    signal openScreen
    signal openCamera

    // 主背景容器
    Rectangle {
        id: mainBackground
        anchors.fill: parent
        radius: 0 // 如果想要圆角窗口可以设为 10，但无边框窗口通常铺满
        color: "#2B2E38"
        
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#323642" }
            GradientStop { position: 1.0; color: "#252830" }
        }

        // 窗口边框
        border.color: "#40454F"
        border.width: 1
    }

    // 标题栏
    Item {
        id: titleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 32 //稍作增高

        // 拖拽区域
        DragHandler {
            target: null
            onActiveChanged: if (active) mainwindow.startSystemMove()
        }

        // 标题文字
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            // 甚至可以加个小 logo
            Rectangle {
                width: 18
                height: 18
                radius: 4
                color: "#FF4444" 
                anchors.verticalCenter: parent.verticalCenter
                
                Text {
                    anchors.centerIn: parent
                    text: "B"
                    font.bold: true
                    font.pixelSize: 12
                    color: "white"
                }
            }

            Text {
                text: "X Record"
                font.pixelSize: 14
                font.bold: true
                font.family: "Segoe UI"
                color: "#E0E0E0"
                anchors.verticalCenter: parent.verticalCenter
                
                style: Text.Outline
                styleColor: "#20000000"
            }
        }

        // 关闭按钮
        Rectangle {
            id: closeButton
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom // 填满高度便于点击
            width: 48
            color: closeMouse.containsMouse ? "#C42B1C" : "transparent"
            
            // 右上角圆角处理 (如果窗口有圆角)
            // radius: 10 
            
            Behavior on color { ColorAnimation { duration: 150 } }

            Text {
                text: "×"
                color: "#FFFFFF"
                anchors.centerIn: parent
                font.pixelSize: 20
                scale: closeMouse.pressed ? 0.9 : 1.0
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: Qt.quit()
            }
        }
    }

    // 内容加载器
    Loader {
        id: optionsAreaLoader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        anchors.topMargin: 0
        height: 80
        asynchronous: true
        z: 1 // 保证在阴影上方

        sourceComponent: OptionsArea {
            screenSelected: mainwindow.screenSelected
            recorderObject: recorder

            onScreenClicked: {
                mainwindow.screenSelected = true
                mainwindow.openScreen()
            }
            onCameraClicked: {
                mainwindow.screenSelected = false
                mainwindow.openCamera()
            }
            onStartRecordClicked: mainwindow.startRecording()
            onStopRecordClicked: mainwindow.stopRecording()
        }
    }

    property alias optionsArea: optionsAreaLoader

    // 屏幕信息标题
    ScreenTitle {
        id: screenTitle
        anchors.top: optionsArea.bottom
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.right: parent.right
        title: "1080 X 720 : Screen"
        z: 0
    }

    // 主内容区
    RowLayout {
        id: contentArea
        anchors.top: screenTitle.bottom // 调整对齐
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 0

        // 左侧侧边栏
        Rectangle {
            id: leftPanel
            Layout.preferredWidth: 150
            Layout.fillHeight: true
            color: "transparent" // 透明以显示主背景
            
            // 分隔线
            Rectangle {
                anchors.right: parent.right
                height: parent.height
                width: 1
                color: "#3A4050"
            }

            Loader {
                id: settingListLoader
                anchors.fill: parent
                source: "settinglist.qml"
                onLoaded: {
                    if (item) {
                        item.itemClicked.connect(function (index, title) {
                            handleSettingItemClick(index, title)
                        })
                    }
                }
            }
        }

        // 右侧内容区
        Rectangle {
            id: rightPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#23262E" // 比背景稍深，凸显内容

            GLView {
                id: previewView
                anchors.fill: parent
                keepRatio: true
                objectName: "previewView"
                visible: stackview.currentItem && stackview.currentItem.showPreview === true
            }

            StackView {
                id: stackview
                anchors.fill: parent
                initialItem: "videopage.qml"
            }
        }
    }

    function handleSettingItemClick(index, title) {
        console.log("点击了:", title, "索引:", index)
        switch (index) {
        case 0: if (stackview) stackview.push("videopage.qml"); break
        case 1: if (stackview) stackview.push("settingpage.qml"); break
        case 2: if (stackview) stackview.push("cuttingpage.qml"); break
        default: console.log("未知的菜单项")
        }
    }
}
