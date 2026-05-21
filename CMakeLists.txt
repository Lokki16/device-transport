cmake_minimum_required(VERSION 3.20)

project(device_transport VERSION 0.1.0 LANGUAGES CXX)

option(DEVICE_TRANSPORT_BUILD_QT_ADAPTER "Build the optional Qt adapter" OFF)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

if(WIN32)
    set(device_transport_SERIAL_SOURCE src/serial_port/windows/serial_port.cpp)
else()
    set(device_transport_SERIAL_SOURCE src/serial_port/unix/serial_port.cpp)
endif()

add_library(device_transport_core INTERFACE)
add_library(device_transport::core ALIAS device_transport_core)

target_compile_features(device_transport_core
    INTERFACE
        cxx_std_11
)

target_include_directories(device_transport_core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(device_transport_core
    PROPERTIES
        EXPORT_NAME core
)

add_library(device_transport_embedded INTERFACE)
add_library(device_transport::embedded ALIAS device_transport_embedded)

target_link_libraries(device_transport_embedded
    INTERFACE
        device_transport_core
)

set_target_properties(device_transport_embedded
    PROPERTIES
        EXPORT_NAME embedded
)

add_library(device_transport_serial
    ${device_transport_SERIAL_SOURCE}
)

add_library(device_transport::serial ALIAS device_transport_serial)

target_compile_features(device_transport_serial
    PUBLIC
        cxx_std_11
)

target_link_libraries(device_transport_serial
    PUBLIC
        device_transport_core
)

set_target_properties(device_transport_serial
    PROPERTIES
        EXPORT_NAME serial
)

add_library(device_transport_desktop
    src/xbee/desktop/xbee.cpp
)

add_library(device_transport::desktop ALIAS device_transport_desktop)

target_compile_features(device_transport_desktop
    PUBLIC
        cxx_std_11
)

target_link_libraries(device_transport_desktop
    PUBLIC
        device_transport_core
        device_transport_serial
)

set_target_properties(device_transport_desktop
    PROPERTIES
        EXPORT_NAME desktop
)

if(UNIX AND NOT APPLE)
    target_link_libraries(device_transport_serial
        PUBLIC
            pthread
    )
endif()

add_library(device_transport INTERFACE)
add_library(device_transport::device_transport ALIAS device_transport)

target_link_libraries(device_transport
    INTERFACE
        device_transport_desktop
)

set_target_properties(device_transport
    PROPERTIES
        EXPORT_NAME device_transport
)

if(DEVICE_TRANSPORT_BUILD_QT_ADAPTER)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core SerialPort)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core SerialPort)

    add_library(device_transport_qt
        src/xbee/qt/qxbee.cpp
    )

    add_library(device_transport::qt ALIAS device_transport_qt)

    target_link_libraries(device_transport_qt
        PUBLIC
            device_transport_core
            Qt${QT_VERSION_MAJOR}::Core
            Qt${QT_VERSION_MAJOR}::SerialPort
    )

    target_compile_features(device_transport_qt
        PUBLIC
            cxx_std_11
    )

    set_target_properties(device_transport_qt
        PROPERTIES
            EXPORT_NAME qt
    )

    install(TARGETS device_transport_qt
        EXPORT device_transportTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

endif()

install(TARGETS
    device_transport
    device_transport_core
    device_transport_embedded
    device_transport_serial
    device_transport_desktop
    EXPORT device_transportTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT device_transportTargets
    NAMESPACE device_transport::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/device_transport
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/device_transportConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/device_transportConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/device_transportConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/device_transport
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/device_transportConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/device_transportConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/device_transport
)
