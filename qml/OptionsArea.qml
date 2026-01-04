import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Effects

Rectangle {
    id: optionsArea
    height: 80
    radius: 8
    color: "#2C2F3C"

    // 添加顶部渐变效果
    gradient: Gradient {
        GradientStop { position: 0.0; color: "#2E3142" }
        GradientStop { position: 1.0; color: "#252834" }
    }


    property bool screenSelected: true
    property var recorderObject: null

    // 信号转发 - 用于触发录制动作
    signal screenClicked()
    signal cameraClicked()
    signal startRecordClicked()
    signal stopRecordClicked()
    signal pauseRecordClicked()
    signal resumeRecordClicked()

    Row {
        id: optionDevice
        spacing: 8
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 6

        // 屏幕录制按钮
        SourceSelectButton {
            id: screenRecordArea
            isSelected: screenSelected
            iconSource: "icons/screen.png"
            labelText: "屏幕"
            iconWidth: 32
            iconHeight: 32

            onClicked: {
                screenSelected = true
                screenClicked()
            }
        }

        // 摄像头录制按钮
        SourceSelectButton {
            id: cameraRecordArea
            isSelected: !screenSelected
            iconSource: "icons/camera.png"
            labelText: "摄像头"
            iconWidth: 28
            iconHeight: 28

            onClicked: {
                screenSelected = false
                cameraClicked()
            }
        }
    }

    VerticalSeparator {
        id: deviceSep
        anchors.left: optionDevice.right
    }

    TimeDisplay {
        id: timeShowArea
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: deviceSep.right
        anchors.leftMargin: 4
        width: 160
        recorderObject: optionsArea.recorderObject
    }

    VerticalSeparator {
        id: timeSep
        anchors.left: timeShowArea.right
        anchors.leftMargin: 4
    }

    // 中间区域容器，用于居中音频指示器
    Item {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: timeSep.right
        anchors.right: audioSep.left

        Row {
            anchors.centerIn: parent
            spacing: 0

            AudioIndicator {
                id: microPhoneArea
                labelText: "MICROPHONE"
                iconSource: "icons/microphone.png"
                recorderObject: optionsArea.recorderObject
                visualizerEnabled: false

                onClicked: {
                    console.log("Microphone area clicked")
                }
            }

            AudioIndicator {
                id: audioArea
                labelText: "AUDIO"
                iconSource: "icons/audio.png"
                recorderObject: optionsArea.recorderObject
                visualizerEnabled: true
                visualizerMode: "volume"

                onClicked: {
                    console.log("Audio area clicked - mode:", visualizerMode)
                }
            }
        }
    }

    VerticalSeparator {
        id: audioSep
        anchors.right: recordActionArea.left
    }

    RecordButton {
        id: recordActionArea
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 140

        recorderObject: optionsArea.recorderObject
        onStartClicked: optionsArea.startRecordClicked()
        onStopClicked: optionsArea.stopRecordClicked()
        onPauseClicked: optionsArea.pauseRecordClicked()
        onResumeClicked: optionsArea.resumeRecordClicked()
    }
}
