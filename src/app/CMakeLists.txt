# ── Static library: models + AppController + QML module (linkable from tests) ─
add_library(logitune-app-lib STATIC
    AppController.cpp
    models/DeviceModel.cpp
    models/SettingsModel.cpp
    models/ButtonModel.cpp
    models/ActionModel.cpp
    models/ProfileModel.cpp
    dialogs/GitHubIssueBuilder.cpp
    dialogs/CrashReportDialog.cpp
    TrayManager.cpp
)

target_include_directories(logitune-app-lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/models
)

target_link_libraries(logitune-app-lib PUBLIC logitune-core Qt6::Quick Qt6::Widgets)
target_compile_definitions(logitune-app-lib PUBLIC PROJECT_VERSION="${PROJECT_VERSION}")

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Qt 6.5+ handles QML subdirectories automatically via QTP0004.
# Qt 6.4: QT_RESOURCE_ALIAS maps subdirectory files to flat type names.
if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
else()
    foreach(_file
        SideNav BatteryChip LogituneSlider LogituneToggle InfoCallout
        DetailPanel KeystrokeCapture ButtonCallout ActionsPanel
        DeviceRender AppProfileBar Toast)
        set_source_files_properties(qml/components/${_file}.qml
            PROPERTIES QT_RESOURCE_ALIAS qml/${_file}.qml)
    endforeach()
    foreach(_file PointScrollPage ButtonsPage EasySwitchPage SettingsPage)
        set_source_files_properties(qml/pages/${_file}.qml
            PROPERTIES QT_RESOURCE_ALIAS qml/${_file}.qml)
    endforeach()
endif()

qt_add_qml_module(logitune-app-lib
    URI Logitune
    VERSION 1.0
    RESOURCE_PREFIX /
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/Logitune
    QML_FILES
        qml/Theme.qml
        qml/Main.qml
        qml/HomeView.qml
        qml/DeviceView.qml
        qml/AppSettingsView.qml
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
        qml/components/AppProfileBar.qml
        qml/components/Toast.qml
        qml/pages/PointScrollPage.qml
        qml/pages/ButtonsPage.qml
        qml/pages/EasySwitchPage.qml
        qml/pages/SettingsPage.qml
    RESOURCES
        qml/assets/mx-master-2s.png
        qml/assets/mx-master-2s-back.png
        qml/assets/mx-master-2s-side.png
        qml/assets/mx-master-3s.png
        qml/assets/mx-master-3s-back.png
        qml/assets/mx-master-3s-side.png
        qml/assets/mx-master-4.png
        qml/assets/mx-master-4-back.png
        qml/assets/mx-master-4-side.png
        qml/assets/logitune-icon.svg
        qml/assets/logitune-tray.svg
)

# ── Executable: main entry point ─────────────────────────────────────────
qt_add_executable(logitune
    main.cpp
)

target_link_libraries(logitune PRIVATE logitune-app-lib logitune-app-libplugin Qt6::Svg Qt6::DBus Qt6::Widgets)
target_link_options(logitune PRIVATE -rdynamic)

install(TARGETS logitune DESTINATION bin)
