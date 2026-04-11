import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: networkBanner

    property variant networkType: 0
    property variant networkQuality: 0
    property var metricQualities: []
    property int metricCount: parametersModel.count
    property int rowHeight: 28
    property int preferredPanelHeight: Math.max(560, 36 + headerRow.implicitHeight + parametersView.contentHeight + 24)
    signal setTypeNetworkIndex(var index)

    width: 420
    implicitWidth: 420
    implicitHeight: preferredPanelHeight
    clip: true

    function qualityColor(quality) {
        switch (quality) {
        case "EXCELLENT":
            return "#3CFE6E"
        case "GOOD":
            return "#FFFF33"
        case "FAIR":
            return "#E8612C"
        case "POOR":
            return "#ED192D"
        default:
            return "#E94B3C"
        }
    }

    function networkIconForType(type) {
        switch (type) {
        case "MAVLINK":
            return "qrc:/ico/dr1.png"
        case "NR":
            return "qrc:/ico/4g.png"
        case "LTE":
            return "qrc:/ico/4g.png"
        case "WCDMA":
            return "qrc:/ico/3g.png"
        case "GSM":
            return "qrc:/ico/2g.png"
        default:
            return "qrc:/ico/nosignal.png"
        }
    }

    function qualityIconForLevel(quality) {
        switch (quality) {
        case "EXCELLENT":
            return "qrc:/ico/5bars.png"
        case "GOOD":
            return "qrc:/ico/4bars.png"
        case "FAIR":
            return "qrc:/ico/3bars.png"
        case "POOR":
            return "qrc:/ico/2bars.png"
        default:
            return "qrc:/ico/nosignal.png"
        }
    }

    Rectangle {
        id: networkRectangle
        anchors.fill: parent
        border.color: "black"
        border.width: 2
        color: "#D69C2F"
        opacity: 0.7
        radius: 17

        ListModel {
            id: parametersModel
        }

        ListModel {
            id: changeTypeNetwork
            ListElement {
                name: "AUTO"
                value: "AT^SYSCFGEX=\"00\",3FFFFFFF,1,2,7FFFFFFFFFFFFFFF,,\r\n"
            }
            ListElement {
                name: "ONLY LTE"
                value: "AT^SYSCFGEX=\"03\",3FFFFFFF,1,2,7FFFFFFFFFFFFFFF,,\r\n"
            }
            ListElement {
                name: "ONLY WCDMA"
                value: "AT^SYSCFGEX=\"02\",3FFFFFFF,1,2,7FFFFFFFFFFFFFFF,,\r\n"
            }
            ListElement {
                name: "ONLY GSM"
                value: "AT^SYSCFGEX=\"01\",3FFFFFFF,1,2,7FFFFFFFFFFFFFFF,,\r\n"
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                id: headerRow
                Layout.fillWidth: true
                spacing: 8

                Image {
                    id: networkTypeImage
                    sourceSize.width: 45
                    sourceSize.height: 45
                    source: networkIconForType(networkType)
                }

                Image {
                    id: networkQualityImage
                    sourceSize.width: 45
                    sourceSize.height: 45
                    source: qualityIconForLevel(networkQuality)
                }

                Item {
                    Layout.fillWidth: true
                }

                ComboBox {
                    id: modesList
                    Layout.preferredWidth: 152
                    Layout.preferredHeight: 32
                    model: changeTypeNetwork
                    textRole: "name"

                    background: Rectangle {
                        radius: 17
                        border.color: "black"
                        border.width: 2
                        color: "#00539C"
                    }

                    onCurrentIndexChanged: {
                        getTypeNetworkIndex(currentIndex)
                    }
                }
            }

            ListView {
                id: parametersView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                boundsBehavior: Flickable.StopAtBounds
                model: parametersModel

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AlwaysOn
                    width: 10
                }

                delegate: Rectangle {
                    required property string name
                    required property string value
                    required property string quality

                    width: ListView.view.width
                    height: rowHeight
                    radius: 15
                    border.color: "black"
                    border.width: 2
                    color: qualityColor(quality)
                    opacity: 0.92

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            Layout.preferredWidth: 78
                            text: name + ":"
                            color: "black"
                            font.bold: true
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: value
                            color: "black"
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        Text {
                            Layout.preferredWidth: 92
                            text: quality
                            color: "black"
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            visible: quality !== ""
                        }
                    }
                }
            }
        }
    }

    function getNetworkParameters(type, quality)
    {
        networkType = type
        networkQuality = quality
    }

    function resetMetricModels()
    {
        parametersModel.clear()
        metricQualities = []
    }

    function appendMetricRow(name, value, quality)
    {
        const normalizedQuality = quality || ""
        parametersModel.append({
                                   "name": name,
                                   "value": value,
                                   "quality": normalizedQuality
                               })
        metricQualities.push(normalizedQuality)
    }

    function setRangeQualities(qualities)
    {
        metricQualities = qualities ? qualities.slice() : []
    }

    function metricQuality(index, fallback)
    {
        if (index < metricQualities.length) {
            return metricQualities[index]
        }
        return fallback
    }

    function populateMetricRows(rows)
    {
        resetMetricModels()
        for (let index = 0; index < rows.length; ++index) {
            const row = rows[index]
            appendMetricRow(row.name, row.value, row.quality || "")
        }
        parametersView.positionViewAtBeginning()
    }

    function formatMetric(value, unit, decimals)
    {
        if (value === undefined || value === null || value === "") {
            return "--"
        }
        if (typeof value === "number") {
            const precision = decimals === undefined ? 1 : decimals
            return value.toFixed(precision) + (unit ? " " + unit : "")
        }
        return unit ? String(value) + " " + unit : String(value)
    }

    function qualityFromPing(pingMs)
    {
        if (pingMs <= 25) {
            return "EXCELLENT"
        }
        if (pingMs <= 50) {
            return "GOOD"
        }
        if (pingMs <= 90) {
            return "FAIR"
        }
        return "POOR"
    }

    function qualityFromLoss(lossPercent)
    {
        if (lossPercent <= 0.5) {
            return "EXCELLENT"
        }
        if (lossPercent <= 2.0) {
            return "GOOD"
        }
        if (lossPercent <= 5.0) {
            return "FAIR"
        }
        return "POOR"
    }

    function qualityFromSystemStatus(status)
    {
        switch (status) {
        case "ACTIVE":
            return "EXCELLENT"
        case "STANDBY":
        case "BOOTING":
            return "GOOD"
        case "CALIBRATING":
        case "UNINITIALIZED":
            return "FAIR"
        case "CRITICAL":
        case "EMERGENCY":
        case "POWEROFF":
            return "POOR"
        default:
            return "GOOD"
        }
    }

    function showSimulationParameters(data)
    {
        if (data === undefined || data === null) {
            clearSimulationParameters()
            return
        }

        const systemQuality = data.systemStatus ? qualityFromSystemStatus(data.systemStatus) : ""
        const linkQuality = data.quality || systemQuality || "GOOD"
        const rows = [
            {"name": "UAV", "value": data.label || "--", "quality": linkQuality},
            {"name": "TYPE", "value": data.vehicleType || "--", "quality": systemQuality || linkQuality},
            {"name": "STATE", "value": data.systemStatus || "--", "quality": systemQuality || linkQuality},
            {"name": "BAT", "value": formatMetric(data.batteryPercentage, "%", 0), "quality": systemQuality || linkQuality},
            {"name": "VOLT", "value": formatMetric(data.batteryVoltage, "V", 2), "quality": systemQuality || linkQuality},
            {"name": "CURR", "value": formatMetric(data.batteryCurrentMilliAmps, "mA", 0), "quality": systemQuality || linkQuality},
            {"name": "HDG", "value": formatMetric(data.headingDegrees, "deg", 0), "quality": linkQuality},
            {"name": "RAT", "value": data.networkType === "NR" ? "5G NR" : (data.networkType || "--"), "quality": linkQuality},
            {"name": "PING", "value": formatMetric(data.pingMs, "ms", 1), "quality": qualityFromPing(Number(data.pingMs || 0))},
            {"name": "RSSI", "value": formatMetric(data.rssiDbm, "dBm", 1), "quality": linkQuality},
            {"name": "RSRP", "value": formatMetric(data.rsrpDbm, "dBm", 1), "quality": linkQuality},
            {"name": "RSRQ", "value": formatMetric(data.rsrqDb, "dB", 1), "quality": linkQuality},
            {"name": "SINR", "value": formatMetric(data.sinrDb, "dB", 1), "quality": linkQuality}
        ]

        if (data.networkType === "NR") {
            rows.push({"name": "SS-RSRP", "value": formatMetric(data.ssRsrpDbm, "dBm", 1), "quality": linkQuality})
            rows.push({"name": "SS-SINR", "value": formatMetric(data.ssSinrDb, "dB", 1), "quality": linkQuality})
        }

        rows.push({"name": "JITTER", "value": formatMetric(data.jitterMs, "ms", 1), "quality": qualityFromPing(Number(data.jitterMs || 0) * 6)})
        rows.push({"name": "LOSS", "value": formatMetric(data.packetLossPct, "%", 1), "quality": qualityFromLoss(Number(data.packetLossPct || 0))})
        rows.push({"name": "THRPT", "value": formatMetric(data.throughputMbps, "Mbps", 1), "quality": linkQuality})
        rows.push({"name": "CELL", "value": data.servingLabel || "--", "quality": linkQuality})
        rows.push({"name": "DIST", "value": formatMetric(data.distanceMeters, "m", 0), "quality": linkQuality})
        rows.push({"name": "SEC", "value": data.security || "--", "quality": linkQuality})

        getNetworkParameters(data.networkType || "", linkQuality)
        populateMetricRows(rows)
    }

    function showMavlinkParameters(data)
    {
        if (data === undefined || data === null) {
            clearSimulationParameters()
            return
        }

        const systemQuality = qualityFromSystemStatus(data.systemStatus || "")
        const rows = [
            {"name": "UAV", "value": data.vehicleType || "--", "quality": systemQuality},
            {"name": "STATUS", "value": data.systemStatus || "--", "quality": systemQuality},
            {"name": "BAT", "value": formatMetric(data.batteryPercentage, "%", 0), "quality": systemQuality},
            {"name": "VOLT", "value": formatMetric(data.batteryVoltage, "V", 2), "quality": systemQuality},
            {"name": "CURR", "value": formatMetric(data.batteryCurrentMilliAmps, "mA", 0), "quality": systemQuality}
        ]

        if (data.latitude !== undefined && data.latitude !== null && data.longitude !== undefined && data.longitude !== null) {
            rows.push({"name": "LAT", "value": formatMetric(data.latitude, "", 6), "quality": systemQuality})
            rows.push({"name": "LON", "value": formatMetric(data.longitude, "", 6), "quality": systemQuality})
        }

        getNetworkParameters("MAVLINK", systemQuality)
        populateMetricRows(rows)
    }

    function clearSimulationParameters()
    {
        getNetworkParameters("", "")
        resetMetricModels()
    }

    function getLteParameters(rssi, rsrp, sinr, rsrq)
    {
        populateMetricRows([
                               {"name": "RSSI", "value": rssi, "quality": metricQuality(0, "GOOD")},
                               {"name": "RSRP", "value": rsrp, "quality": metricQuality(1, "GOOD")},
                               {"name": "SINR", "value": sinr, "quality": metricQuality(2, "GOOD")},
                               {"name": "RSRQ", "value": rsrq, "quality": metricQuality(3, "GOOD")}
                           ])
    }

    function getWCDMAParameters(rssi, rscp, ecio)
    {
        populateMetricRows([
                               {"name": "RSSI", "value": rssi, "quality": metricQuality(0, "GOOD")},
                               {"name": "RSCP", "value": rscp, "quality": metricQuality(1, "GOOD")},
                               {"name": "ECIO", "value": ecio, "quality": metricQuality(2, "GOOD")}
                           ])
    }

    function getGSMParameters(rssi)
    {
        populateMetricRows([
                               {"name": "RSSI", "value": rssi, "quality": metricQuality(0, "GOOD")}
                           ])
    }

    function getCellularRangeParameters(rssi, rsrp, rsrq, sinr)
    {
        setRangeQualities([rssi, rsrp, sinr, rsrq])
    }

    function getTypeNetworkIndex(index)
    {
        setTypeNetworkIndex(changeTypeNetwork.get(index).value)
    }
}
