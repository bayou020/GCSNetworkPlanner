#include "logging.h"

#include "publication_logger.h"

logging::logging(QObject *parent) : QObject(parent)
{
    timeS = QDateTime::currentDateTime().toMSecsSinceEpoch();
    timeM = time.currentDateTime().toString("yyyy-MM-dd_hh:mm:ss,zzz");
    timeL = timeM;
    timeN = 0;
    timeN2 = 0;
    timer.start();
    initializeLogFiles();
}

void logging::initializeLogFiles()
{
    setFileNamePitch();
    setFileNameYaw();
    setFileNameRoll();
    setFileNameAltitude();
    setFileNameGPSLat();
    setFileNameGPSLong();
}

QString logging::createLogFilePath(const QString &prefix)
{
    return prefix;
}

void logging::setFileNamePitch()
{
    filenameP = createLogFilePath("pitch");
    internFileP = filenameP;
}

void logging::setFileNameYaw()
{
    filenameY = createLogFilePath("yaw");
    internFileY = filenameY;
}

void logging::setFileNameRoll()
{
    filenameR = createLogFilePath("roll");
    internFileR = filenameR;
}


void logging::setFileNameAltitude()
{
    filenameALT = createLogFilePath("altitude");
    internFileALT = filenameALT;
}

void logging::setFileNameGPSLat()
{
    filenameGPSLat = createLogFilePath("gps_lat");
    internFileGPSLa = filenameGPSLat;
}

void logging::setFileNameGPSLong()
{
    filenameGPSLong = createLogFilePath("gps_long");
    internFileGPSLong = filenameGPSLong;
}

void logging::appendMetricSample(const QString &path, double data)
{
    if (path.isEmpty()) {
        return;
    }

    if (!timer.isValid()) {
        timer.start();
    }

    timeN2 = timeN;
    timeN = timer.elapsed();

    PublicationLogger::instance().logEvent(
        QStringLiteral("telemetry_rx"),
        {{QStringLiteral("metric_name"), path},
         {QStringLiteral("value"), data},
         {QStringLiteral("evidence_layer"), QStringLiteral("ui_visualization_only")},
         {QStringLiteral("note"), QStringLiteral("legacy_metric_slot")},
         {QStringLiteral("status"), QStringLiteral("received")}});
}

////////////////////////////////////////////////////////////////////////////////////////
void logging::writeInTheFileYaw(float data)
{
    appendMetricSample(internFileY, data);
}
void logging::writeInTheFileRoll(float data)
{
    appendMetricSample(internFileR, data);
}
void logging::writeInTheFilePitch(float data)
{
    appendMetricSample(internFileP, data);
}
void logging::writeInTheFileAltitude(double data)
{
    appendMetricSample(internFileALT, data);
}
void logging::writeInTheFileGPSLat(double data)
{
    appendMetricSample(internFileGPSLa, data);
}
void logging::writeInTheFileGPSLong(double data)
{
    appendMetricSample(internFileGPSLong, data);
}
