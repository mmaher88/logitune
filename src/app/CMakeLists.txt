# ── Static library: models + AppController + QML module (linkable from tests) ─
add_library(logitune-app-lib STATIC
    AppController.cpp
    models/DeviceModel.cpp
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

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
endif()

# Qt 6.4 compat: strip subdirectory prefix so types register as top-level module types.
# e.g. qml/components/Toast.qml registers as "Toast" not "components/Toast"
set_source_files_properties(qml/components/SideNav.qml         PROPERTIES QT_RESOURCE_ALIAS qml/SideNav.qml)
set_source_files_properties(qml/components/BatteryChip.qml     PROPERTIES QT_RESOURCE_ALIAS qml/BatteryChip.qml)
set_source_files_properties(qml/components/LogituneSlider.qml  PROPERTIES QT_RESOURCE_ALIAS qml/LogituneSlider.qml)
set_source_files_properties(qml/components/LogituneToggle.qml  PROPERTIES QT_RESOURCE_ALIAS qml/LogituneToggle.qml)
set_source_files_properties(qml/components/InfoCallout.qml     PROPERTIES QT_RESOURCE_ALIAS qml/InfoCallout.qml)
set_source_files_properties(qml/components/DetailPanel.qml     PROPERTIES QT_RESOURCE_ALIAS qml/DetailPanel.qml)
set_source_files_properties(qml/components/KeystrokeCapture.qml PROPERTIES QT_RESOURCE_ALIAS qml/KeystrokeCapture.qml)
set_source_files_properties(qml/components/ButtonCallout.qml   PROPERTIES QT_RESOURCE_ALIAS qml/ButtonCallout.qml)
set_source_files_properties(qml/components/ActionsPanel.qml    PROPERTIES QT_RESOURCE_ALIAS qml/ActionsPanel.qml)
set_source_files_properties(qml/components/DeviceRender.qml    PROPERTIES QT_RESOURCE_ALIAS qml/DeviceRender.qml)
set_source_files_properties(qml/components/AppProfileBar.qml   PROPERTIES QT_RESOURCE_ALIAS qml/AppProfileBar.qml)
set_source_files_properties(qml/components/Toast.qml           PROPERTIES QT_RESOURCE_ALIAS qml/Toast.qml)
set_source_files_properties(qml/pages/PointScrollPage.qml      PROPERTIES QT_RESOURCE_ALIAS qml/PointScrollPage.qml)
set_source_files_properties(qml/pages/ButtonsPage.qml          PROPERTIES QT_RESOURCE_ALIAS qml/ButtonsPage.qml)
set_source_files_properties(qml/pages/EasySwitchPage.qml       PROPERTIES QT_RESOURCE_ALIAS qml/EasySwitchPage.qml)
set_source_files_properties(qml/pages/SettingsPage.qml         PROPERTIES QT_RESOURCE_ALIAS qml/SettingsPage.qml)

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
        qml/assets/mx-master-3s.png
        qml/assets/mx-master-3s-back.png
        qml/assets/mx-master-3s-side.png
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
