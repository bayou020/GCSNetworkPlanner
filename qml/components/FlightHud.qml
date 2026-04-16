import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: flightHud

    property real headingDegrees: 0
    property real pitchDegrees: 0
    property real rollDegrees: 0
    property real altitudeMeters: 0
    property bool collapsed: false

    readonly property color shellColor: "#07120d"
    readonly property color panelColor: "#091611"
    readonly property color panelEdgeColor: "#163323"
    readonly property color accentColor: "#53f36c"
    readonly property color accentSoftColor: "#34a852"
    readonly property color textPrimaryColor: "#dfffe2"
    readonly property color textMutedColor: "#7ab88a"
    readonly property real compactSize: 78
    readonly property real expandedWidth: 462
    readonly property real expandedHeight: 326
    readonly property real shellPadding: 16
    readonly property real metricCardWidth: 102
    readonly property real metricCardHeight: 72
    readonly property real clusterSize: 206
    readonly property real clusterFooterHeight: 28

    implicitWidth: collapsed ? compactSize : expandedWidth
    implicitHeight: collapsed ? compactSize : expandedHeight
    width: implicitWidth
    height: implicitHeight
    z: 60

    function wrappedHeading(value) {
        const normalized = Number(value)
        if (!isFinite(normalized)) {
            return 0
        }
        return ((normalized % 360) + 360) % 360
    }

    function cardinalHeading(value) {
        const labels = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
        return labels[Math.round(flightHud.wrappedHeading(value) / 45) % labels.length]
    }

    function pitchMarkerModel() {
        return [-30, -20, -10, 0, 10, 20, 30]
    }

    function clampedPitchOffset() {
        return Math.max(-44, Math.min(44, pitchDegrees * 2.05))
    }

    Behavior on width {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    Behavior on height {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.fill: parent
        radius: collapsed ? width / 2 : 26
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.rgba(12 / 255, 22 / 255, 17 / 255, collapsed ? 0.9 : 0.94) }
            GradientStop { position: 1.0; color: Qt.rgba(6 / 255, 14 / 255, 11 / 255, collapsed ? 0.88 : 0.9) }
        }
        border.color: "#1c3b29"
        border.width: 2
    }

    Button {
        id: expandButton
        anchors.centerIn: parent
        visible: collapsed
        width: 54
        height: 54
        text: "HUD"
        onClicked: flightHud.collapsed = false

        background: Rectangle {
            radius: width / 2
            color: "#0d1e15"
            border.color: flightHud.accentColor
            border.width: 1.5
        }

        contentItem: Text {
            text: expandButton.text
            color: flightHud.textPrimaryColor
            font.pixelSize: 12
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    Item {
        anchors.fill: parent
        visible: !collapsed

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: flightHud.shellPadding
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "FLIGHT HUD"
                        color: flightHud.textPrimaryColor
                        font.pixelSize: 21
                        font.bold: true
                    }

                    Text {
                        text: "ATTITUDE AND HEADING"
                        color: flightHud.textMutedColor
                        font.pixelSize: 11
                        font.bold: true
                        font.letterSpacing: 1.6
                    }
                }

                Rectangle {
                    id: headingChip
                    Layout.preferredWidth: 154
                    Layout.preferredHeight: 40
                    radius: 20
                    color: "#08140f"
                    border.color: flightHud.accentColor
                    border.width: 1.6

                    Row {
                        anchors.centerIn: parent
                        spacing: 8

                        Text {
                            text: flightHud.cardinalHeading(headingDegrees)
                            color: flightHud.accentColor
                            font.pixelSize: 22
                            font.bold: true
                        }

                        Text {
                            text: Math.round(flightHud.wrappedHeading(headingDegrees)) + "°"
                            color: flightHud.textPrimaryColor
                            font.pixelSize: 18
                            font.bold: true
                        }
                    }
                }

                Button {
                    id: collapseButton
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    text: "−"
                    onClicked: flightHud.collapsed = true

                    background: Rectangle {
                        radius: width / 2
                        color: "#0d1e15"
                        border.color: flightHud.panelEdgeColor
                        border.width: 1
                    }

                    contentItem: Text {
                        text: collapseButton.text
                        color: flightHud.textPrimaryColor
                        font.pixelSize: 17
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 14

                ColumnLayout {
                    Layout.preferredWidth: flightHud.metricCardWidth
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: flightHud.metricCardWidth
                        Layout.preferredHeight: flightHud.metricCardHeight
                        radius: 14
                        color: "#07110d"
                        border.color: flightHud.panelEdgeColor
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

                            Text {
                                text: "ALTITUDE"
                                color: flightHud.textMutedColor
                                font.pixelSize: 10
                                font.bold: true
                                font.letterSpacing: 1.2
                            }

                            Text {
                                text: altitudeMeters.toFixed(1)
                                color: flightHud.textPrimaryColor
                                font.pixelSize: 29
                                font.bold: true
                            }

                            Text {
                                text: "meters AGL"
                                color: flightHud.textMutedColor
                                font.pixelSize: 10
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: flightHud.metricCardWidth
                        Layout.preferredHeight: flightHud.metricCardHeight
                        radius: 14
                        color: "#07110d"
                        border.color: flightHud.panelEdgeColor
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

                            Text {
                                text: "HEADING"
                                color: flightHud.textMutedColor
                                font.pixelSize: 10
                                font.bold: true
                                font.letterSpacing: 1.2
                            }

                            Text {
                                text: Math.round(flightHud.wrappedHeading(headingDegrees)) + "°"
                                color: flightHud.accentColor
                                font.pixelSize: 29
                                font.bold: true
                            }

                            Text {
                                text: flightHud.cardinalHeading(headingDegrees)
                                color: flightHud.textPrimaryColor
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }
                    }
                }

                Item {
                    id: attitudeCluster
                    Layout.preferredWidth: flightHud.clusterSize
                    Layout.preferredHeight: flightHud.clusterSize + flightHud.clusterFooterHeight
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter

                    Rectangle {
                        id: attitudeDial
                        anchors.fill: parent
                        anchors.bottomMargin: flightHud.clusterFooterHeight
                        radius: width / 2
                        color: "#07120d"
                        border.color: "#1f412d"
                        border.width: 3
                    }

                    Rectangle {
                        anchors.centerIn: attitudeDial
                        width: attitudeDial.width - 18
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.color: Qt.rgba(83 / 255, 243 / 255, 108 / 255, 0.16)
                        border.width: 1
                    }

                    Rectangle {
                        id: innerMask
                        anchors.centerIn: attitudeDial
                        width: attitudeDial.width - 34
                        height: width
                        color: "transparent"
                        clip: true

                        Item {
                            id: horizonLayer
                            width: innerMask.width * 1.9
                            height: innerMask.height * 2.15
                            anchors.centerIn: parent
                            y: flightHud.clampedPitchOffset()

                            transform: Rotation {
                                origin.x: horizonLayer.width / 2
                                origin.y: horizonLayer.height / 2
                                angle: -rollDegrees
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                height: parent.height / 2
                                color: "#123f2f"
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: parent.height / 2
                                color: "#4c3410"
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                height: 2
                                color: flightHud.accentColor
                            }

                            Repeater {
                                model: flightHud.pitchMarkerModel()

                                delegate: Item {
                                    required property int modelData
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 102
                                    height: 18
                                    y: parent.height / 2 - height / 2 - modelData * 3.0

                                    Row {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 8

                                        Text {
                                            visible: modelData !== 0
                                            text: Math.abs(modelData)
                                            color: "#c8f7d0"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }

                                        Rectangle {
                                            width: modelData === 0 ? 70 : 46
                                            height: modelData === 0 ? 2 : 1
                                            color: modelData === 0 ? flightHud.accentColor : "#b3dcb9"
                                        }

                                        Text {
                                            visible: modelData !== 0
                                            text: Math.abs(modelData)
                                            color: "#c8f7d0"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }
                                }
                            }
                        }

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            width: 90
                            height: 4
                            radius: 2
                            color: "#ebffef"
                        }

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            width: 4
                            height: 30
                            radius: 2
                            color: "#ebffef"
                        }
                    }

                    Canvas {
                        id: circularViewportMask
                        anchors.centerIn: innerMask
                        width: innerMask.width
                        height: innerMask.height
                        z: 2
                        antialiasing: true

                        onPaint: {
                            const ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            ctx.fillStyle = "rgba(7, 18, 13, 0.68)"
                            ctx.fillRect(0, 0, width, height)
                            ctx.globalCompositeOperation = "destination-out"
                            ctx.beginPath()
                            ctx.arc(width / 2, height / 2, Math.max(0, width / 2 - 3), 0, Math.PI * 2)
                            ctx.closePath()
                            ctx.fill()
                            ctx.globalCompositeOperation = "source-over"
                        }

                        Component.onCompleted: requestPaint()
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                    }

                    Rectangle {
                        anchors.centerIn: innerMask
                        width: innerMask.width
                        height: innerMask.height
                        radius: width / 2
                        z: 3
                        color: "transparent"
                        border.color: "#163323"
                        border.width: 1.5
                    }

                    Repeater {
                        model: [
                            {"label": "N", "x": 0.5, "y": 0.08},
                            {"label": "E", "x": 0.86, "y": 0.5},
                            {"label": "S", "x": 0.5, "y": 0.88},
                            {"label": "W", "x": 0.14, "y": 0.5}
                        ]

                        delegate: Text {
                            required property var modelData
                            text: modelData.label
                            color: flightHud.textMutedColor
                            font.pixelSize: 12
                            font.bold: true
                            x: attitudeDial.x + attitudeDial.width * modelData.x - width / 2
                            y: attitudeDial.y + attitudeDial.height * modelData.y - height / 2
                        }
                    }

                    Rectangle {
                        anchors.horizontalCenter: attitudeDial.horizontalCenter
                        anchors.top: attitudeDial.top
                        anchors.topMargin: 8
                        width: 24
                        height: 8
                        radius: 4
                        color: flightHud.accentColor
                    }

                    Rectangle {
                        anchors.horizontalCenter: attitudeDial.horizontalCenter
                        anchors.top: attitudeDial.bottom
                        anchors.topMargin: 6
                        width: 120
                        height: 24
                        radius: 12
                        color: "#09140f"
                        border.color: flightHud.panelEdgeColor
                        border.width: 1

                        Row {
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                text: "HDG"
                                color: flightHud.textMutedColor
                                font.pixelSize: 9
                                font.bold: true
                                font.letterSpacing: 1.1
                            }

                            Text {
                                text: Math.round(flightHud.wrappedHeading(headingDegrees)) + "°"
                                color: flightHud.accentColor
                                font.pixelSize: 15
                                font.bold: true
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: flightHud.metricCardWidth
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: flightHud.metricCardWidth
                        Layout.preferredHeight: flightHud.metricCardHeight
                        radius: 14
                        color: "#07110d"
                        border.color: flightHud.panelEdgeColor
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

                            Text {
                                text: "PITCH"
                                color: flightHud.textMutedColor
                                font.pixelSize: 10
                                font.bold: true
                                font.letterSpacing: 1.2
                            }

                            Text {
                                text: pitchDegrees.toFixed(1) + "°"
                                color: flightHud.textPrimaryColor
                                font.pixelSize: 29
                                font.bold: true
                            }

                            Text {
                                text: "nose attitude"
                                color: flightHud.textMutedColor
                                font.pixelSize: 10
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: flightHud.metricCardWidth
                        Layout.preferredHeight: flightHud.metricCardHeight
                        radius: 14
                        color: "#07110d"
                        border.color: flightHud.panelEdgeColor
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

                            Text {
                                text: "ROLL"
                                color: flightHud.textMutedColor
                                font.pixelSize: 10
                                font.bold: true
                                font.letterSpacing: 1.2
                            }

                            Text {
                                text: rollDegrees.toFixed(1) + "°"
                                color: flightHud.textPrimaryColor
                                font.pixelSize: 29
                                font.bold: true
                            }

                            Text {
                                text: "bank angle"
                                color: flightHud.textMutedColor
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }
        }
    }
}
