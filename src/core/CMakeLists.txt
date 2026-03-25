add_library(logitune-core STATIC)

target_sources(logitune-core PRIVATE
    hidpp/HidppTypes.cpp
    hidpp/HidrawDevice.cpp
    hidpp/Transport.cpp
    hidpp/FeatureDispatcher.cpp
    hidpp/features/SmartShift.cpp
    hidpp/features/HiResWheel.cpp
    hidpp/features/ThumbWheel.cpp
    hidpp/features/ReprogControls.cpp
    hidpp/features/GestureV2.cpp
    hidpp/features/Battery.cpp
    hidpp/features/AdjustableDPI.cpp
    hidpp/features/DeviceName.cpp
)

target_include_directories(logitune-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logitune-core PUBLIC Qt6::Core Qt6::DBus ${UDEV_LIBRARIES})
target_include_directories(logitune-core PRIVATE ${UDEV_INCLUDE_DIRS})
