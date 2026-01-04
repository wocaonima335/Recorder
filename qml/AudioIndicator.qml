import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Effects

Item {
    id: root
    width: 80
    height: 80

    property string labelText: "AUDIO"
    property string iconSource: ""
    property var recorderObject: null
    property string visualizerMode: "volume"
    property bool visualizerEnabled: true

    signal clicked()

    property bool isHovered: mouseArea.containsMouse
    property bool isRecording: recorderObject ? recorderObject.isRecording : false

    PanelBackground {
        anchors.fill: parent
        isHovered: root.isHovered
        isRecording: root.isRecording
    }

    Image {
        id: iconImage
        anchors.centerIn: parent
        source: iconSource
        width: 30
        height: 30
        fillMode: Image.PreserveAspectCrop
        visible: !audioVisualizerLoader.active
        opacity: visible ? 1 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: 200
            }
        }
    }

    Loader {
        id: audioVisualizerLoader
        anchors.fill: parent
        anchors.margins: 8
        anchors.topMargin: 10
        anchors.bottomMargin: 12

        active: visualizerEnabled && isRecording && recorderObject && recorderObject.audioSampler
        asynchronous: true
        opacity: active ? 1 : 0

        sourceComponent: Component {
            AudioVisualizer {
                audioSampler: recorderObject ? recorderObject.audioSampler : null
                mode: root.visualizerMode
            }
        }

        Behavior on opacity {
            NumberAnimation {
                duration: 400
                easing.type: Easing.OutCubic
            }
        }
    }

    Text {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        anchors.horizontalCenter: parent.horizontalCenter
        text: labelText
        color: isRecording ? "#4A9EFF" : (isHovered ? "#FFFFFF" : "#B0B0B0")
        font.pixelSize: 10
        font.bold: true
        font.letterSpacing: 0.3

        Behavior on color {
            ColorAnimation {
                duration: 200
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            root.clicked()
        }
    }

    scale: isHovered ? 1.05 : 1.0
    transformOrigin: Item.Center
    Behavior on scale {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }
}
