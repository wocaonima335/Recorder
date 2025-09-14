# QML模块
qt_add_library(qml_module STATIC)

qt_add_qml_module(qml_module
    URI bandicam
    VERSION 1.0
    QML_FILES
    Main.qml
     QML_FILES videopage.qml
     QML_FILES settingpage.qml
     QML_FILES cuttingpage.qml
)

target_link_libraries(qml_module
    PRIVATE Qt6::Quick
)
