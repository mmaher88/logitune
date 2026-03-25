qt_add_executable(logitune
    main.cpp
    models/DeviceModel.cpp
    models/ButtonModel.cpp
    models/ActionModel.cpp
)

qt_add_qml_module(logitune
    URI Logitune
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/HomeView.qml
        qml/DeviceView.qml
        qml/components/SideNav.qml
        qml/components/BatteryChip.qml
        qml/components/LogituneSlider.qml
        qml/components/LogituneToggle.qml
        qml/components/InfoCallout.qml
        qml/components/DetailPanel.qml
        qml/components/KeystrokeCapture.qml
        qml/components/ButtonCallout.qml
        qml/components/ActionsPanel.qml
        qml/components/DeviceRender.qml
        qml/pages/PointScrollPage.qml
        qml/pages/ButtonsPage.qml
        qml/pages/EasySwitchPage.qml
        qml/pages/SettingsPage.qml
)

target_include_directories(logitune PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/models
)

target_link_libraries(logitune PRIVATE logitune-core Qt6::Quick Qt6::Svg Qt6::DBus)

install(TARGETS logitune DESTINATION bin)
