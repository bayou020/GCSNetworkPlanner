import QtQuick
import QtQml
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: udpConnect
    width: 260
    height: 220
    opacity: 1

    signal newDdsArgumentsDJI(var args)
    signal newDdsArgumentsMAV(var args)

    Rectangle {
        anchors.fill: parent
        color: "#80ffffff"
        radius: 8
        border.color: "#3a4e57"
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true

            ToolButton {
                text: "X"
                onClicked: udpConnect.visible = false
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("UAV Selection")
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 15
                font.bold: true
            }

            Item {
                Layout.preferredWidth: 24
            }
        }

        Label {
            text: qsTr("UAV Name")
        }

        TextField {
            id: textFieldUavName
            Layout.fillWidth: true
            text: "UAV_MAV"
            validator: RegularExpressionValidator {
                regularExpression: /[0-9_A-Za-z]{1,10}/
            }
        }

        Label {
            text: qsTr("UAV ID")
        }

        TextField {
            id: textFieldDomainID
            Layout.fillWidth: true
            text: "0"
            inputMethodHints: Qt.ImhDigitsOnly
            validator: RegularExpressionValidator {
                regularExpression: /[0-9]{1,3}/
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Button {
                id: vehicleTypeButton
                Layout.preferredWidth: 72
                checkable: true
                text: checked ? qsTr("MAV") : qsTr("DJI")
            }

            Button {
                id: buttonConnect
                Layout.fillWidth: true
                text: qsTr("Connect")
                onClicked: {
                    var argumentsList = [
                        textFieldUavName.text,
                        textFieldDomainID.text,
                        textFieldUavName.text
                    ]

                    if (buttonConnect.text !== "Connect" && buttonConnect.text !== "Reconnect")
                        return

                    if (vehicleTypeButton.text === "MAV")
                        udpConnect.newDdsArgumentsMAV(argumentsList)
                    else
                        udpConnect.newDdsArgumentsDJI(argumentsList)
                }
            }
        }
    }

    function connectionStatusUpdate(status) {
        vehicleTypeButton.visible = status === "Reconnect" || status === "Connect"
        buttonConnect.text = status
    }

    function getUavType() {
        return vehicleTypeButton.text
    }

    function getConnectionStatus() {
        return buttonConnect.text
    }
}
