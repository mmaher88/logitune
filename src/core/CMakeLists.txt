add_library(logitune-core STATIC)

target_sources(logitune-core PRIVATE
    DeviceManager.cpp
    DeviceSession.cpp
    PhysicalDevice.cpp
    hidpp/HidppTypes.cpp
    hidpp/HidrawDevice.cpp
    hidpp/Transport.cpp
    hidpp/FeatureDispatcher.cpp
    hidpp/CommandProcessor.cpp
    hidpp/features/SmartShift.cpp
    hidpp/features/HiResWheel.cpp
    hidpp/features/ThumbWheel.cpp
    hidpp/features/ReprogControls.cpp
    hidpp/features/GestureV2.cpp
    hidpp/features/Battery.cpp
    hidpp/features/AdjustableDPI.cpp
    hidpp/features/DeviceName.cpp
    hidpp/ReceiverInfo.cpp
    hidpp/capabilities/BatteryCapability.cpp
    hidpp/capabilities/SmartShiftCapability.cpp
    hidpp/capabilities/ReprogControlsCapability.cpp
    ProfileEngine.cpp
    RadialMenuConfig.cpp
    ActionExecutor.cpp
    DeviceRegistry.cpp
    devices/DescriptorWriter.cpp
    devices/JsonDevice.cpp
    actions/ActionPreset.cpp
    actions/ActionPresetRegistry.cpp
    interfaces/IDesktopIntegration.cpp
    interfaces/IInputInjector.cpp
    interfaces/ITransport.cpp
    desktop/LinuxDesktopBase.cpp
    desktop/GnomeDesktop.cpp
    desktop/HyprlandDesktop.cpp
    desktop/KDeDesktop.cpp
    desktop/GenericDesktop.cpp
    input/UinputInjector.cpp
    logging/LogManager.cpp
    logging/CrashHandler.cpp
    DeviceFetcher.cpp
    DistroDetector.cpp
)

qt_add_resources(logitune-core "action-presets"
    PREFIX "/logitune"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/actions"
    FILES actions/actions.json
)

target_include_directories(logitune-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logitune-core PUBLIC Qt6::Core Qt6::DBus Qt6::Gui Qt6::Network ${UDEV_LIBRARIES})
if(TARGET LayerShellQt::Interface)
    target_compile_definitions(logitune-core PUBLIC LOGITUNE_LAYER_SHELL)
endif()
target_include_directories(logitune-core PRIVATE ${UDEV_INCLUDE_DIRS})
