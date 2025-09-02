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
    color: "white"
    title: qsTr("bandicam")

    Rectangle {
        id: optionsArea
        anchors.left: parent.left
        anchors.right: parent.right
        height: 70
        visible: true
        color: "green"
    }

    RowLayout {
        anchors.top: optionsArea.bottom
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

                Button
                {
                    id:videobutton
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 50
                    Layout.alignment: Qt.AlignHCenter
                    flat: true
                    background: Rectangle
                    {
                        color:"red"
                    }

                    onClicked:
                    {
                        if(stackview) stackview.push("videopage.qml")
                    }
                }

                Button
                {
                    id :settingbutton
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 50
                    Layout.alignment: Qt.AlignHCenter
                    flat: true
                    background: Rectangle
                    {
                        color:"gray"
                    }

                    onClicked:
                    {
                        if(stackview) stackview.push("settingpage.qml")
                    }
                }

                Button
                {
                    id :cuttingbutton
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 50
                    Layout.alignment: Qt.AlignHCenter
                    flat:true
                    background: Rectangle
                    {
                        color:"blue"
                    }

                    onClicked:
                    {
                        if(stackview) stackview.push("cuttingpage.qml")
                    }
                }

            }
        }

        Rectangle
        {
            id :rightPanel
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
