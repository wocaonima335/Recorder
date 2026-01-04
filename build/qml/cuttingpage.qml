import QtQuick 2.15

Item {
    id:cuttingpage

    // 剪辑页不显示预览
    property bool showPreview: false

    Rectangle
    {
        id : cuttingarea
        color: "green"
        anchors.fill:parent
    }

}
