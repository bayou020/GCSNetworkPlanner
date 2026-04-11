#include "weather_service.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QGeoCoordinate>
#include <QSet>

namespace {

QString coordinateKey(double latitude, double longitude)
{
    return QString::number(latitude, 'f', 3) + QLatin1Char(',') + QString::number(longitude, 'f', 3);
}

QString legacyWeatherTileLayer(QString layer)
{
    layer = layer.trimmed();
    if (layer.isEmpty()) {
        return QStringLiteral("temp_new");
    }

    const QString normalized = layer.toUpper();
    if (normalized == QStringLiteral("WND") || normalized == QStringLiteral("WS10")) {
        return QStringLiteral("wind_new");
    }
    if (normalized == QStringLiteral("TA2") || normalized == QStringLiteral("TEMP")) {
        return QStringLiteral("temp_new");
    }
    if (normalized == QStringLiteral("APM") || normalized == QStringLiteral("PRESSURE")) {
        return QStringLiteral("pressure_new");
    }
    if (normalized == QStringLiteral("CL") || normalized == QStringLiteral("CLOUDS")) {
        return QStringLiteral("clouds_new");
    }
    if (normalized == QStringLiteral("PA0") || normalized == QStringLiteral("PR0")
            || normalized == QStringLiteral("PRECIPITATION")) {
        return QStringLiteral("precipitation_new");
    }
    if (layer.endsWith(QStringLiteral("_new"), Qt::CaseInsensitive)) {
        return layer;
    }

    return QStringLiteral("temp_new");
}

}

WeatherService::WeatherService(QObject *parent)
    : QObject(parent)
    , m_apiKey(qEnvironmentVariable("OPENWEATHERMAP_API_KEY"))
{
    if (m_apiKey.isEmpty()) {
        m_apiKey = qEnvironmentVariable("OPENWEATHER_API_KEY");
    }

    connect(&m_networkManager, &QNetworkAccessManager::finished,
            this, &WeatherService::handleReply);
}

bool WeatherService::loading() const { return m_loading; }
bool WeatherService::hasData() const { return m_hasData; }
bool WeatherService::apiKeyPresent() const { return !m_apiKey.isEmpty(); }
QVariantList WeatherService::markers() const { return m_markers; }
QString WeatherService::errorString() const { return m_errorString; }
QString WeatherService::weatherTileTemplate() const
{
    if (m_apiKey.isEmpty()) {
        return QString();
    }

    const QString requestedLayer = qEnvironmentVariableIsSet("OPENWEATHERMAP_TILE_LAYER")
        ? qEnvironmentVariable("OPENWEATHERMAP_TILE_LAYER")
        : QStringLiteral("TA2");
    const QString weatherLayer = legacyWeatherTileLayer(requestedLayer);
    return QStringLiteral("https://tile.openweathermap.org/map/%1/{z}/{x}/{y}.png?appid=%2")
        .arg(weatherLayer, m_apiKey);
}

void WeatherService::fetchWeather(double latitude, double longitude)
{
    QVariantMap point;
    point.insert(QStringLiteral("latitude"), latitude);
    point.insert(QStringLiteral("longitude"), longitude);
    fetchWeatherPoints(QVariantList{point});
}

void WeatherService::fetchWeatherRadius(double latitude, double longitude, double radiusKm)
{
    const QGeoCoordinate center(latitude, longitude);
    const double radiusMeters = radiusKm * 1000.0;
    const QList<QGeoCoordinate> coordinates = {
        center,
        center.atDistanceAndAzimuth(radiusMeters, 0.0),
        center.atDistanceAndAzimuth(radiusMeters, 45.0),
        center.atDistanceAndAzimuth(radiusMeters, 90.0),
        center.atDistanceAndAzimuth(radiusMeters, 135.0),
        center.atDistanceAndAzimuth(radiusMeters, 180.0),
        center.atDistanceAndAzimuth(radiusMeters, 225.0),
        center.atDistanceAndAzimuth(radiusMeters, 270.0),
        center.atDistanceAndAzimuth(radiusMeters, 315.0)
    };

    QVariantList points;
    for (const QGeoCoordinate &coordinate : coordinates) {
        QVariantMap point;
        point.insert(QStringLiteral("latitude"), coordinate.latitude());
        point.insert(QStringLiteral("longitude"), coordinate.longitude());
        points.append(point);
    }

    fetchWeatherPoints(points);
}

void WeatherService::fetchWeatherPoints(const QVariantList &points)
{
    if (m_apiKey.isEmpty()) {
        clearWeatherData();
        setHasData(false);
        setErrorString(QStringLiteral("Set OPENWEATHERMAP_API_KEY to enable weather."));
        return;
    }

    clearWeatherData();
    setHasData(false);
    setLoading(true);
    setErrorString(QString());

    ++m_batchId;
    m_pendingReplies = 0;

    QSet<QString> seenCoordinates;
    int queuedPoints = 0;

    for (int index = 0; index < points.size(); ++index) {
        const QVariantMap point = points.at(index).toMap();
        const double latitude = point.value(QStringLiteral("latitude")).toDouble();
        const double longitude = point.value(QStringLiteral("longitude")).toDouble();
        const QString key = coordinateKey(latitude, longitude);
        if (seenCoordinates.contains(key)) {
            continue;
        }
        seenCoordinates.insert(key);

        if (queuedPoints >= 9) {
            break;
        }

        QUrl url(QStringLiteral("https://api.openweathermap.org/data/2.5/weather"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("lat"), QString::number(latitude, 'f', 6));
        query.addQueryItem(QStringLiteral("lon"), QString::number(longitude, 'f', 6));
        query.addQueryItem(QStringLiteral("units"), QStringLiteral("metric"));
        query.addQueryItem(QStringLiteral("appid"), m_apiKey);
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        QNetworkReply *reply = m_networkManager.get(request);
        reply->setProperty("batchId", m_batchId);
        reply->setProperty("markerIndex", queuedPoints);
        reply->setProperty("primary", point.value(QStringLiteral("primary")).toBool());
        ++m_pendingReplies;
        ++queuedPoints;
    }

    if (queuedPoints == 0) {
        setLoading(false);
        setErrorString(QStringLiteral("No valid weather points to fetch."));
    }
}

void WeatherService::clear()
{
    clearWeatherData();
    setHasData(false);
    setLoading(false);
    setErrorString(QString());
}

void WeatherService::handleReply(QNetworkReply *reply)
{
    const int replyBatchId = reply->property("batchId").toInt();
    const QByteArray body = reply->readAll();
    if (replyBatchId != m_batchId) {
        reply->deleteLater();
        return;
    }

    --m_pendingReplies;
    const bool lastReply = (m_pendingReplies <= 0);

    if (reply->error() != QNetworkReply::NoError) {
        if (m_markers.isEmpty()) {
            setErrorString(reply->errorString());
        }
        reply->deleteLater();
        if (lastReply) {
            setLoading(false);
        }
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(body);
    if (!document.isObject()) {
        reply->deleteLater();
        if (m_markers.isEmpty()) {
            setErrorString(QStringLiteral("Weather service returned invalid JSON."));
        }
        if (lastReply) {
            setLoading(false);
        }
        return;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("cod")).toInt() >= 400) {
        reply->deleteLater();
        if (m_markers.isEmpty()) {
            setErrorString(root.value(QStringLiteral("message")).toString(QStringLiteral("Weather request failed.")));
        }
        if (lastReply) {
            setLoading(false);
        }
        return;
    }

    const QJsonObject coord = root.value(QStringLiteral("coord")).toObject();
    const QJsonObject main = root.value(QStringLiteral("main")).toObject();
    const QJsonArray weather = root.value(QStringLiteral("weather")).toArray();
    const QJsonObject firstWeather = weather.isEmpty() ? QJsonObject() : weather.first().toObject();

    const QString iconCode = firstWeather.value(QStringLiteral("icon")).toString();
    QVariantMap marker;
    marker.insert(QStringLiteral("latitude"), coord.value(QStringLiteral("lat")).toDouble());
    marker.insert(QStringLiteral("longitude"), coord.value(QStringLiteral("lon")).toDouble());
    marker.insert(QStringLiteral("locationName"), root.value(QStringLiteral("name")).toString());
    marker.insert(QStringLiteral("description"), firstWeather.value(QStringLiteral("description")).toString());
    marker.insert(QStringLiteral("temperatureC"), main.value(QStringLiteral("temp")).toDouble());
    marker.insert(QStringLiteral("tempLabel"),
                  QString::number(main.value(QStringLiteral("temp")).toDouble(), 'f', 0) + QStringLiteral(" C"));
    marker.insert(QStringLiteral("primary"), reply->property("primary").toBool());
    marker.insert(QStringLiteral("iconUrl"),
                  iconCode.isEmpty()
                      ? QString()
                      : QStringLiteral("https://openweathermap.org/img/wn/%1@2x.png").arg(iconCode));
    appendMarker(marker);
    setHasData(!m_markers.isEmpty());
    if (!m_markers.isEmpty()) {
        setErrorString(QString());
    }

    reply->deleteLater();
    if (lastReply) {
        setLoading(false);
    }
}

void WeatherService::setLoading(bool value)
{
    if (m_loading == value)
        return;
    m_loading = value;
    emit loadingChanged();
}

void WeatherService::setHasData(bool value)
{
    if (m_hasData == value)
        return;
    m_hasData = value;
    emit hasDataChanged();
}

void WeatherService::setErrorString(const QString &value)
{
    if (m_errorString == value)
        return;
    m_errorString = value;
    emit errorChanged();
}

void WeatherService::clearWeatherData()
{
    if (m_markers.isEmpty()) {
        return;
    }
    m_markers.clear();
    emit markersChanged();
}

void WeatherService::appendMarker(const QVariantMap &marker)
{
    m_markers.append(marker);
    emit markersChanged();
}
