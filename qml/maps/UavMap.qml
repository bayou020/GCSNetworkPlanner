import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.animation
import QtPositioning
import QtLocation
import MapLibre.Location 4.0


Item {
    id: root
    function preferredMapType(mapTypes) {
        for (let i = 0; i < mapTypes.length; ++i) {
            const mapType = mapTypes[i];
            if (mapType.style === MapType.StreetMap
                    || mapType.style === MapType.PedestrianMap
                    || mapType.style === MapType.CarNavigationMap
                    || mapType.style === MapType.CycleMap) {
                return mapType;
            }
        }
        return mapTypes.length > 0 ? mapTypes[0] : null;
    }

    function antennaColor(rat) {
        switch ((rat || "").toLowerCase()) {
        case "nr":
            return "#7d4cff"
        case "lte":
            return "#ff7b38"
        default:
            return "#4f91c9"
        }
    }

    function altitudeBounds() {
        if (typeof simulationFeed === "undefined"
                || simulationFeed === null
                || simulationFeed.uavCount === 0) {
            return {"min": 0, "max": 0}
        }
        return {
            "min": Number(simulationFeed.minAltitudeMeters),
            "max": Number(simulationFeed.maxAltitudeMeters)
        }
    }

    function altitudeElevationOffset(altitudeMeters) {
        const bounds = altitudeBounds()
        const spread = Math.max(0.1, bounds.max - bounds.min)
        const normalized = spread < 0.5
                ? 0.5
                : Math.max(0.0, Math.min(1.0, (altitudeMeters - bounds.min) / spread))
        const tiltFactor = 0.65 + Math.max(0.0, Math.min(1.0, map.tilt / 60.0)) * 0.9
        const zoomFactor = Math.max(0.9, Math.min(1.35, 0.9 + (map.zoomLevel - 15.0) / 10.0))
        return (28 + normalized * 84) * tiltFactor * zoomFactor
    }

    function altitudeProjection(altitudeMeters) {
        const lift = altitudeElevationOffset(altitudeMeters)
        const tiltNorm = Math.max(0.0, Math.min(1.0, (map.tilt - 8.0) / 62.0))
        const zoomNorm = Math.max(0.75, Math.min(1.3, 0.82 + (map.zoomLevel - 15.0) / 9.0))
        const perspective = lift * tiltNorm * zoomNorm
        const bearingRad = map.bearing * Math.PI / 180.0

        return {
            "x": -Math.sin(bearingRad) * perspective * 0.72,
            "y": -Math.cos(bearingRad) * perspective * 1.08,
            "tiltNorm": tiltNorm
        }
    }

    function simulationBounds() {
        if (typeof simulationFeed === "undefined"
                || simulationFeed === null
                || !simulationFeed.hasData) {
            return null
        }

        let minLatitude = Infinity
        let maxLatitude = -Infinity
        let minLongitude = Infinity
        let maxLongitude = -Infinity

        const appendBounds = function(items) {
            for (let i = 0; i < items.length; ++i) {
                const entry = items[i]
                const latitude = Number(entry.latitude)
                const longitude = Number(entry.longitude)
                if (!Number.isFinite(latitude) || !Number.isFinite(longitude)) {
                    continue
                }

                minLatitude = Math.min(minLatitude, latitude)
                maxLatitude = Math.max(maxLatitude, latitude)
                minLongitude = Math.min(minLongitude, longitude)
                maxLongitude = Math.max(maxLongitude, longitude)
            }
        }

        appendBounds(simulationFeed.antennas)
        appendBounds(simulationFeed.uavs)

        if (!Number.isFinite(minLatitude) || !Number.isFinite(minLongitude)) {
            return null
        }

        return {
            "minLatitude": minLatitude,
            "maxLatitude": maxLatitude,
            "minLongitude": minLongitude,
            "maxLongitude": maxLongitude
        }
    }

    function zoomLevelForSimulationBounds(bounds) {
        const latitudeSpan = Math.max(0.0005, bounds.maxLatitude - bounds.minLatitude)
        const longitudeSpan = Math.max(0.0005, bounds.maxLongitude - bounds.minLongitude)
        const maxSpan = Math.max(latitudeSpan, longitudeSpan)

        if (maxSpan <= 0.002) {
            return 15
        }
        if (maxSpan <= 0.005) {
            return 14
        }
        if (maxSpan <= 0.01) {
            return 13
        }
        if (maxSpan <= 0.02) {
            return 12
        }
        if (maxSpan <= 0.05) {
            return 11
        }
        if (maxSpan <= 0.1) {
            return 10
        }
        if (maxSpan <= 0.2) {
            return 9
        }
        return 8
    }

    function maybeCenterOnSimulation(force) {
        if ((!force && simulationCentered)
                || typeof simulationFeed === "undefined"
                || simulationFeed === null
                || !simulationFeed.hasData) {
            return
        }

        const bounds = simulationBounds()
        if (bounds === null) {
            return
        }

        const centerLatitude = (bounds.minLatitude + bounds.maxLatitude) * 0.5
        const centerLongitude = (bounds.minLongitude + bounds.maxLongitude) * 0.5
        map.center = QtPositioning.coordinate(centerLatitude, centerLongitude)
        map.zoomLevel = zoomLevelForSimulationBounds(bounds)
        simulationCentered = true
    }

    function mapCoordinateFromPoint(point) {
        return map.toCoordinate(Qt.point(point.x, point.y), false);
    }

    function ensureWeatherOverlay() {
        if (weatherOverlaySourceParam !== null
                || typeof weatherService === "undefined"
                || weatherService === null
                || weatherService.weatherTileTemplate === "") {
            return
        }

        const tileUrl = weatherService.weatherTileTemplate
                .replace(/\\/g, "\\\\")
                .replace(/"/g, "\\\"")

        weatherOverlaySourceParam = Qt.createQmlObject(`
            import MapLibre.Location 4.0

            SourceParameter {
                styleId: "openweather-source"
                type: "raster"
                property var tiles: ["${tileUrl}"]
                property int tileSize: 256
            }
        `, mapStyle, "openWeatherSource")

        weatherOverlayLayerParam = Qt.createQmlObject(`
            import MapLibre.Location 4.0

            LayerParameter {
                styleId: "openweather-layer"
                type: "raster"
                property string source: "openweather-source"

                paint: {
                    "raster-opacity": 1.0
                }
            }
        `, mapStyle, "openWeatherLayer")

        mapStyle.addParameter(weatherOverlaySourceParam)
        mapStyle.addParameter(weatherOverlayLayerParam)
    }

    function reloadSampleModel() {
        sampleModel.clear()
        if (typeof cellbase === "undefined" || cellbase === null) {
            return
        }

        cellindex = cellbase.getRows()

        for (let i = 0; i < cellindex - 1; ++i) {
            sampleModel.append({
                                   "latitude": cellbase.getLatitude(i),
                                   "longitude": cellbase.getLongitude(i),
                                   "range": cellbase.getRange(i),
                                   "radio": cellbase.getRadio(i)
                               })

            if (sampleModel.get(i).radio === "LTE") {
                sampleModel.setProperty(i, "radio", "red")
            } else if (sampleModel.get(i).radio === "UMTS") {
                sampleModel.setProperty(i, "radio", "green")
            } else if (sampleModel.get(i).radio === "GSM") {
                sampleModel.setProperty(i, "radio", "blue")
            } else {
                sampleModel.setProperty(i, "radio", "white")
            }
        }
    }

    function handleLeftMapTap(point) {
        const coordinate = mapCoordinateFromPoint(point)
        mouseLatitude = coordinate.latitude
        mouseLongitude = coordinate.longitude
        console.log("map click lat " + coordinate.latitude + " lon " + coordinate.longitude)
        if (typeof weatherService !== "undefined" && weatherService !== null) {
            if (!weatherService.apiKeyPresent) {
                console.warn("OPENWEATHERMAP_API_KEY is not set")
            }
            const columns = map.zoomLevel >= 12 ? 3 : 2
            const rows = map.zoomLevel >= 10 ? 3 : 2
            const points = [{
                               "latitude": coordinate.latitude,
                               "longitude": coordinate.longitude,
                               "primary": true
                           }]
            for (let row = 0; row < rows; ++row) {
                for (let column = 0; column < columns; ++column) {
                    const x = ((column + 0.5) / columns) * map.width
                    const y = ((row + 0.5) / rows) * map.height
                    const viewportCoordinate = map.toCoordinate(Qt.point(x, y), false)
                    points.push({
                                    "latitude": viewportCoordinate.latitude,
                                    "longitude": viewportCoordinate.longitude
                                })
                }
            }
            weatherService.fetchWeatherPoints(points)
        }
        signalAppenList(coordinate.latitude, coordinate.longitude)

        updatePointsMap()
        reloadSampleModel()
    }

    function handleRightMapTap() {
        sampleModel.clear()
        meteoModel.clear()
        clearAntennas()
        if (typeof weatherService !== "undefined" && weatherService !== null) {
            weatherService.clear()
        }

        console.log("top lat " + topleftRLat + " top lon " + topleftRLon)

        if (polylinemap.pathLength() > 0) {
            polylinemap.removeCoordinate(polylinemap.pathLength() - 1)
        }
    }

    property double valueLatitude: 39.904459
    property double valueLongitude: 116.406847
    property double maptype: 0
    property double pathlen: 1
    property double mouseLatitude: 0
    property double mouseLongitude:0
    property Rectangle highlightItem : null;
    property double distance: 0
    property double tiltValue:0
    property double anglec: 0
    property variant listWP: []
    property int indexR:0


    signal signalAppenList(var x,var y)
    signal signalCalculateDistance(var lat1,var latn,var long1,var longn)
    signal  distanceCalculated(double distance)
    signal notifyAddDistance()
    property variant locationOslo: QtPositioning.coordinate( 59.93, 10.76)
    property int cellindex:0
    signal clearAntennas()
    property int weatherIndex:0
    property real topleftRLat:0
    property real topleftRLon:0
    property real bottomrightRLat:0
    property real bottomrightRLon:0
    property var weatherOverlaySourceParam: null
    property var weatherOverlayLayerParam: null
    property bool simulationCentered: false
    property real lastSimulationTime: -1
    property bool denseSimulation: typeof simulationFeed !== "undefined"
                                   && simulationFeed !== null
                                   && simulationFeed.uavCount >= 50
    property int densePositionAnimationMs: denseSimulation ? 85 : 260
    property int denseHeadingAnimationMs: denseSimulation ? 120 : 220


    anchors.fill: parent

    Planner {
        id:planner
    }

    Connections {
        target: typeof weatherService !== "undefined" ? weatherService : null

        function onMarkersChanged() {
            console.log("weather markers", weatherService.markers.length)
        }

        function onErrorChanged() {
            if (weatherService.errorString !== "") {
                console.warn("weather error:", weatherService.errorString)
            }
        }
    }

    Connections {
        target: typeof simulationFeed !== "undefined" ? simulationFeed : null

        function onUavsChanged() {
            const forceRecenter = simulationFeed.simTime <= 1.0
                    || (root.lastSimulationTime >= 0
                        && simulationFeed.simTime + 0.001 < root.lastSimulationTime)
            root.maybeCenterOnSimulation(forceRecenter)
        }

        function onAntennasChanged() {
            const forceRecenter = simulationFeed.simTime <= 1.0
                    || (root.lastSimulationTime >= 0
                        && simulationFeed.simTime + 0.001 < root.lastSimulationTime)
            root.maybeCenterOnSimulation(forceRecenter)
        }

        function onSimTimeChanged() {
            if (root.lastSimulationTime >= 0
                    && simulationFeed.simTime + 0.001 < root.lastSimulationTime) {
                root.simulationCentered = false
            }
            root.lastSimulationTime = simulationFeed.simTime
        }

        function onHasDataChanged() {
            if (!simulationFeed.hasData) {
                root.simulationCentered = false
                root.lastSimulationTime = -1
            }
        }

        function onErrorChanged() {
            if (simulationFeed.errorString !== "") {
                console.warn("ns-3 simulation error:", simulationFeed.errorString)
            }
        }
    }

    Map {
        id: map
        anchors.fill: parent
        zoomLevel: 3
        minimumZoomLevel: 2
        maximumZoomLevel: 20
        tilt: root.tiltValue
        property bool pinchAdjustingZoom: false
        property vector3d animDest: Qt.vector3d(0, 0, 0)

        plugin: Plugin {
            id: mapPlugin
            name: "maplibre"

            PluginParameter {
                name: "maplibre.api.provider"
                value: "mapbox"
            }

            PluginParameter {
                name: "maplibre.api.key"
                value: typeof mapboxAccessToken !== "undefined" ? mapboxAccessToken : ""
            }

            PluginParameter {
                name: "maplibre.map.styles"
                value: typeof mapboxStyleUrl !== "undefined" ? mapboxStyleUrl : ""
            }
        }

        copyrightsVisible: true
        center {
            latitude: root.valueLatitude
            longitude: root.valueLongitude
        }

        activeMapType: root.preferredMapType(supportedMapTypes)

        MapLibre.style: Style {
            id: mapStyle
        }

        Component.onCompleted: {
            if ((typeof mapboxAccessToken === "undefined" || mapboxAccessToken === "")
                    || (typeof mapboxStyleUrl === "undefined" || mapboxStyleUrl === "")) {
                console.warn("Set MAPBOX_ACCESS_TOKEN and MAPBOX_STYLE_URL to load the MapLibre base map.")
            }
            resetPinchMinMax()
            root.ensureWeatherOverlay()
        }

        BoundaryRule on zoomLevel {
            id: zoomBoundaryRule
            minimum: map.minimumZoomLevel
            maximum: map.maximumZoomLevel
        }

        onZoomLevelChanged: function() {
            zoomBoundaryRule.returnToBounds()
            if (!pinchAdjustingZoom) {
                resetPinchMinMax()
            }
            sliderHorizontalZoom.value = map.zoomLevel
        }

        onAnimDestChanged: {
            if (flickAnimation.running) {
                const delta = Qt.vector2d(animDest.x - flickAnimation.animDestLast.x,
                                          animDest.y - flickAnimation.animDestLast.y)
                map.pan(-delta.x, -delta.y)
                flickAnimation.animDestLast = animDest
            }
        }

        function resetPinchMinMax() {
            pinch.persistentScale = 1
            pinch.scaleAxis.minimum = Math.pow(2, minimumZoomLevel - zoomLevel + 1)
            pinch.scaleAxis.maximum = Math.pow(2, maximumZoomLevel - zoomLevel - 1)
        }

        ListModel {
            id: meteoModel
        }

        ListModel {
            id: sampleModel
        }

        MapPolyline {
            id: polylinemap
            line.width: 2
            line.color: "blue"
        }

        MapRectangle {
            id: rectangleMap
            color: "green"
            border.width: 6
            border.color: "blue"
            opacity: 0.1
            topLeft {
                latitude: root.topleftRLat
                longitude: root.topleftRLon
            }
            bottomRight {
                latitude: root.bottomrightRLat
                longitude: root.bottomrightRLon
            }
        }

        MapItemView {
            id: mapItemView
            model: sampleModel

            delegate: MapQuickItem {
                id: delegate_quad
                anchorPoint.x: delegate_quad.width * 0.5
                anchorPoint.y: delegate_quad.height * 0.5
                zoomLevel: map.zoomLevel

                sourceItem: Image {
                    id: delegate_quad_img
                    sourceSize.width: 40
                    sourceSize.height: 40
                    source: "qrc:/ico/marker.gif"
                    transform: Rotation {
                        id: delegate_quad_img_rot
                        origin.x: delegate_quad_img.width / 2
                        origin.y: delegate_quad_img.height / 2
                    }
                }

                coordinate: QtPositioning.coordinate(latitude, longitude)
            }
        }

        MapItemView {
            id: mapitemcircle
            model: sampleModel

            delegate: MapCircle {
                id: circleradius
                color: radio
                center {
                    latitude: latitude
                    longitude: longitude
                }
                radius: range
                border.color: "black"
                border.width: 10
                opacity: 0.1
            }
        }

        MapItemView {
            id: simulationAntennaCoverage
            model: typeof simulationFeed !== "undefined" && simulationFeed !== null
                   ? simulationFeed.antennaModel : null

            delegate: MapCircle {
                required property var modelData
                color: Qt.alpha(root.antennaColor(modelData.rat), 0.18)
                border.color: root.antennaColor(modelData.rat)
                border.width: 3
                opacity: 1.0
                center: QtPositioning.coordinate(modelData.latitude, modelData.longitude)
                radius: modelData.rangeMeters
                visible: modelData.rangeMeters > 0
            }
        }

        MapItemView {
            id: simulationAntennaMarkers
            model: typeof simulationFeed !== "undefined" && simulationFeed !== null
                   ? simulationFeed.antennaModel : null

            delegate: MapQuickItem {
                required property var modelData
                coordinate: QtPositioning.coordinate(modelData.latitude, modelData.longitude)
                anchorPoint.x: 20
                anchorPoint.y: 20
                z: 15

                sourceItem: Item {
                    width: 76
                    height: 76

                    Rectangle {
                        anchors.centerIn: parent
                        width: 28
                        height: 28
                        radius: 14
                        color: root.antennaColor(modelData.rat)
                        border.color: "white"
                        border.width: 2

                        Text {
                            anchors.centerIn: parent
                            text: "A"
                            color: "white"
                            font.bold: true
                            font.pixelSize: 13
                        }
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        color: "#dd13202b"
                        radius: 8
                        height: 20
                        width: Math.max(42, antennaLabel.implicitWidth + 14)

                        Text {
                            id: antennaLabel
                            anchors.centerIn: parent
                            color: "white"
                            font.pixelSize: 11
                            font.bold: true
                            text: modelData.label
                        }
                    }
                }
            }
        }

        MapItemView {
            id: simulationUavMarkers
            model: typeof simulationFeed !== "undefined" && simulationFeed !== null
                   ? simulationFeed.uavModel : null

            delegate: MapQuickItem {
                required property var modelData
                property bool selected: typeof simulationFeed !== "undefined"
                                        && simulationFeed !== null
                                        && simulationFeed.selectedUavId === modelData.id
                property bool denseMarker: root.denseSimulation && !selected
                property real latitude: modelData.latitude !== undefined
                                        ? Number(modelData.latitude) : 0
                property real longitude: modelData.longitude !== undefined
                                         ? Number(modelData.longitude) : 0
                property real altitudeMeters: modelData.altitudeMeters !== undefined
                                              ? Number(modelData.altitudeMeters) : 0
                property real headingDegrees: modelData.headingDegrees !== undefined
                                              ? Number(modelData.headingDegrees) : 0
                property string collisionSeverity: modelData.collisionSeverity !== undefined
                                                  ? String(modelData.collisionSeverity) : "SAFE"
                property var projection: denseMarker
                                         ? {"x": 0, "y": 0, "tiltNorm": 0}
                                         : root.altitudeProjection(altitudeMeters)
                property real elevationOffset: denseMarker
                                               ? 0
                                               : root.altitudeElevationOffset(altitudeMeters)
                property bool showAltitudeIndicator: !denseMarker
                                                    && map.tilt >= 60
                                                    && map.zoomLevel >= 4
                property color collisionAccent: collisionSeverity === "ALERT"
                                                ? "#ff3355"
                                                : (collisionSeverity === "WARNING"
                                                   ? "#ffb300"
                                                   : "#6f9bbd")
                autoFadeIn: false
                coordinate: QtPositioning.coordinate(latitude, longitude, altitudeMeters)
                anchorPoint.x: denseMarker ? 16 : uavMarkerBody.width * 0.5
                anchorPoint.y: denseMarker ? 16 : groundAnchor.y
                zoomLevel: map.zoomLevel
                z: selected ? 40 : (denseMarker ? 18 : 25)

                Behavior on latitude {
                    enabled: !denseMarker && root.densePositionAnimationMs > 0
                    NumberAnimation {
                        duration: root.densePositionAnimationMs
                        easing.type: Easing.InOutQuad
                    }
                }

                Behavior on longitude {
                    enabled: !denseMarker && root.densePositionAnimationMs > 0
                    NumberAnimation {
                        duration: root.densePositionAnimationMs
                        easing.type: Easing.InOutQuad
                    }
                }

                Behavior on headingDegrees {
                    enabled: !denseMarker && root.denseHeadingAnimationMs > 0
                    NumberAnimation {
                        duration: root.denseHeadingAnimationMs
                        easing.type: Easing.OutCubic
                    }
                }

                sourceItem: Item {
                    id: uavMarkerBody
                    width: denseMarker ? 32 : 92
                    height: denseMarker ? 32 : 184

                    readonly property real groundCenterX: groundAnchor.x + groundAnchor.width * 0.5
                    readonly property real groundCenterY: groundAnchor.y + groundAnchor.height * 0.5
                    readonly property real floatingBottom: Math.max(36, groundAnchor.y - 18)
                    readonly property real projectedX: projection.x
                    readonly property real projectedY: projection.y
                    readonly property real projectedTilt: projection.tiltNorm
                    readonly property real droneBottomY: groundCenterY + projectedY - 18
                    readonly property real droneCenterX: groundCenterX + projectedX
                    readonly property real stemDx: droneCenterX - groundCenterX
                    readonly property real stemDy: droneBottomY - groundCenterY
                    readonly property real stemLength: Math.max(8, Math.sqrt(stemDx * stemDx + stemDy * stemDy))
                    readonly property real stemAngle: Math.atan2(stemDy, stemDx) * 180 / Math.PI + 90
                    readonly property real shadowWidth: Math.max(12, 28 - elevationOffset * 0.06)
                    readonly property real shadowHeight: Math.max(5, shadowWidth * (0.38 - projectedTilt * 0.08))

                    Rectangle {
                        visible: denseMarker
                        anchors.centerIn: parent
                        width: selected ? 30 : 24
                        height: width
                        radius: width / 2
                        color: selected ? "#55ffd54f"
                                        : (collisionSeverity === "ALERT"
                                           ? "#66ff3355"
                                           : (collisionSeverity === "WARNING"
                                              ? "#66ffb300"
                                              : "#55153242"))
                        border.color: selected ? "#ffd54f" : collisionAccent
                        border.width: selected ? 2 : 1
                    }

                    Image {
                        id: denseUavImage
                        visible: denseMarker
                        anchors.centerIn: parent
                        width: selected ? 20 : 16
                        height: width
                        sourceSize.width: width
                        sourceSize.height: height
                        fillMode: Image.PreserveAspectFit
                        smooth: false
                        mipmap: false
                        source: "qrc:/ico/drone_i.ico"

                        transform: Rotation {
                            origin.x: denseUavImage.width / 2
                            origin.y: denseUavImage.height / 2
                            angle: headingDegrees
                        }
                    }

                    Rectangle {
                        visible: denseMarker && selected
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.top
                        anchors.bottomMargin: 4
                        color: "#dd174762"
                        radius: 7
                        height: 18
                        width: Math.max(54, denseUavLabel.implicitWidth + 12)

                        Text {
                            id: denseUavLabel
                            anchors.centerIn: parent
                            color: "white"
                            font.pixelSize: 10
                            font.bold: true
                            text: modelData.label
                        }
                    }

                    Rectangle {
                        visible: !denseMarker
                        x: uavMarkerBody.groundCenterX - width * 0.5 - uavMarkerBody.projectedX * 0.16
                        y: uavMarkerBody.groundCenterY - height * 0.5 - Math.max(0, uavMarkerBody.projectedY) * 0.08
                        width: uavMarkerBody.shadowWidth
                        height: uavMarkerBody.shadowHeight
                        radius: height / 2
                        color: selected ? "#88ffd54f"
                                        : (collisionSeverity === "ALERT"
                                           ? "#88ff3355"
                                           : (collisionSeverity === "WARNING"
                                              ? "#88ffb300"
                                              : "#7a22384a"))
                        opacity: 0.88 - uavMarkerBody.projectedTilt * 0.18
                    }

                    Rectangle {
                        id: groundAnchor
                        visible: !denseMarker
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 8
                        width: 10
                        height: 10
                        radius: 5
                        color: selected ? "#ffd54f"
                                        : (collisionSeverity === "ALERT"
                                           ? "#ff3355"
                                           : (collisionSeverity === "WARNING"
                                              ? "#ffb300"
                                              : "#d2141d28"))
                        border.color: selected ? "#ffd54f" : collisionAccent
                        border.width: 1.5
                    }

                    Rectangle {
                        visible: !denseMarker
                        x: uavMarkerBody.groundCenterX - width * 0.5
                        y: uavMarkerBody.groundCenterY - height
                        width: selected ? 4 : 3
                        height: uavMarkerBody.stemLength
                        radius: width / 2
                        color: selected ? "#ffd54f" : collisionAccent
                        opacity: 0.82
                        rotation: uavMarkerBody.stemAngle
                        transformOrigin: Item.Bottom
                    }

                    Rectangle {
                        visible: !denseMarker
                        x: liveUavImage.x + liveUavImage.width * 0.5 - width * 0.5
                        y: liveUavImage.y + liveUavImage.height - height
                        width: 44
                        height: 44
                        radius: 22
                        color: selected ? "#55ffd54f"
                                        : (collisionSeverity === "ALERT"
                                           ? "#22ff3355"
                                           : (collisionSeverity === "WARNING"
                                              ? "#22ffb300"
                                              : "transparent"))
                        border.color: selected ? "#ffd54f"
                                               : (collisionSeverity === "SAFE"
                                                  ? "transparent"
                                                  : collisionAccent)
                        border.width: selected ? 2 : (collisionSeverity === "SAFE" ? 0 : 2)
                    }

                    Image {
                        id: liveUavImage
                        visible: !denseMarker
                        sourceSize.width: 40
                        sourceSize.height: 40
                        width: 40
                        height: 40
                        x: uavMarkerBody.droneCenterX - width * 0.5
                        y: uavMarkerBody.droneBottomY - height
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        mipmap: true
                        source: "qrc:/ico/drone_i.ico"
                        transform: [
                            Scale {
                                origin.x: liveUavImage.width / 2
                                origin.y: liveUavImage.height
                                xScale: 1.0 + uavMarkerBody.projectedTilt * 0.08
                                yScale: 1.0 - uavMarkerBody.projectedTilt * 0.1
                            },
                            Rotation {
                                origin.x: liveUavImage.width / 2
                                origin.y: liveUavImage.height / 2
                                angle: headingDegrees
                            }
                        ]
                    }

                    Rectangle {
                        visible: !denseMarker
                        x: uavMarkerBody.droneCenterX - width * 0.5
                        y: liveUavImage.y - height - 6
                        color: collisionSeverity === "ALERT"
                               ? "#dd8b1624"
                               : (collisionSeverity === "WARNING"
                                  ? "#dd7a4e00"
                                  : "#dd174762")
                        radius: 8
                        height: 20
                        width: Math.max(42, uavLabel.implicitWidth + 14)

                        Text {
                            id: uavLabel
                            anchors.centerIn: parent
                            color: "white"
                            font.pixelSize: 11
                            font.bold: true
                            text: modelData.label
                        }
                    }

                    Rectangle {
                        x: uavMarkerBody.droneCenterX - width * 0.5
                        y: liveUavImage.y - height - 30
                        visible: showAltitudeIndicator
                        color: selected ? "#d2ffd54f"
                                        : (collisionSeverity === "ALERT"
                                           ? "#c58b1624"
                                           : (collisionSeverity === "WARNING"
                                              ? "#c57a4e00"
                                              : "#c5223440"))
                        border.color: selected ? "#ffe082" : collisionAccent
                        border.width: 1
                        radius: 7
                        height: 18
                        width: Math.max(40, altitudeLabel.implicitWidth + 12)

                        Text {
                            id: altitudeLabel
                            anchors.centerIn: parent
                            color: selected ? "#1d2630" : "#f2f8fc"
                            font.pixelSize: 10
                            font.bold: true
                            text: Math.round(altitudeMeters) + " m"
                        }
                    }

                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            onTapped: function() {
                                if (typeof simulationFeed !== "undefined" && simulationFeed !== null) {
                                    simulationFeed.selectUavById(modelData.id)
                                }
                            }
                        }
                    }
                }
            }
        }

        MapQuickItem {
            id: main_quadcopter
            visible: typeof simulationFeed === "undefined"
                     || simulationFeed === null
                     || !simulationFeed.hasData
            coordinate: QtPositioning.coordinate(root.valueLatitude, root.valueLongitude)
            anchorPoint.x: main_quadcopter.width * 0.5
            anchorPoint.y: main_quadcopter.height * 0.5
            zoomLevel: map.zoomLevel

            sourceItem: Image {
                id: quadcopter_image
                sourceSize.width: 40
                sourceSize.height: 40
                source: "qrc:/ico/drone_i.ico"

                transform: Rotation {
                    id: quadcopter_image_rot
                    origin.x: quadcopter_image.width / 2
                    origin.y: quadcopter_image.height / 2
                    angle: 0
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        console.log("Main Quad clicked")
                    }
                }
            }
        }

        PinchHandler {
            id: pinch
            target: null
            property real rawBearing: 0
            property var startCentroid
            onActiveChanged: {
                if (active) {
                    flickAnimation.stop()
                    startCentroid = map.toCoordinate(centroid.position, false)
                } else {
                    flickAnimation.restart(centroid.velocity)
                    map.resetPinchMinMax()
                }
            }
            onScaleChanged: function(delta) {
                map.pinchAdjustingZoom = true
                map.zoomLevel += Math.log2(delta)
                map.alignCoordinateToPoint(startCentroid, centroid.position)
                map.pinchAdjustingZoom = false
            }
            onRotationChanged: function(delta) {
                rawBearing -= delta
                map.bearing = Math.abs(rawBearing) < 5 ? 0 : rawBearing
                map.alignCoordinateToPoint(startCentroid, centroid.position)
            }
            grabPermissions: PointerHandler.TakeOverForbidden
        }

        WheelHandler {
            id: wheel
            acceptedDevices: Qt.platform.pluginName === "cocoa" || Qt.platform.pluginName === "wayland"
                             ? PointerDevice.Mouse | PointerDevice.TouchPad
                             : PointerDevice.Mouse
            onWheel: function(event) {
                const coordinate = map.toCoordinate(wheel.point.position)
                switch (event.modifiers) {
                case Qt.NoModifier:
                    map.zoomLevel += event.angleDelta.y / 120
                    break
                case Qt.ShiftModifier:
                    map.bearing += event.angleDelta.y / 15
                    break
                case Qt.ControlModifier:
                    map.tilt += event.angleDelta.y / 15
                    break
                }
                map.alignCoordinateToPoint(coordinate, wheel.point.position)
            }
        }

        DragHandler {
            id: drag
            target: null
            onTranslationChanged: function(delta) {
                map.pan(-delta.x, -delta.y)
            }
            onActiveChanged: {
                if (active) {
                    flickAnimation.stop()
                } else {
                    flickAnimation.restart(centroid.velocity)
                }
            }
        }

        Vector3dAnimation on animDest {
            id: flickAnimation
            property vector3d animDestLast: Qt.vector3d(0, 0, 0)
            from: Qt.vector3d(0, 0, 0)
            duration: 500
            easing.type: Easing.OutQuad

            function restart(vel) {
                stop()
                map.animDest = Qt.vector3d(0, 0, 0)
                animDestLast = Qt.vector3d(0, 0, 0)
                to = Qt.vector3d(vel.x / duration * 100, vel.y / duration * 100, 0)
                start()
            }
        }

        DragHandler {
            id: tiltHandler
            minimumPointCount: 2
            maximumPointCount: 2
            target: null
            xAxis.enabled: false
            grabPermissions: PointerHandler.TakeOverForbidden
            onActiveChanged: {
                if (active) {
                    flickAnimation.stop()
                }
            }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: function(eventPoint) {
                root.handleLeftMapTap(eventPoint.position)
            }
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: function() {
                root.handleRightMapTap()
            }
        }

        MapItemView {
            id: weatherMarkers
            model: typeof weatherService !== "undefined" && weatherService !== null
                   ? weatherService.markers : []

            delegate: MapQuickItem {
                required property var modelData
                property var marker: modelData
                coordinate: QtPositioning.coordinate(marker.latitude, marker.longitude)
                anchorPoint.x: 44
                anchorPoint.y: marker.primary ? 98 : 78
                z: marker.primary ? 20 : 10

                sourceItem: Column {
                    spacing: 2

                    Rectangle {
                        width: marker.primary ? 124 : 88
                        height: marker.primary ? 94 : 74
                        radius: 14
                        color: "transparent"
                        border.width: 2
                        border.color: "#6f9bb3"

                        Column {
                            anchors.centerIn: parent
                            spacing: marker.primary ? -2 : -6

                            Image {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 46
                                height: 46
                                fillMode: Image.PreserveAspectFit
                                source: marker.iconUrl
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: marker.tempLabel
                                color: "#174762"
                                font.bold: true
                                font.pixelSize: 15
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                visible: marker.primary && marker.locationName !== ""
                                text: marker.locationName
                                color: "#174762"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 14
                        height: 14
                        rotation: 45
                        color: "transparent"
                        border.width: 2
                        border.color: "#6f9bb3"
                    }
                }
            }
        }

        Rectangle {
            visible: typeof weatherService !== "undefined"
                     && weatherService !== null
                     && weatherService.errorString !== ""
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 12
            z: 200
            radius: 10
            color: "#cc8a1f1f"
            border.width: 1
            border.color: "#e7b3b3"
            width: Math.min(parent.width - 24, weatherErrorText.implicitWidth + 24)
            height: weatherErrorText.implicitHeight + 16

            Text {
                id: weatherErrorText
                anchors.centerIn: parent
                width: parent.width - 16
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                color: "white"
                font.pixelSize: 13
                text: weatherService.errorString
            }
        }

        Rectangle {
            visible: typeof simulationFeed !== "undefined"
                     && simulationFeed !== null
                     && simulationFeed.hasData
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.leftMargin: 12
            anchors.bottomMargin: 12
            z: 220
            radius: 10
            color: "#cc13202b"
            border.width: 1
            border.color: "#508bb7"
            width: Math.max(180, simulationStatusColumn.implicitWidth + 20)
            height: simulationStatusColumn.implicitHeight + 16

            Column {
                id: simulationStatusColumn
                anchors.centerIn: parent
                spacing: 2

                Text {
                    color: "white"
                    font.bold: true
                    font.pixelSize: 13
                    text: "ns-3 " + simulationFeed.rat.toUpperCase() + " live"
                }

                Text {
                    color: "#d2e8f7"
                    font.pixelSize: 12
                    text: "t = " + simulationFeed.simTime.toFixed(1) + " s"
                }

                Text {
                    color: "#d2e8f7"
                    font.pixelSize: 12
                    text: simulationFeed.antennas.length + " antennas, "
                          + simulationFeed.uavs.length + " UAVs"
                }
            }
        }
    }
    RowLayout
    {
        id: rowLayoutZoomBar
        x: 361
        y: 445
        width: 499
        height: 24
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8

        ToolButton
        {
            id: toolButtonZoomMapOut
            text: "-"
            // tooltip: ""
            onClicked:
            {
                sliderHorizontalZoom.value = sliderHorizontalZoom.value - 0.5;
                sliderHorizontalZoom
            }
        }

        Slider
        {
            id: sliderHorizontalZoom
            x: 0
            //tickmarksEnabled: false
            stepSize: 0.2
            from: 2
            value: 3
            to: 20
            onValueChanged:
            {
                map.zoomLevel =value;
            }
        }


        ToolButton
        {
            id: toolButtonZommMapIn
            x: 200
            text: "+"
            onClicked:
            {
                sliderHorizontalZoom.value = sliderHorizontalZoom.value + 0.5;
            }
        }
        Image
        {
            id: centerImage

            width: 27
            height: 27
            fillMode: Image.PreserveAspectFit
            sourceSize.height: 27
            sourceSize.width: 27
            source: "qrc:/ico/center.ico"
            MouseArea
            {
                id: mouseareaCenterImage
                anchors.left: parent.left
                anchors.leftMargin: 0
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 0
                anchors.top: parent.top
                anchors.topMargin: 0
                anchors.right: parent.right
                hoverEnabled: true
                onEntered:
                {
                    centerImage.source = "qrc:/ico/centeri.ico";
                }
                onExited:
                {
                    centerImage.source = "qrc:/ico/center.ico";
                }
                onClicked: {
                    map.center = main_quadcopter.coordinate
                }
            }
        }

        Slider {
            id: sliderHorizontalTilt
            x: 300
            y: 0

            from: 0
            to: 60
            Layout.preferredWidth: -1
            stepSize: 0.2
            value: 4
            Layout.alignment: Qt.AlignLeft | Qt.AlignTop
            onValueChanged: {
                map.tilt=value
            }
        }
    }

    function updateUavGPS(xc,yc)
    {
        main_quadcopter.coordinate = QtPositioning.coordinate(xc,yc)
        if(xc === 0) valueLatitude = 39.904459; else valueLatitude = xc;
        if(yc === 0) valueLongitude = 116.406847; else valueLongitude = yc;

        //console.log("UavMap.updateUavGPS(xc,yc)")
    }

    function updatePointsMap()

    {
        distance=0
        var pathlength=polylinemap.pathLength();
        for (var i=0;i< pathlength;i++)
        {
            var lat1,long1,latn,longn;
            var coor1=0
            coor1=polylinemap.coordinateAt(i);
            var coorn=0
            coorn=polylinemap.coordinateAt(i+1);
            var distancee=coor1.distanceTo(coorn);
            //console.log(pathlength + "path number")
            //console.log(distancee+"  distance");
            //          lat1=coor1.latitude;
            //          long1=coor1.longitude;
            //        latn=coorn.latitude;
            //        longn=coorn.longitude
            //        signalCalculateDistance(lat1,latn,long1,longn)
            distance=distancee+distance
        }
        //console.log(distance + " momo d")
        distanceCalculated(distance)

    }

    function angleRefresh(xca){
        quadcopter_image_rot.angle=xca*180/Math.PI


    }

    function polylineAdd(x,y)
    {


        polylinemap.addCoordinate(QtPositioning.coordinate(x,y))


        //sampleModel.set(i,{"latitude":x,"longitude":y})}
        sampleModel.append({"latitude":x,"longitude":y})





        //        for(var i=0;i<=polylinemap.pathLength();i++)
        //        {
        //        listWP[i]= listWP.push(x)
        //        console.log("append WP "+ listWP)
        //        }


    }

    function polylineRemove(row)
    {
        polylinemap.removeCoordinate(row)
        indexR=row

    }


}























































/*##^## Designer {
    D{i:0;autoSize:true;height:480;width:640}D{i:5;autoSize:true;height:480;width:640}
D{i:51;anchors_height:27;anchors_width:27;anchors_x:-40}
}
 ##^##*/
