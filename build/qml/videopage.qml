import QtQuick 2.15
import GLPreview 1.0

Item {
    id: videopage

    GLView {
        id: previewView
        anchors.fill: parent
        keepRatio: true
        objectName: "previewView"
    }
}
