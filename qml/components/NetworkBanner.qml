import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: networkBanner

    property variant networkType: 0
    property variant networkQuality: 0
    property string dataFilterMode: "ALL"
    property var metricQualities: []
    property var lastVehicleData: null
    property string lastVehicleDataMode: ""
    property int metricCount: parametersModel.count
    property bool hasMetrics: metricCount > 0
    property int rowHeight: metricCount >= 14 ? 30 : (metricCount >= 10 ? 32 : 36)
    property int rowSpacing: 4
    property int minPanelHeight: 212
    property int maxListHeight: 560
    readonly property int computedListHeight: hasMetrics
                                            ? Math.min(maxListHeight,
                                                       metricCount * rowHeight
                                                       + Math.max(0, metricCount - 1) * rowSpacing)
                                            : 0
    property int preferredPanelHeight: hasMetrics
                                      ? Math.max(minPanelHeight,
                                                 28 + headerColumn.implicitHeight
                                                 + computedListHeight
                                                 + 28)
                                      : minPanelHeight
    readonly property color shellColor: "#0a1612"
    readonly property color panelColor: "#0f2019"
    readonly property color panelEdgeColor: "#214234"
    readonly property color accentColor: "#54f06b"
    readonly property color accentSoftColor: "#2a8c49"
    readonly property color textPrimaryColor: "#ebfff0"
    readonly property color textMutedColor: "#8dc69b"
    signal setTypeNetworkIndex(var index)

    width: 404
    implicitWidth: 404
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
        border.color: panelEdgeColor
        border.width: 2
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(15 / 255, 32 / 255, 25 / 255, 0.94) }
            GradientStop { position: 1.0; color: Qt.rgba(10 / 255, 22 / 255, 18 / 255, 0.9) }
        }
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

        ListModel {
            id: displayModes
            ListElement {
                name: "ALL"
                value: "ALL"
            }
            ListElement {
                name: "FLIGHT"
                value: "FLIGHT"
            }
            ListElement {
                name: "LINK"
                value: "LINK"
            }
            ListElement {
                name: "POWER"
                value: "POWER"
            }
            ListElement {
                name: "COLLISION"
                value: "COLLISION"
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            ColumnLayout {
                id: headerColumn
                Layout.fillWidth: true
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3

                        Text {
                            text: "NETWORK"
                            color: textPrimaryColor
                            font.pixelSize: 19
                            font.bold: true
                        }

                        Text {
                            text: hasMetrics ? ((networkType || "--") + " LINK OVERVIEW") : "WAITING FOR TELEMETRY"
                            color: textMutedColor
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 1.3
                        }
                    }

                    ComboBox {
                        id: modesList
                        Layout.preferredWidth: 156
                        Layout.preferredHeight: 34
                        model: changeTypeNetwork
                        textRole: "name"

                        background: Rectangle {
                            radius: 17
                            border.color: panelEdgeColor
                            border.width: 1.5
                            color: "#123344"
                        }

                        contentItem: Text {
                            leftPadding: 12
                            rightPadding: 24
                            text: modesList.displayText
                            color: textPrimaryColor
                            verticalAlignment: Text.AlignVCenter
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        onCurrentIndexChanged: {
                            getTypeNetworkIndex(currentIndex)
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        radius: 14
                        color: "#09130f"
                        border.color: panelEdgeColor
                        border.width: 1.5

                        Image {
                            id: networkTypeImage
                            anchors.centerIn: parent
                            sourceSize.width: 28
                            sourceSize.height: 28
                            source: networkIconForType(networkType)
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        radius: 14
                        color: "#09130f"
                        border.color: panelEdgeColor
                        border.width: 1.5

                        Image {
                            id: networkQualityImage
                            anchors.centerIn: parent
                            sourceSize.width: 28
                            sourceSize.height: 28
                            source: qualityIconForLevel(networkQuality)
                        }
                    }

                    ComboBox {
                        id: dataModeList
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        model: displayModes
                        textRole: "name"

                        background: Rectangle {
                            radius: 17
                            border.color: panelEdgeColor
                            border.width: 1.5
                            color: "#123344"
                        }

                        contentItem: Text {
                            leftPadding: 12
                            rightPadding: 24
                            text: dataModeList.displayText
                            color: textPrimaryColor
                            verticalAlignment: Text.AlignVCenter
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        onCurrentIndexChanged: {
                            const mode = displayModes.get(currentIndex)
                            networkBanner.dataFilterMode = mode ? mode.value : "ALL"
                            networkBanner.refreshDisplayedVehicleData()
                        }
                    }
                }
            }

            ListView {
                id: parametersView
                Layout.fillWidth: true
                Layout.preferredHeight: computedListHeight
                Layout.maximumHeight: maxListHeight
                clip: true
                spacing: rowSpacing
                boundsBehavior: Flickable.StopAtBounds
                model: parametersModel
                visible: hasMetrics

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AlwaysOn
                    width: 8

                    contentItem: Rectangle {
                        implicitWidth: 8
                        radius: 4
                        color: "#1d4734"
                    }

                    background: Rectangle {
                        radius: 4
                        color: "#0a1712"
                    }
                }

                delegate: Rectangle {
                    required property string name
                    required property string value
                    required property string quality

                    width: ListView.view.width
                    height: rowHeight
                    radius: 14
                    border.color: panelEdgeColor
                    border.width: 1
                    color: "#09130f"
                    opacity: 0.96

                    Rectangle {
                        width: 6
                        height: parent.height - 10
                        radius: 3
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        color: quality !== "" ? qualityColor(quality) : accentSoftColor
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 22
                        anchors.rightMargin: 12
                        spacing: 8

                        Text {
                            Layout.preferredWidth: 78
                            text: name + ":"
                            color: textMutedColor
                            font.bold: true
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        Text {
                            Layout.fillWidth: true
                            text: value
                            color: textPrimaryColor
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }

                        Text {
                            Layout.preferredWidth: 92
                            text: quality
                            color: quality !== "" ? qualityColor(quality) : textMutedColor
                            font.bold: true
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            visible: quality !== ""
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !hasMetrics
                radius: 16
                color: "#09130f"
                border.color: panelEdgeColor
                border.width: 1

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Rectangle {
                        width: 56
                        height: 56
                        radius: 28
                        anchors.horizontalCenter: parent.horizontalCenter
                        color: "#102019"
                        border.color: panelEdgeColor
                        border.width: 1

                        Image {
                            anchors.centerIn: parent
                            source: networkIconForType(networkType)
                            sourceSize.width: 28
                            sourceSize.height: 28
                        }
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "No vehicle data yet"
                        color: textPrimaryColor
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 250
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        text: "Select a UAV or start the live telemetry bridge to populate flight, power, link, and collision metrics."
                        color: textMutedColor
                        font.pixelSize: 12
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

    function addRow(rows, name, value, quality)
    {
        rows.push({
                      "name": name,
                      "value": value,
                      "quality": quality || ""
                  })
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

    function qualityFromCollision(severity)
    {
        switch (severity) {
        case "ALERT":
            return "POOR"
        case "WARNING":
            return "FAIR"
        case "SAFE":
            return "EXCELLENT"
        default:
            return "GOOD"
        }
    }

    function appendFlightRows(rows, data, generalQuality, linkQuality)
    {
        addRow(rows, "UAV", data.label || data.vehicleType || "--", generalQuality)
        addRow(rows, "TYPE", data.vehicleType || "--", generalQuality)
        addRow(rows, "STATE", data.systemStatus || "--", generalQuality)
        addRow(rows, "HDG", formatMetric(data.headingDegrees, "deg", 0), linkQuality || generalQuality)
        addRow(rows, "LAT", formatMetric(data.latitude, "", 6), generalQuality)
        addRow(rows, "LON", formatMetric(data.longitude, "", 6), generalQuality)
        addRow(rows, "ALT", formatMetric(data.altitudeMeters, "m", 1), generalQuality)
    }

    function appendPowerRows(rows, data, generalQuality)
    {
        addRow(rows, "BAT", formatMetric(data.batteryPercentage, "%", 0), generalQuality)
        addRow(rows, "VOLT", formatMetric(data.batteryVoltage, "V", 2), generalQuality)
        addRow(rows, "CURR", formatMetric(data.batteryCurrentMilliAmps, "mA", 0), generalQuality)
    }

    function appendLinkRows(rows, data, linkQuality)
    {
        addRow(rows, "RAT", data.networkType === "NR" ? "5G NR" : (data.networkType || "--"), linkQuality)
        addRow(rows, "PING", formatMetric(data.pingMs, "ms", 1), qualityFromPing(Number(data.pingMs || 0)))
        addRow(rows, "RSSI", formatMetric(data.rssiDbm, "dBm", 1), linkQuality)
        addRow(rows, "RSRP", formatMetric(data.rsrpDbm, "dBm", 1), linkQuality)
        addRow(rows, "RSRQ", formatMetric(data.rsrqDb, "dB", 1), linkQuality)
        addRow(rows, "SINR", formatMetric(data.sinrDb, "dB", 1), linkQuality)
        if (data.networkType === "NR") {
            addRow(rows, "SS-RSRP", formatMetric(data.ssRsrpDbm, "dBm", 1), linkQuality)
            addRow(rows, "SS-SINR", formatMetric(data.ssSinrDb, "dB", 1), linkQuality)
        }
        addRow(rows, "JITTER", formatMetric(data.jitterMs, "ms", 1), qualityFromPing(Number(data.jitterMs || 0) * 6))
        addRow(rows, "LOSS", formatMetric(data.packetLossPct, "%", 1), qualityFromLoss(Number(data.packetLossPct || 0)))
        addRow(rows, "THRPT", formatMetric(data.throughputMbps, "Mbps", 1), linkQuality)
        addRow(rows, "CELL", data.servingLabel || "--", linkQuality)
        addRow(rows, "DIST", formatMetric(data.distanceMeters, "m", 0), linkQuality)
        addRow(rows, "SEC", data.security || "--", linkQuality)
    }

    function appendCollisionRows(rows, data)
    {
        const severity = data.collisionSeverity || "SAFE"
        const collisionQuality = qualityFromCollision(severity)
        addRow(rows, "COLL", severity, collisionQuality)
        addRow(rows, "SEP", formatMetric(data.nearestUavDistanceMeters, "m", 1), collisionQuality)
        addRow(rows, "HSEP", formatMetric(data.collisionHorizontalDistanceMeters, "m", 1), collisionQuality)
        addRow(rows, "VSEP", formatMetric(data.nearestUavVerticalDistanceMeters, "m", 1), collisionQuality)
        addRow(rows, "NEAR", data.collisionPeerLabel || "--", collisionQuality)
        addRow(rows, "CLOSE", formatMetric(data.collisionClosingSpeedMps, "m/s", 2), collisionQuality)
        addRow(rows, "TTC", formatMetric(data.collisionTimeToClosestSeconds, "s", 1), collisionQuality)
        addRow(rows, "PRED", formatMetric(data.collisionPredictedMinDistanceMeters, "m", 1), collisionQuality)
        addRow(rows, "AVOID", data.collisionAvoidanceMode || "NONE", collisionQuality)
        addRow(rows, "SAFEBOX", data.collisionSafetyVolumeBreached ? "BREACHED" : "CLEAR", collisionQuality)
    }

    function refreshDisplayedVehicleData()
    {
        if (lastVehicleData === undefined || lastVehicleData === null) {
            clearSimulationParameters()
            return
        }

        const data = lastVehicleData
        const systemQuality = data.systemStatus ? qualityFromSystemStatus(data.systemStatus) : ""
        const linkQuality = data.quality || systemQuality || "GOOD"
        const rows = []
        const mode = dataFilterMode || "ALL"

        if (mode === "ALL" || mode === "FLIGHT") {
            appendFlightRows(rows, data, systemQuality || linkQuality, linkQuality)
        }
        if (mode === "ALL" || mode === "POWER") {
            appendPowerRows(rows, data, systemQuality || linkQuality)
        }
        if (mode === "ALL" || mode === "LINK") {
            appendLinkRows(rows, data, linkQuality)
        }
        if (mode === "ALL" || mode === "COLLISION") {
            appendCollisionRows(rows, data)
        }

        getNetworkParameters(data.networkType || (lastVehicleDataMode === "mavlink" ? "MAVLINK" : ""),
                             linkQuality)
        populateMetricRows(rows)
    }

    function showSimulationParameters(data)
    {
        if (data === undefined || data === null) {
            clearSimulationParameters()
            return
        }
        lastVehicleData = data
        lastVehicleDataMode = "simulation"
        refreshDisplayedVehicleData()
    }

    function showMavlinkParameters(data)
    {
        if (data === undefined || data === null) {
            clearSimulationParameters()
            return
        }
        lastVehicleData = data
        lastVehicleDataMode = "mavlink"
        refreshDisplayedVehicleData()
    }

    function clearSimulationParameters()
    {
        lastVehicleData = null
        lastVehicleDataMode = ""
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
