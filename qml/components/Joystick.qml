import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: mainwindow
    objectName: "window"

    property int currentJoystick: 0
    property int currentPitch: 3
    property int currentYaw: 0
    property int currentRoll: 2
    property int currentThorttle: 1
    property real axisvalue: 0
    property int axisCount: 4
    readonly property var defaultAssignments: [0, 1, 2, 3]
    readonly property string selectedTargetLabel: typeof simulationFeed !== "undefined"
                                                  && simulationFeed !== null
                                                  && simulationFeed.hasSelectedUav
                                                  ? (simulationFeed.selectedUav.label || "--")
                                                  : "--"
    readonly property int selectedTargetSystemId: typeof simulationFeed !== "undefined"
                                                  && simulationFeed !== null
                                                  && simulationFeed.hasSelectedUav
                                                  ? Number(simulationFeed.selectedUavId) + 1
                                                  : 1

    signal axisCd(double currentAxis, double currentParameter)
    signal axisValueChanged(double currentAxis, double valueAxis)
    signal joystickProtocolChanged(int index)

    width: 448
    implicitWidth: 448
    implicitHeight: contentColumn.implicitHeight + 24
    radius: 18
    color: "#d9112333"
    border.color: "#50697f"
    border.width: 1
    clip: true

    ListModel {
        id: listparameters

        ListElement { key: "Yaw"; value: 0 }
        ListElement { key: "Throttle"; value: 1 }
        ListElement { key: "Roll"; value: 2 }
        ListElement { key: "Pitch"; value: 3 }
        ListElement { key: "None"; value: 4 }
    }

    function generateJoystickWidgets(id) {
        currentJoystick = Math.max(0, id)
        axes.model = axisCount
    }

    function assignmentIndexForAxis(axisIndex) {
        if (axisIndex >= 0 && axisIndex < defaultAssignments.length) {
            return defaultAssignments[axisIndex]
        }

        return 4
    }

    function updateAssignment(axisIndex, assignmentIndex) {
        mainwindow.axisCd(axisIndex, assignmentIndex)
        axismanager(axisIndex, assignmentIndex)
    }

    function axismanager(axisIndex, assignmentIndex) {
        switch (assignmentIndex) {
        case 0:
            currentYaw = axisIndex
            break
        case 1:
            currentThorttle = axisIndex
            break
        case 2:
            currentRoll = axisIndex
            break
        case 3:
            currentPitch = axisIndex
            break
        default:
            break
        }
    }

    Component.onCompleted: {
        generateJoystickWidgets(joysticks.currentIndex >= 0 ? joysticks.currentIndex : 0)
        joystickProtocolChanged(protocolCombo.currentIndex)
    }

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: qsTr("Manual Control")
                    color: "#f1f5f9"
                    font.pixelSize: 18
                    font.bold: true
                }

                Text {
                    text: qsTr("Target: %1 (sysid %2)").arg(selectedTargetLabel).arg(selectedTargetSystemId)
                    color: "#9ec7df"
                    font.pixelSize: 12
                    font.bold: true
                }

                Text {
                    text: QJoysticks.deviceNames.length > 0
                          ? qsTr("Input: %1").arg(QJoysticks.deviceNames[currentJoystick] || QJoysticks.deviceNames[0])
                          : qsTr("Input: Virtual joystick")
                    color: "#7fa6bd"
                    font.pixelSize: 11
                }
            }

            ComboBox {
                id: protocolCombo
                Layout.preferredWidth: 120
                model: [
                    { "key": "MAVLink", "value": 0 },
                    { "key": "DJI", "value": 1 }
                ]
                textRole: "key"

                onCurrentIndexChanged: joystickProtocolChanged(currentIndex)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ComboBox {
                id: joysticks
                Layout.fillWidth: true
                model: QJoysticks.deviceNames.length > 0
                       ? QJoysticks.deviceNames
                       : [qsTr("Virtual joystick")]

                onCurrentIndexChanged: generateJoystickWidgets(currentIndex)
            }

            Rectangle {
                Layout.preferredWidth: 122
                Layout.preferredHeight: 34
                radius: 10
                color: "#182633"
                border.color: "#345064"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: qsTr("%1 axes").arg(axisCount)
                    color: "#d7e7f2"
                    font.pixelSize: 12
                    font.bold: true
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                id: axes
                model: axisCount

                delegate: RowLayout {
                    required property int index

                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        Layout.preferredWidth: 56
                        text: qsTr("Axis %1").arg(index + 1)
                        color: "#d7e7f2"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    ProgressBar {
                        id: axisBar
                        Layout.fillWidth: true
                        Layout.preferredHeight: 22
                        from: -100
                        to: 100
                        value: 0

                        background: Rectangle {
                            implicitWidth: 220
                            implicitHeight: 22
                            color: "#e6e6e6"
                            radius: 11
                        }

                        contentItem: Item {
                            implicitWidth: 220
                            implicitHeight: 22

                            Rectangle {
                                width: axisBar.visualPosition * parent.width
                                height: parent.height
                                radius: 11
                                color: "#17a81a"
                            }
                        }

                        Behavior on value {
                            NumberAnimation {
                                duration: 120
                                easing.type: Easing.OutCubic
                            }
                        }

                        Connections {
                            target: QJoysticks

                            function onAxisChanged(js, axis, value) {
                                if (currentJoystick !== js || axis !== index) {
                                    return
                                }

                                axisBar.value = QJoysticks.getAxis(js, index) * 100
                                axisvalue = QJoysticks.getAxis(js, index) * 1000
                                mainwindow.axisValueChanged(index, axisvalue)
                            }
                        }
                    }

                    ComboBox {
                        id: axesassignements
                        Layout.preferredWidth: 148
                        textRole: "key"
                        model: listparameters
                        currentIndex: assignmentIndexForAxis(index)

                        Component.onCompleted: updateAssignment(index, currentIndex)

                        onActivated: updateAssignment(index, currentIndex)
                    }
                }
            }
        }
    }
}
