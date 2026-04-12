
#include <QApplication>
#include <QQuickView>
#include <QQuickWindow>
#include <QQmlEngine>
#include <QObject>
#include <QTimer>
#include <QtQuick/QSGRendererInterface>

#include "udpgcs.h"
#include "djigcs.h"
#include <QJoysticks.h>
#include <QStyleFactory>
#include <QQmlApplicationEngine>
#include <QtSerialPort/QSerialPort>
#include <QPalette>
#include <QQmlContext>
#include "mavlink/serialportreader.h"
#include "mavlink/mavlink_raw_message.h"
#include "joystickparameters.h"
#include <QStandardPaths>
#include <QStringLiteral>
#include <QByteArray>
#include "qmlplot.h"
#include "qcustomplot.h"
#include "modem_decode.h"
#include <QDir>
#include "logging.h"
#include "mavlink/mav_gcs_manager.h"
#include "weather_service.h"
#include "ns3_simulation_feed.h"
#include "video_stream_feed.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QCoreApplication::setOrganizationName("GCSNetworkPlanner");
    QCoreApplication::setApplicationName("NetworkPlannerGCS");

    QApplication a(argc, argv);
    QGuiApplication::setDesktopFileName(QStringLiteral("NetworkPlannerGCS"));

#ifdef QMAPLIBRE_PLUGIN_PATH
    QCoreApplication::addLibraryPath(QStringLiteral(QMAPLIBRE_PLUGIN_PATH));
#endif

    Mavlink_Raw_Message *mav_dec= new Mavlink_Raw_Message();
    SerialPortReader *serial=new SerialPortReader();
    JoystickParameters *joystick=new JoystickParameters();
    udpgcs * udp= new udpgcs();
    modem_decode * huawei= new modem_decode ();
    CustomPlotItem * plot = new CustomPlotItem();
    logging * log = new logging();
    QGimball *gimbal=new QGimball;
    WeatherService *weatherService = new WeatherService(&a);
    Ns3SimulationFeed *simulationFeed = new Ns3SimulationFeed(&a);
    VideoStreamFeed *videoStreamFeed = new VideoStreamFeed(&a);
    DJI::onboardSDK::DjiGcs dji;


    // loading QML view
    QQuickView w;
    QQmlEngine *engine = w.engine();
#ifdef QMAPLIBRE_QML_IMPORT_PATH
    engine->addImportPath(QStringLiteral(QMAPLIBRE_QML_IMPORT_PATH));
#endif
    const QString locationCachePath =
        QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/QtLocation";
    QDir(locationCachePath).removeRecursively();

    qmlRegisterType<CustomPlotItem>("CustomPlot", 1, 0, "CustomPlotItem");
    /*
     * QJoysticks is single instance, you can use the "getInstance()" function
     * directly if you want, or you can create a pointer to it to make code
     * easier to read;
     */
    QJoysticks* instance = QJoysticks::getInstance();
    const QString mapboxAccessToken = qEnvironmentVariable("MAPBOX_ACCESS_TOKEN");
    const QString mapboxStyleUrl = qEnvironmentVariable("MAPBOX_STYLE_URL");
    simulationFeed->startListening();
    videoStreamFeed->startListening();

    /* Enable the virtual joystick */
    instance->setVirtualJoystickRange (1);
    instance->setVirtualJoystickEnabled (true);
    /*
     * Register the QJoysticks with the QML engine, so that the QML interface
     * can easilly use it.
     */
    engine->rootContext()->setContextProperty("QJoysticks", instance);
    engine->rootContext()->setContextProperty("mavJoy", mav_dec);
    engine->rootContext()->setContextProperty("djiController", &dji);
    engine->rootContext()->setContextProperty("weatherService", weatherService);
    engine->rootContext()->setContextProperty("simulationFeed", simulationFeed);
    engine->rootContext()->setContextProperty("videoStreamFeed", videoStreamFeed);
    engine->rootContext()->setContextProperty("mapboxAccessToken", mapboxAccessToken);
    engine->rootContext()->setContextProperty("mapboxStyleUrl", mapboxStyleUrl);
    w.setSource(QUrl(QStringLiteral("qrc:/UavMapForm.qml")));
    //log->setFileName();




    w.setResizeMode(QQuickView::SizeRootObjectToView);

    // qml root object instance
    auto *item = qobject_cast<QQuickItem *>(w.rootObject());
    if (!item) {
        qCritical() << "Failed to load QML root item";
        return -1;
    }

    udp->bindHost();

    const auto activeVehicleBackend = [item]() -> QString {
        QVariant backendResult;
        if (QMetaObject::invokeMethod(item,
                                      "activeVehicleBackend",
                                      Q_RETURN_ARG(QVariant, backendResult)))
        {
            const QString backend = backendResult.toString().trimmed().toUpper();
            if (backend == QLatin1String("DJI"))
            {
                return backend;
            }
        }
        return QStringLiteral("MAVLINK");
    };

    //    //    //--------UDP__Connection----------//
    QObject::connect(udp,SIGNAL(onPilotChanged(QByteArray)),mav_dec,SLOT(dds_mavlink_decode(QByteArray)),Qt::DirectConnection);
    QObject::connect(mav_dec,SIGNAL(dds_mavlink_encodeSignal(QByteArray)),udp,SLOT(WritePilot(QByteArray)),Qt::DirectConnection);
    QObject::connect(udp,SIGNAL(onModemChanged(QByteArray)),huawei,SLOT(SignalAnalytics(QByteArray)),Qt::DirectConnection);

    QObject::connect(mav_dec,SIGNAL(plotParameters(QList<float>)),plot,SLOT(mavParameters(QList<float>)),Qt::DirectConnection);
    QObject::connect(mav_dec,SIGNAL(attitudeyaw(float)),plot,SLOT(uavParameters(float)),Qt::DirectConnection);
    QObject::connect(mav_dec,SIGNAL(attitudegauges(float,float)),plot,SLOT(uavParameters2(float,float)),Qt::DirectConnection);
    QObject::connect(huawei,SIGNAL(lteParameters(double,double,double,double)),plot,SLOT(modemLteParameters(double,double,double,double)),Qt::DirectConnection);

    //QObject::connect(&meteo,SIGNAL(sizeCities()),item,SIGNAL(meteoSize()));
    QObject::connect(item,SIGNAL(plotIndexChanged(QVariant)),plot,SLOT(treatementQMLPlot(QVariant)),Qt::DirectConnection);
    QObject::connect(item,SIGNAL(newDdsArgumentsDJI(QVariant)),&dji,SLOT(init(QVariant)));
    QObject::connect(&dji,SIGNAL(connectionStatus(QVariant)),item,SLOT(connectionStatusUpdate(QVariant)));
    QObject::connect(mav_dec,SIGNAL(signalCoordinate(QVariant,QVariant)),item,SLOT(updateUavGPS(QVariant,QVariant)));
    //--------Mavlink__Gauges----------//
    QObject::connect(mav_dec,SIGNAL(attitudeyaw(float)),log,SLOT(writeInTheFileYaw(float)));
    QObject::connect(mav_dec,SIGNAL(attitudepitch(float)),log,SLOT(writeInTheFilePitch(float)));
    QObject::connect(mav_dec,SIGNAL(attituderoll(float)),log,SLOT(writeInTheFileRoll(float)));
      QObject::connect(mav_dec,SIGNAL(gpsaltituderaw(double)),log,SLOT(writeInTheFileAltitude(double)));
    QObject::connect(mav_dec,SIGNAL(gpslatituderaw(double)),log,SLOT(writeInTheFileGPSLat(double)));
           QObject::connect(mav_dec,SIGNAL(gpslongtituderaw(double)),log,SLOT(writeInTheFileGPSLong(double)));
   //////////////////
    QObject::connect(mav_dec, &Mavlink_Raw_Message::attitudeyaw, item,
                     [item](float yawRadians) {
        QMetaObject::invokeMethod(item, "updateFlightHeading",
                                  Qt::DirectConnection,
                                  Q_ARG(QVariant, QVariant(yawRadians)));
    });
    QObject::connect(mav_dec, &Mavlink_Raw_Message::attitudepitch, item,
                     [item](float pitchRadians) {
        QMetaObject::invokeMethod(item, "updateFlightPitch",
                                  Qt::DirectConnection,
                                  Q_ARG(QVariant, QVariant(pitchRadians)));
    });
    QObject::connect(mav_dec, &Mavlink_Raw_Message::attituderoll, item,
                     [item](float rollRadians) {
        QMetaObject::invokeMethod(item, "updateFlightRoll",
                                  Qt::DirectConnection,
                                  Q_ARG(QVariant, QVariant(rollRadians)));
    });
    QObject::connect(mav_dec, &Mavlink_Raw_Message::gpsaltituderaw, item,
                     [item](double altitudeMeters) {
        QMetaObject::invokeMethod(item, "updateFlightAltitude",
                                  Qt::DirectConnection,
                                  Q_ARG(QVariant, QVariant(altitudeMeters)));
    });
    QObject::connect(mav_dec,SIGNAL(angleCoordinate(QVariant)),item,SLOT(angleRefresh(QVariant)));
    //--------Mavlink__Commands----------//


    //--------END----------//

    QObject::connect(item,SIGNAL(buttonVersionClicked()),&dji,SLOT(apiCoreDroneVersion()));
    QObject::connect(item,SIGNAL(activateDjiUav(QVariant)),&dji,SLOT(apiCoreActive(QVariant)));
    QObject::connect(item,SIGNAL(djiObatainControl(QVariant)),&dji,SLOT(apiCoreSetControl(QVariant)));

    QObject::connect(&dji,SIGNAL(appendDjiApiLogQML(QVariant)),item,SLOT(appendDjiApiLogQML(QVariant)));
    QObject::connect(&dji,SIGNAL(signalUpdateActivationButton(QVariant)),item,SLOT(updateActivateButton(QVariant)));
    QObject::connect(&dji,SIGNAL(signalUpdateObtainControlButton(QVariant)),item,SLOT(updateObtainControlButton(QVariant)));
    QObject::connect(mav_dec, &Mavlink_Raw_Message::uav_type, item,
                     [item](const QString &vehicleType) {
        QMetaObject::invokeMethod(item, "updateMavlinkVehicleType",
                                  Qt::DirectConnection,
                                  Q_ARG(QVariant, QVariant(vehicleType)));
    });
    QObject::connect(mav_dec, &Mavlink_Raw_Message::sys_status, item,
                     [item](const QString &systemStatus) {
        QMetaObject::invokeMethod(item, "updateMavlinkSystemStatus",
                                  Qt::DirectConnection,
                                  Q_ARG(QVariant, QVariant(systemStatus)));
    });
    QObject::connect(simulationFeed, &Ns3SimulationFeed::selectedUavChanged, mav_dec,
                     [simulationFeed, mav_dec]() {
        const int selectedId = simulationFeed->selectedUavId();
        mav_dec->setTargetSystemId(selectedId >= 0 ? selectedId + 1 : 1);
    });
    QObject::connect(simulationFeed,
                     &Ns3SimulationFeed::selectedUavChanged,
                     videoStreamFeed,
                     [simulationFeed, videoStreamFeed]() {
        videoStreamFeed->setSelectedUavId(simulationFeed->selectedUavId());
    });

    //-------------Latititude_longitude_Signals_to_mission------------//
    QObject::connect(item,SIGNAL(sendSignalLatitude(QVariant)),mav_dec,SLOT(get_QML_test(QVariant)));
    QObject::connect(item,SIGNAL(sendSignalLongitude(QVariant)),mav_dec,SLOT(get_QML_test_long(QVariant)));

    //---------Joystick_Connections_____________//
    QObject::connect(item,SIGNAL(joystickParameters(double,double)),joystick,SLOT(get_axis_qml(double,double)));
    QObject::connect(item,SIGNAL(joystickValue(double,double)),joystick,SLOT(get_axis_value_qml(double,double)));
    QObject::connect(item,SIGNAL(joystickProtocolChanged(int)),joystick,SLOT(get_joystick_protocol_qml(int)));
    QObject::connect(joystick,
                     &JoystickParameters::qmljoystickcontrols,
                     &a,
                     [item, &dji, mav_dec, activeVehicleBackend](int roll,
                                                                int pitch,
                                                                int yaw,
                                                                int throttle) {
        if (!item->property("manualControlActive").toBool())
        {
            return;
        }

        if (activeVehicleBackend() == QLatin1String("DJI"))
        {
            dji.sendVirtualRcCommand(roll, pitch, yaw, throttle);
            return;
        }

        mav_dec->ch3_joystick(roll, pitch, yaw, throttle);
    });
    //---------Mission_Modes_____________//
    QObject::connect(item,SIGNAL(sendSignalFlightModes(int,int)),mav_dec,SLOT(setMode(int,int)));
    QObject::connect(mav_dec,SIGNAL(sendMissionSetResult(QVariant)),item,SLOT(resultFlightMode(QVariant)));
    //    QObject::connect(item,SIGNAL(missionResultSignal(int)),mav_dec,SLOT(setIndexMode(int)));
    //     QObject::connect(mav_dec,SIGNAL(indexModeAccepted(QVariant)),item,SLOT(resultIndex(QVariant)));
    //---------CELLULAR_NETWORK_PROCESSING_____________//
    QObject::connect(huawei,SIGNAL(sendNetworkGraphicTypes(QVariant,QVariant)),item,SIGNAL(sendNetworkIcons(QVariant,QVariant)));
    QObject::connect(huawei,SIGNAL(guiLTEparameters(QVariant,QVariant,QVariant,QVariant)),item,SIGNAL(guiQMLLTEParameters(QVariant,QVariant,QVariant,QVariant)));
    QObject::connect(huawei,SIGNAL(guiWCDMAparameters(QVariant,QVariant,QVariant)),item,SIGNAL(guiQMLWCDMAParameters(QVariant,QVariant,QVariant)));
    QObject::connect(huawei,SIGNAL(guiGSMparameters(QVariant)),item,SIGNAL(guiQMLGSMParameters(QVariant)));
    QObject::connect(huawei,SIGNAL(guiRangeparameters(QVariant,QVariant,QVariant,QVariant)),item,SIGNAL(guiQMLRangeParameters(QVariant,QVariant,QVariant,QVariant)));
    QObject::connect(item,SIGNAL(setTypeNetworkIndex(QVariant)),huawei,SLOT(receiveModemQML(QVariant)));
    QObject::connect(huawei,SIGNAL(sendModemCommands(QByteArray)),udp,SLOT(WriteModem(QByteArray)));
    //---------GIMBAL_PROCESSING_____________//
    QObject::connect(instance,SIGNAL(PovSend(QByteArray)),udp,SLOT(WriteGimbal(QByteArray)));
    QObject::connect(mav_dec,SIGNAL( qmlBatteryInfoSignal(QVariant,QVariant,QVariant)),item,SLOT(getBatteryData(QVariant,QVariant,QVariant)),Qt::DirectConnection);
    dji.startUpdateFlightInstruments(41);

    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    qDebug() << cachePath;



    //qmlEngine.rootContext()->setContextProperty ("QJoysticks", instance);



    /*
     * Load main.qml and run the application.
     */
    //qmlEngine.load (QUrl (QStringLiteral ("qrc:/joystick.qml")));

    w.show();

    // QObject *rect = item->findChild<QObject*>("windowJoystick");



    return a.exec();
}
