#-------------------------------------------------
#
# Project created by QtCreator 2017-08-20T13:05:09
#
#-------------------------------------------------

QT       += core gui quick widgets quickwidgets qml serialport network svg printsupport multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = bin/NetworkPlannerGCS
TEMPLATE = app


INCLUDEPATH += src_qfi \
               src_qfi/example \
               dji_sdk/inc \
               QJoysticks/src \
               QJoysticks/lib/SDL/include \
               mavlink/mavlink/c_library_v2-master \
               /opt/homebrew/include
SOURCES += main.cpp\
    src_qfi/qfi_VSI.cpp \
    src_qfi/qfi_TC.cpp \
    src_qfi/qfi_PFD.cpp \
    src_qfi/qfi_NAV.cpp \
    src_qfi/qfi_HSI.cpp \
    src_qfi/qfi_ASI.cpp \
    src_qfi/qfi_ALT.cpp \
    src_qfi/qfi_ADI.cpp \
    flightinstrumentsimageprovider.cpp \
    djigcs.cpp \
    djigcs/QonboardSDK.cpp \
    mavlink/serialportreader.cpp \
    mavlink/qtsdljoystick.cpp \
    mavlink/mavlink_raw_message.cpp \
    joystickparameters.cpp \
    udpgcs.cpp \
    qcustomplot.cpp \
    qmlplot.cpp \
    modem_decode.cpp \
    file_writer.cpp \
    logging.cpp \
    mavlink/mav_gcs_manager.cpp \
    dji_sdk/src/DJI_API.cpp \
    dji_sdk/src/DJI_App.cpp \
    dji_sdk/src/DJI_Camera.cpp \
    dji_sdk/src/DJI_Codec.cpp \
    dji_sdk/src/DJI_Flight.cpp \
    dji_sdk/src/DJI_Follow.cpp \
    dji_sdk/src/DJI_HardDriver.cpp \
    dji_sdk/src/DJI_HotPoint.cpp \
    dji_sdk/src/DJI_Link.cpp \
    dji_sdk/src/DJI_Logging.cpp \
    dji_sdk/src/DJI_Memory.cpp \
    dji_sdk/src/DJI_Mission.cpp \
    dji_sdk/src/DJI_VirtualRC.cpp \
    dji_sdk/src/DJI_WayPoint.cpp

HEADERS  += \
    src_qfi/qfi_VSI.h \
    src_qfi/qfi_TC.h \
    src_qfi/qfi_PFD.h \
    src_qfi/qfi_NAV.h \
    src_qfi/qfi_HSI.h \
    src_qfi/qfi_ASI.h \
    src_qfi/qfi_ALT.h \
    src_qfi/qfi_ADI.h \
    flightinstrumentsimageprovider.h \
    djigcs.h \
    djigcs/QonboardSDK.h \
    mavlink/serialportreader.h \
    mavlink/qtsdljoystick.h \
    mavlink/mavlink_raw_message.h \
    joystickparameters.h \
    network_linker.h \
    udpgcs.h \
    qcustomplot.h \
    qmlplot.h \
    modem_decode.h \
    file_writer.h \
    logging.h \
    mavlink/mav_gcs_manager.h


RESOURCES += \
    src_qfi/qfi.qrc \
    icon.qrc \
    qml.qrc

DISTFILES += \
    Plot/qtquickcontrols2.conf

FORMS +=

LIBS +=  -L../bin/ \
    -lSDL2


include ($$PWD/QJoysticks/QJoysticks.pri)
