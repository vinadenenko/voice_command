import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Voice_command_test_app

Window {
    id: window
    width: 640
    height: 480
    visible: true
    title: qsTr("Voice Command Usage")

    App {
        id: app
    }

    Connections {
        target: app
        function onRequestChangeColor(color) {
            rec.color = color
        }
    }

    Button {
        onPressed: app.onButtonPressed()
        onReleased: app.onButtonReleased()
        text: app.isRecording ? "Release to Stop" : "Press and hold to talk"
    }

    Rectangle {
        id: rec
        anchors.centerIn: parent
        width: 50
        height: 50
        color: "white"
    }

    Popup {
        id: processingPopup
        modal: true
        visible: app.isProcessing
        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        padding: 20

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 10

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: true
            }

            Label {
                text: "Processing..."
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
