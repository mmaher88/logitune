# ── Static library: models + AppRoot + QML module (linkable from tests) ─
add_library(logitune-app-lib STATIC
    AppRoot.cpp
    models/DeviceModel.cpp
    models/SettingsModel.cpp
    models/ButtonModel.cpp
    models/ActionFilterModel.cpp
    models/ActionModel.cpp
    models/ProfileModel.cpp
    models/EditorModel.cpp
    services/ActiveDeviceResolver.cpp
    services/DeviceCommandHandler.cpp
    services/ButtonActionDispatcher.cpp
    services/RadialMenuController.cpp
    services/CursorPositionProvider.cpp
    services/ProfileOrchestrator.cpp
    dialogs/GitHubIssueBuilder.cpp
    dialogs/CrashReportDialog.cpp
    TrayManager.cpp
)

target_include_directories(logitune-app-lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/models
    ${CMAKE_CURRENT_SOURCE_DIR}/services
)

target_link_libraries(logitune-app-lib PUBLIC logitune-core Qt6::Quick Qt6::Widgets)

# Native non-focusing overlay on KDE/wlroots Wayland compositors.
# Qt 6.5+ supports selecting the shell integration per window.
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.5)
    find_package(LayerShellQt 6.0 QUIET)
    if(TARGET LayerShellQt::Interface)
        target_link_libraries(logitune-app-lib PUBLIC LayerShellQt::Interface)
        target_compile_definitions(logitune-app-lib PRIVATE LOGITUNE_LAYER_SHELL)
    endif()
endif()
target_compile_definitions(logitune-app-lib PUBLIC
    PROJECT_VERSION="${PROJECT_VERSION}"
    PROJECT_VERSION_FULL="${LOGITUNE_DISPLAY_VERSION}"
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# Qt 6.5+ handles QML subdirectories automatically via QTP0004.
# Qt 6.4: QT_RESOURCE_ALIAS maps subdirectory files to flat type names.
if(COMMAND qt_policy)
    qt_policy(SET QTP0004 NEW)
else()
    foreach(_file
        SideNav BatteryChip LogituneSlider LogituneToggle InfoCallout
        DetailPanel KeystrokeCapture ButtonCallout ActionsPanel
        DeviceCard DeviceRender AppProfileBar Toast EditorToolbar
        EditableText ConflictBanner DiffModal HotspotControl RadialMenuOverlay RadialMenuEditor RadialMenuRing RadialActionIcon RadialActionCatalog RingCatalogItem RingIconButton RingMenuItem RingButton RingComboBox RingTextField)
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
        qml/RadialMenuSettingsView.qml
        qml/components/RadialActionCatalog.qml
        qml/components/RingCatalogItem.qml
        qml/components/RingIconButton.qml
        qml/components/RingMenuItem.qml
        qml/components/RingButton.qml
        qml/components/RingComboBox.qml
        qml/components/RingTextField.qml
        qml/components/SideNav.qml
        qml/components/BatteryChip.qml
        qml/components/LogituneSlider.qml
        qml/components/LogituneToggle.qml
        qml/components/InfoCallout.qml
        qml/components/DetailPanel.qml
        qml/components/KeystrokeCapture.qml
        qml/components/ButtonCallout.qml
        qml/components/ActionsPanel.qml
        qml/components/RadialMenuRing.qml
        qml/components/RadialActionIcon.qml
        qml/components/RadialMenuOverlay.qml
        qml/components/RadialMenuEditor.qml
        qml/components/DeviceCard.qml
        qml/components/DeviceRender.qml
        qml/components/AppProfileBar.qml
        qml/components/Toast.qml
        qml/components/EditorToolbar.qml
        qml/components/EditableText.qml
        qml/components/ConflictBanner.qml
        qml/components/DiffModal.qml
        qml/components/HotspotControl.qml
        qml/pages/PointScrollPage.qml
        qml/pages/ButtonsPage.qml
        qml/pages/EasySwitchPage.qml
        qml/pages/SettingsPage.qml
    RESOURCES
        qml/assets/actions/search.svg
        qml/assets/actions/search-dark.svg
        qml/assets/actions/settings.svg
        qml/assets/actions/settings-dark.svg
        qml/assets/actions/more.svg
        qml/assets/actions/more-dark.svg
        qml/assets/actions/plus.svg
        qml/assets/actions/plus-dark.svg
        qml/assets/actions/grid.svg
        qml/assets/actions/grid-dark.svg
        qml/assets/actions/trash.svg
        qml/assets/actions/trash-dark.svg
        qml/assets/actions/edit.svg
        qml/assets/actions/edit-dark.svg
        qml/assets/actions/help.svg
        qml/assets/actions/help-dark.svg
        qml/assets/actions/chevron.svg
        qml/assets/actions/chevron-dark.svg
        qml/assets/actions/browser.svg
        qml/assets/actions/browser-dark.svg
        qml/assets/actions/terminal.svg
        qml/assets/actions/terminal-dark.svg
        qml/assets/actions/brush.svg
        qml/assets/actions/brush-dark.svg
        qml/assets/actions/crop.svg
        qml/assets/actions/crop-dark.svg
        qml/assets/actions/eyedropper.svg
        qml/assets/actions/eyedropper-dark.svg
        qml/assets/actions/star.svg
        qml/assets/actions/star-dark.svg
        qml/assets/actions/heart.svg
        qml/assets/actions/heart-dark.svg
        qml/assets/actions/mail.svg
        qml/assets/actions/mail-dark.svg
        qml/assets/actions/chat.svg
        qml/assets/actions/chat-dark.svg
        qml/assets/actions/camera.svg
        qml/assets/actions/camera-dark.svg
        qml/assets/actions/video.svg
        qml/assets/actions/video-dark.svg
        qml/assets/actions/microphone.svg
        qml/assets/actions/microphone-dark.svg
        qml/assets/actions/file.svg
        qml/assets/actions/file-dark.svg
        qml/assets/actions/save.svg
        qml/assets/actions/save-dark.svg
        qml/assets/actions/link.svg
        qml/assets/actions/link-dark.svg
        qml/assets/actions/folder.svg
        qml/assets/actions/folder-dark.svg
        qml/assets/actions/copy.svg
        qml/assets/actions/paste.svg
        qml/assets/actions/cut.svg
        qml/assets/actions/undo.svg
        qml/assets/actions/redo.svg
        qml/assets/actions/play.svg
        qml/assets/actions/next.svg
        qml/assets/actions/previous.svg
        qml/assets/actions/stop.svg
        qml/assets/actions/volume.svg
        qml/assets/actions/volume-down.svg
        qml/assets/actions/mute.svg
        qml/assets/actions/back.svg
        qml/assets/actions/forward.svg
        qml/assets/actions/keyboard.svg
        qml/assets/actions/launch.svg
        qml/assets/actions/screenshot.svg
        qml/assets/actions/desktop.svg
        qml/assets/actions/windows.svg
        qml/assets/actions/close.svg
        qml/assets/actions/brightness.svg
        qml/assets/actions/calculator.svg
        qml/assets/actions/dpi.svg
        qml/assets/actions/wheel.svg
        qml/assets/actions/none.svg
        qml/assets/actions/action.svg
        qml/assets/actions/action-dark.svg
        qml/assets/actions/back-dark.svg
        qml/assets/actions/brightness-dark.svg
        qml/assets/actions/calculator-dark.svg
        qml/assets/actions/close-dark.svg
        qml/assets/actions/copy-dark.svg
        qml/assets/actions/cut-dark.svg
        qml/assets/actions/desktop-dark.svg
        qml/assets/actions/dpi-dark.svg
        qml/assets/actions/forward-dark.svg
        qml/assets/actions/keyboard-dark.svg
        qml/assets/actions/launch-dark.svg
        qml/assets/actions/mute-dark.svg
        qml/assets/actions/next-dark.svg
        qml/assets/actions/none-dark.svg
        qml/assets/actions/paste-dark.svg
        qml/assets/actions/play-dark.svg
        qml/assets/actions/previous-dark.svg
        qml/assets/actions/redo-dark.svg
        qml/assets/actions/screenshot-dark.svg
        qml/assets/actions/stop-dark.svg
        qml/assets/actions/undo-dark.svg
        qml/assets/actions/volume-dark.svg
        qml/assets/actions/volume-down-dark.svg
        qml/assets/actions/wheel-dark.svg
        qml/assets/actions/windows-dark.svg
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
