import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: videoMonitor

    property bool hasFrame: false
    property string frameSource: ""
    property string uavLabel: "--"
    property string resolution: "--"
    property double receiveFps: 0
    property int droppedFrames: 0
    property int receivedFrames: 0
    property int frameAgeMs: -1

    readonly property var metricTiles: [
        {"name": "FPS", "value": hasFrame ? receiveFps.toFixed(1) : "--"},
        {"name": "RES", "value": resolution},
        {"name": "DROP", "value": hasFrame ? droppedFrames.toString() : "--"},
        {"name": "AGE", "value": hasFrame && frameAgeMs >= 0 ? frameAgeMs + " ms" : "--"},
        {"name": "RX", "value": hasFrame ? receivedFrames.toString() : "--"},
        {"name": "STATE", "value": hasFrame ? "DECODING" : "IDLE"}
    ]

    width: 440
    implicitWidth: 440
    implicitHeight: 328
    clip: true

    Rectangle {
        anchors.fill: parent
        radius: 18
        color: "#d2141d28"
        border.color: "#112333"
        border.width: 2
        clip: true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "Video"
                color: "#f2f8fc"
                font.pixelSize: 16
                font.bold: true
            }

            Rectangle {
                radius: 12
                color: "#19344c"
                border.color: "#7ed5ff"
                border.width: 1
                Layout.preferredHeight: 24
                Layout.preferredWidth: 96

                Text {
                    anchors.centerIn: parent
                    text: uavLabel
                    color: "#7ed5ff"
                    font.pixelSize: 12
                    font.bold: true
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: hasFrame ? "LIVE" : "WAITING"
                color: hasFrame ? "#7dff95" : "#ffd566"
                font.pixelSize: 12
                font.bold: true
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 188
            radius: 16
            color: "#0d1823"
            border.color: "#112333"
            border.width: 1
            clip: true

            Image {
                anchors.fill: parent
                anchors.margins: 1
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: true
                cache: false
                retainWhileLoading: true
                source: frameSource
                visible: hasFrame
            }

            Column {
                anchors.centerIn: parent
                spacing: 6
                visible: !hasFrame

                Text {
                    text: "No simulated video yet"
                    color: "#f2f8fc"
                    font.pixelSize: 14
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    text: "Start the live ns-3 + RPi wrapper"
                    color: "#9db7c8"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 3
            rowSpacing: 8
            columnSpacing: 8

            Repeater {
                model: videoMonitor.metricTiles

                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    radius: 10
                    color: "#152331"
                    border.color: "#1f3346"
                    border.width: 1
                    clip: true

                    Column {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 1

                        Text {
                            text: modelData.name
                            color: "#9db7c8"
                            font.pixelSize: 10
                            font.bold: true
                        }

                        Text {
                            width: parent.width
                            text: modelData.value
                            color: "#f2f8fc"
                            font.pixelSize: 12
                            font.bold: true
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }
}
