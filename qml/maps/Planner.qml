import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: planner
    width: 260
    height: 320

    property double count: 0
    property double mouseLatitude: 0
    property double mouseLongitude: 0
    property int libRow: -1
    property double distance: 0
    property double distanceT: 0
    property var listWpLatitude: []
    property var listWpLongitude: []

    signal polylineCoordinates(double x, double y)
    signal polylineRemove(int row)
    signal sendSignalLatitude(var latitude)
    signal sendSignalLongitude(var longitude)
    signal distanceUpdate()
    signal locatSend(var position)

    ListModel {
        id: libraryModel
    }

    Rectangle {
        anchors.fill: parent
        color: "#dff5fb"
        border.color: "#143f54"
        border.width: 1
        radius: 14
        opacity: 0.92
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                text: qsTr("Mission Planner")
                font.pixelSize: 16
                font.bold: true
            }

            Label {
                text: libraryModel.count + qsTr(" pts")
                font.pixelSize: 12
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            radius: 10
            border.color: "#9cb8c7"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Latitude")
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Longitude")
                        font.bold: true
                    }
                }

                ListView {
                    id: waypointList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: libraryModel
                    spacing: 2

                    delegate: Rectangle {
                        width: waypointList.width
                        height: 32
                        radius: 8
                        color: planner.libRow === index ? "#b8e1f1" : "transparent"
                        border.color: planner.libRow === index ? "#1e6b8a" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 8

                            Label {
                                Layout.fillWidth: true
                                text: Number(latitude).toFixed(6)
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: Number(longitude).toFixed(6)
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: planner.libRow = index
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                Layout.fillWidth: true
                text: qsTr("Remove")
                enabled: planner.libRow >= 0 && planner.libRow < libraryModel.count
                onClicked: {
                    if (planner.libRow < 0 || planner.libRow >= libraryModel.count)
                        return

                    libraryModel.remove(planner.libRow, 1)
                    polylineRemove(planner.libRow)
                    removeWPS()
                    distanceUpdate()
                    planner.libRow = Math.min(planner.libRow, libraryModel.count - 1)
                }
            }

            Button {
                Layout.fillWidth: true
                text: qsTr("Set Mission")
                enabled: libraryModel.count > 0
                onClicked: {
                    sendSignalLatitude(listWpLatitude)
                    sendSignalLongitude(listWpLongitude)
                }
            }
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: (distanceT / 1000).toFixed(2) + " km"
            font.pixelSize: 22
            font.bold: true
        }
    }

    function addCoordinate(x, y) {
        if (!visible)
            return

        libraryModel.append({"latitude": x, "longitude": y})
        polylineCoordinates(x, y)
        sendWPS()
    }

    function sendWPS() {
        var listWPLatitude = []
        var listWPLongitude = []
        for (var i = 0; i < libraryModel.count; ++i) {
            listWPLatitude.push(libraryModel.get(i).latitude)
            listWPLongitude.push(libraryModel.get(i).longitude)
        }

        listWpLatitude = listWPLatitude
        listWpLongitude = listWPLongitude
    }

    function removeWPS() {
        if (planner.libRow < 0)
            return

        listWpLatitude.splice(planner.libRow, 1)
        listWpLongitude.splice(planner.libRow, 1)
    }

    function receiveDistance(distancee) {
        distanceT = distancee
    }
}
