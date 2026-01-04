import QtQuick 2.15

Item {
    id : settingpage

    // 设置页不显示预览
    property bool showPreview: false

    Rectangle
    {
        id : settingarea
        color: "blue"
        anchors.fill: parent
    }
}
