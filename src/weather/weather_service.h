#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QVariantList>

class QNetworkReply;

class WeatherService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY hasDataChanged)
    Q_PROPERTY(bool apiKeyPresent READ apiKeyPresent NOTIFY apiKeyPresentChanged)
    Q_PROPERTY(QVariantList markers READ markers NOTIFY markersChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QString weatherTileTemplate READ weatherTileTemplate NOTIFY apiKeyPresentChanged)

public:
    explicit WeatherService(QObject *parent = nullptr);

    bool loading() const;
    bool hasData() const;
    bool apiKeyPresent() const;
    QVariantList markers() const;
    QString errorString() const;
    QString weatherTileTemplate() const;

    Q_INVOKABLE void fetchWeather(double latitude, double longitude);
    Q_INVOKABLE void fetchWeatherRadius(double latitude, double longitude, double radiusKm);
    Q_INVOKABLE void fetchWeatherPoints(const QVariantList &points);
    Q_INVOKABLE void clear();

signals:
    void loadingChanged();
    void hasDataChanged();
    void apiKeyPresentChanged();
    void markersChanged();
    void errorChanged();

private slots:
    void handleReply(QNetworkReply *reply);

private:
    void setLoading(bool value);
    void setHasData(bool value);
    void setErrorString(const QString &value);
    void clearWeatherData();
    void appendMarker(const QVariantMap &marker);

    QNetworkAccessManager m_networkManager;
    QString m_apiKey;
    bool m_loading = false;
    bool m_hasData = false;
    QVariantList m_markers;
    QString m_errorString;
    int m_batchId = 0;
    int m_pendingReplies = 0;
};

#endif
