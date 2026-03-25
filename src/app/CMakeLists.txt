qt_add_executable(logitune main.cpp)

qt_add_qml_module(logitune
    URI Logitune
    VERSION 1.0
    QML_FILES
        qml/Main.qml
)

target_link_libraries(logitune PRIVATE logitune-core Qt6::Quick Qt6::Svg Qt6::DBus)

install(TARGETS logitune DESTINATION bin)
