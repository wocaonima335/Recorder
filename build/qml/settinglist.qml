import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls

pragma ComponentBehavior: Bound

Item {
    id: root

    // 添加信号，用于通知点击事件
    signal itemClicked(int index, string title)
    
    // 添加属性来跟踪当前选中的Item
    property int currentIndex: 0

    ListModel {
        id: settingList

        ListElement {
            title: "主页"
            icon: "icons/main.png"
            iconGray: "icons/main-gray.png"
        }
        ListElement {
            title: "设置"
            icon: "icons/Setting.png"
            iconGray: "icons/setting-gray.png"
        }
        ListElement {
            title: "视频"
            icon: "icons/video.png"
            iconGray: "icons/video-gray.png"
        }
    }

    ListView {
        id: listView
        anchors.fill: parent
        orientation: ListView.Vertical
        spacing: 0
        model: settingList

        delegate: Item {
            id: delegateFrame
            width: root.width
            height: 50

            required property string icon
            required property string iconGray
            required property string title
            required property int index

            // 添加背景Rectangle来实现高光效果
            Rectangle {
                id: backgroundRect
                anchors.left :parent.left
                anchors.top :parent.top
                anchors.bottom:parent.bottom
                width:4
                color: delegateFrame.index === root.currentIndex ? "#5F8EFC" : "transparent"
                opacity:1
                radius: 1

                
                // 添加平滑的颜色过渡动画
                Behavior on color {
                    ColorAnimation {
                        duration: 200
                        easing.type: Easing.OutQuad
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin:10
                anchors.margins: 5
                spacing:10
                
                Image {
                    id: image
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    source: delegateFrame.index === root.currentIndex ? delegateFrame.icon : delegateFrame.iconGray
                    fillMode: Image.PreserveAspectFit
                }

                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideLeft
                    text: delegateFrame.title
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    color:delegateFrame.index === root.currentIndex ? "#FFFFFF" : "gray"
                    font.pixelSize: 16
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    // 更新当前选中的索引
                    root.currentIndex = delegateFrame.index
                    root.itemClicked(delegateFrame.index, delegateFrame.title)
                }
            }
        }
    }
}
