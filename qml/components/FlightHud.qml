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
    readonly property color frameColor: "#112333"
    readonly property color surfaceColor: "#d2141d28"
    readonly property color panelColor: "#c8182633"
    readonly property color panelStrongColor: "#d01b2a39"
    readonly property color accentColor: "#7ed5ff"
    readonly property color accentWarmColor: "#ffd566"
    readonly property color textPrimaryColor: "#f2f8fc"
    readonly property color textMutedColor: "#9db7c8"

    implicitWidth: collapsed ? 76 : 304
    implicitHeight: collapsed ? 76 : 316
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

    function headingTickLabel(offsetDegrees) {
        if (Math.abs(offsetDegrees) < 1) {
            return ""
        }
        const labelHeading = flightHud.wrappedHeading(headingDegrees + offsetDegrees)
        if (Math.round(labelHeading) % 90 === 0) {
            return cardinalHeading(labelHeading)
        }
        return Math.round(labelHeading).toString()
    }

    Behavior on width {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    Behavior on height {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    Rectangle {
        anchors.fill: parent
        radius: collapsed ? width / 2 : 28
        color: flightHud.surfaceColor
        border.color: flightHud.frameColor
        border.width: 1
    }

    Button {
        id: expandButton
        anchors.centerIn: parent
        width: 48
        height: 48
        visible: collapsed
        text: "HUD"
        onClicked: flightHud.collapsed = false

        background: Rectangle {
            radius: width / 2
            color: flightHud.panelStrongColor
            border.color: flightHud.accentColor
            border.width: 1
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

    ColumnLayout {
        id: hudLayout
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10
        visible: !collapsed

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "Flight HUD"
                color: flightHud.textPrimaryColor
                font.pixelSize: 16
                font.bold: true
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: 76
                Layout.preferredHeight: 28
                radius: 14
                color: flightHud.panelStrongColor
                border.color: flightHud.accentColor
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: flightHud.cardinalHeading(headingDegrees)
                    color: flightHud.accentColor
                    font.pixelSize: 13
                    font.bold: true
                }
            }

            Button {
                id: collapseButton
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
                visible: !collapsed
                text: "−"
                onClicked: flightHud.collapsed = true

                background: Rectangle {
                    radius: width / 2
                    color: flightHud.panelStrongColor
                    border.color: flightHud.frameColor
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

        Rectangle {
            id: headingRibbon
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            radius: 18
            color: flightHud.panelColor
            border.color: flightHud.frameColor
            border.width: 1
            clip: true

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: parent.height
                color: "#142434"
            }

            Repeater {
                model: [-60, -30, 0, 30, 60]

                delegate: Item {
                    width: 48
                    height: parent.height
                    x: (index * ((headingRibbon.width - width) / 4))

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.topMargin: 10
                        width: index === 2 ? 3 : 2
                        height: index === 2 ? 20 : 12
                        radius: 1
                        color: index === 2 ? flightHud.accentWarmColor : "#bfd4e3"
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 8
                        text: flightHud.headingTickLabel(modelData)
                        color: index === 2 ? flightHud.accentWarmColor : flightHud.textMutedColor
                        font.pixelSize: index === 2 ? 18 : 11
                        font.bold: true
                    }
                }
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 6
                width: 2
                height: parent.height - 12
                radius: 1
                color: flightHud.accentWarmColor
                opacity: 0.85
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 6
                width: 72
                height: 24
                radius: 12
                color: "#d0121d2a"
                border.color: flightHud.frameColor
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: Math.round(flightHud.wrappedHeading(headingDegrees)) + "°"
                    color: flightHud.textPrimaryColor
                    font.pixelSize: 14
                    font.bold: true
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Rectangle {
                id: horizonCard
                Layout.preferredWidth: 166
                Layout.preferredHeight: 154
                radius: 24
                color: flightHud.panelColor
                border.color: flightHud.frameColor
                border.width: 1
                clip: true

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 10
                    radius: 20
                    color: "#0b131b"
                    clip: true

                    Item {
                        id: horizonLayer
                        width: parent.width * 1.8
                        height: parent.height * 2.1
                        anchors.centerIn: parent
                        y: Math.max(-32, Math.min(32, pitchDegrees * 1.45))

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
                            color: "#6eb6ff"
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: parent.height / 2
                            color: "#6b4520"
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            height: 3
                            color: flightHud.accentWarmColor
                        }
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        width: 54
                        height: 4
                        radius: 2
                        color: flightHud.textPrimaryColor
                    }

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        width: 4
                        height: 22
                        radius: 2
                        color: flightHud.textPrimaryColor
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: horizonCard.Layout.preferredHeight
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 62
                    radius: 18
                    color: flightHud.panelStrongColor
                    border.color: flightHud.frameColor
                    border.width: 1

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 3

                        Text {
                            text: "Altitude"
                            color: flightHud.textMutedColor
                            font.pixelSize: 11
                            font.bold: true
                        }

                        Text {
                            text: altitudeMeters.toFixed(1) + " m"
                            color: flightHud.textPrimaryColor
                            font.pixelSize: 24
                            font.bold: true
                            elide: Text.ElideRight
                            width: parent.width
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    radius: 16
                    color: flightHud.panelColor
                    border.color: flightHud.frameColor
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Pitch  " + pitchDegrees.toFixed(1) + "°"
                        color: flightHud.textPrimaryColor
                        font.pixelSize: 14
                        font.bold: true
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    radius: 16
                    color: flightHud.panelColor
                    border.color: flightHud.frameColor
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Roll   " + rollDegrees.toFixed(1) + "°"
                        color: flightHud.textPrimaryColor
                        font.pixelSize: 14
                        font.bold: true
                    }
                }
            }
        }
    }
}
