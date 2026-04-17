cmake_minimum_required(VERSION 3.21)

project(NetworkPlannerGCS LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(QT_REQUIRED_COMPONENTS
    Core
    Gui
    Widgets
    Quick
    QuickWidgets
    Qml
    Network
    Sql
    SerialPort
    Svg
    SvgWidgets
    PrintSupport
    Positioning
    Location
)

find_package(Qt6 REQUIRED COMPONENTS ${QT_REQUIRED_COMPONENTS})
set(QT_VERSION_MAJOR 6)

if(COMMAND qt_standard_project_setup)
    qt_standard_project_setup()
endif()

get_target_property(QT6_CORE_LIBRARY Qt6::Core LOCATION)
get_filename_component(QT6_LIBRARY_DIR "${QT6_CORE_LIBRARY}" DIRECTORY)

set(MAPLIBRE_INSTALL_PREFIX "" CACHE PATH "Install prefix containing the QMapLibre CMake package")
if(NOT MAPLIBRE_INSTALL_PREFIX)
    set(MAPLIBRE_INSTALL_PREFIX "$ENV{MAPLIBRE_INSTALL_PREFIX}")
endif()
if(NOT MAPLIBRE_INSTALL_PREFIX)
    set(MAPLIBRE_INSTALL_PREFIX "$ENV{QMAPLIBRE_INSTALL_PREFIX}")
endif()
if(NOT MAPLIBRE_INSTALL_PREFIX)
    set(MAPLIBRE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/.deps/maplibre-native-qt/install")
endif()

find_package(QMapLibre REQUIRED COMPONENTS Location HINTS "${MAPLIBRE_INSTALL_PREFIX}")

find_package(PkgConfig REQUIRED)
pkg_search_module(SDL2 REQUIRED IMPORTED_TARGET sdl2 SDL2)
set(MAVLINK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/mavlink/generated")

set(PROJECT_SOURCES
    src/app/main.cpp
    src/core/file_writer.cpp
    src/core/logging.cpp
    src/core/ns3_simulation_feed.cpp
    src/core/publication_logger.cpp
    src/core/video_stream_feed.cpp
    src/dji/djigcs.cpp
    src/dji/QonboardSDK.cpp
    src/mavlink/mav_gcs_manager.cpp
    src/mavlink/mavlink_raw_message.cpp
    src/mavlink/qtsdljoystick.cpp
    src/mavlink/serialportreader.cpp
    src/network/modem_decode.cpp
    src/network/udpgcs.cpp
    src/plot/qmlplot.cpp
    src/ui/joystickparameters.cpp
    src/weather/weather_service.cpp
    third_party/dji_sdk/src/DJI_API.cpp
    third_party/dji_sdk/src/DJI_App.cpp
    third_party/dji_sdk/src/DJI_Camera.cpp
    third_party/dji_sdk/src/DJI_Codec.cpp
    third_party/dji_sdk/src/DJI_Flight.cpp
    third_party/dji_sdk/src/DJI_Follow.cpp
    third_party/dji_sdk/src/DJI_HardDriver.cpp
    third_party/dji_sdk/src/DJI_HotPoint.cpp
    third_party/dji_sdk/src/DJI_Link.cpp
    third_party/dji_sdk/src/DJI_Logging.cpp
    third_party/dji_sdk/src/DJI_Memory.cpp
    third_party/dji_sdk/src/DJI_Mission.cpp
    third_party/dji_sdk/src/DJI_VirtualRC.cpp
    third_party/dji_sdk/src/DJI_WayPoint.cpp
    third_party/qcustomplot/qcustomplot.cpp
    third_party/qfi/qfi_ADI.cpp
    third_party/qfi/qfi_ALT.cpp
    third_party/qfi/qfi_ASI.cpp
    third_party/qfi/qfi_HSI.cpp
    third_party/qfi/qfi_NAV.cpp
    third_party/qfi/qfi_PFD.cpp
    third_party/qfi/qfi_TC.cpp
    third_party/qfi/qfi_VSI.cpp
    third_party/QJoysticks/src/QJoysticks.cpp
    third_party/QJoysticks/src/QJoysticks/SDL_Joysticks.cpp
    third_party/QJoysticks/src/QJoysticks/VirtualJoystick.cpp
    third_party/QJoysticks/src/QJoysticks/Android_Joystick.cpp
    third_party/qfi/qfi.qrc
    third_party/QJoysticks/etc/resources/qjoysticks-res.qrc
    resources/assets.qrc
    resources/qml.qrc
)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

get_target_property(QMAPLIBRE_IMPORTED_CONFIGS QMapLibre::PluginGeoServices IMPORTED_CONFIGURATIONS)
if(CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" QMAPLIBRE_BUILD_CONFIGURATION)
endif()
if(NOT QMAPLIBRE_BUILD_CONFIGURATION)
    list(GET QMAPLIBRE_IMPORTED_CONFIGS 0 QMAPLIBRE_IMPORTED_CONFIGURATION)
elseif(QMAPLIBRE_BUILD_CONFIGURATION IN_LIST QMAPLIBRE_IMPORTED_CONFIGS)
    set(QMAPLIBRE_IMPORTED_CONFIGURATION "${QMAPLIBRE_BUILD_CONFIGURATION}")
else()
    list(GET QMAPLIBRE_IMPORTED_CONFIGS 0 QMAPLIBRE_IMPORTED_CONFIGURATION)
endif()

get_target_property(QMAPLIBRE_GEOSERVICES_PLUGIN
    QMapLibre::PluginGeoServices
    "IMPORTED_LOCATION_${QMAPLIBRE_IMPORTED_CONFIGURATION}")
get_filename_component(QMAPLIBRE_PLUGIN_DIR "${QMAPLIBRE_GEOSERVICES_PLUGIN}" DIRECTORY)
get_filename_component(QMAPLIBRE_PLUGIN_DIR "${QMAPLIBRE_PLUGIN_DIR}" DIRECTORY)

if(TARGET QMapLibre::PluginQmlLocation)
    set(QMAPLIBRE_QML_PLUGIN_TARGET QMapLibre::PluginQmlLocation)
elseif(TARGET QMapLibre::PluginQml)
    set(QMAPLIBRE_QML_PLUGIN_TARGET QMapLibre::PluginQml)
else()
    message(FATAL_ERROR "QMapLibre QML plugin target was not found in the installed package.")
endif()

get_target_property(QMAPLIBRE_QML_PLUGIN
    ${QMAPLIBRE_QML_PLUGIN_TARGET}
    "IMPORTED_LOCATION_${QMAPLIBRE_IMPORTED_CONFIGURATION}")
get_filename_component(QMAPLIBRE_QML_IMPORT_PATH "${QMAPLIBRE_QML_PLUGIN}" DIRECTORY)
get_filename_component(QMAPLIBRE_QML_IMPORT_PATH "${QMAPLIBRE_QML_IMPORT_PATH}" DIRECTORY)
if(QMAPLIBRE_QML_PLUGIN_TARGET STREQUAL "QMapLibre::PluginQmlLocation")
    get_filename_component(QMAPLIBRE_QML_IMPORT_PATH "${QMAPLIBRE_QML_IMPORT_PATH}" DIRECTORY)
endif()

get_target_property(QMAPLIBRE_CORE_LIBRARY QMapLibre::Core LOCATION)
get_filename_component(QMAPLIBRE_LIBRARY_DIR "${QMAPLIBRE_CORE_LIBRARY}" DIRECTORY)

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME NetworkPlannerGCS
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    BUILD_RPATH "${QT6_LIBRARY_DIR};${QMAPLIBRE_LIBRARY_DIR}"
)

if(UNIX AND NOT APPLE)
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--disable-new-dtags")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/dji
    ${CMAKE_SOURCE_DIR}/src/mavlink
    ${CMAKE_SOURCE_DIR}/src/network
    ${CMAKE_SOURCE_DIR}/src/plot
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/weather
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/dji_sdk/inc
    ${MAVLINK_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/qfi
    ${CMAKE_SOURCE_DIR}/third_party/qcustomplot
    ${CMAKE_SOURCE_DIR}/third_party/QJoysticks/src
)

if(NOT ANDROID)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_SUPPORTED)
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SDL_MAIN_HANDLED SDL_WIN)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    QMAPLIBRE_QML_IMPORT_PATH="${QMAPLIBRE_QML_IMPORT_PATH}"
    QMAPLIBRE_PLUGIN_PATH="${QMAPLIBRE_PLUGIN_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickWidgets
    Qt${QT_VERSION_MAJOR}::Qml
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::SerialPort
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::SvgWidgets
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Positioning
    Qt${QT_VERSION_MAJOR}::Location
    QMapLibre::Location
    PkgConfig::SDL2
)

qmaplibre_location_setup_plugins(${PROJECT_NAME})
