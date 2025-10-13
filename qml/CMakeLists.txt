# QML模块
qt_add_library(qml_module STATIC)

# 设置C++17标准
set_target_properties(qml_module PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

qt_add_qml_module(qml_module
    URI bandicam
    VERSION 1.0
    QML_FILES
    Main.qml
     QML_FILES videopage.qml
     QML_FILES settingpage.qml
     QML_FILES cuttingpage.qml
     QML_FILES settinglist.qml
)

target_link_libraries(qml_module
    PRIVATE Qt6::Quick
)
