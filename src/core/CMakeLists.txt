add_library(logitune-core STATIC)

target_sources(logitune-core PRIVATE
    DeviceManager.cpp
    hidpp/HidppTypes.cpp
    hidpp/HidrawDevice.cpp
    hidpp/Transport.cpp
    hidpp/FeatureDispatcher.cpp
    hidpp/CommandQueue.cpp
    hidpp/features/SmartShift.cpp
    hidpp/features/HiResWheel.cpp
    hidpp/features/ThumbWheel.cpp
    hidpp/features/ReprogControls.cpp
    hidpp/features/GestureV2.cpp
    hidpp/features/Battery.cpp
    hidpp/features/AdjustableDPI.cpp
    hidpp/features/DeviceName.cpp
    hidpp/capabilities/BatteryCapability.cpp
    hidpp/capabilities/SmartShiftCapability.cpp
    ProfileEngine.cpp
    ActionExecutor.cpp
    DeviceRegistry.cpp
    devices/JsonDevice.cpp
    interfaces/IDesktopIntegration.cpp
    interfaces/IInputInjector.cpp
    interfaces/ITransport.cpp
    desktop/LinuxDesktopBase.cpp
    desktop/GnomeDesktop.cpp
    desktop/KDeDesktop.cpp
    desktop/GenericDesktop.cpp
    input/UinputInjector.cpp
    logging/LogManager.cpp
    logging/CrashHandler.cpp
)

target_include_directories(logitune-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logitune-core PUBLIC Qt6::Core Qt6::DBus ${UDEV_LIBRARIES})
target_include_directories(logitune-core PRIVATE ${UDEV_INCLUDE_DIRS})
