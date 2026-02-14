import QtQuick
import QtQuick.Controls

import Voice_command_test_app

Window {
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
            // text: "Push to Talk"
            // anchors.centerIn: parent
            onPressed: app.onButtonPressed()
            onReleased: app.onButtonReleased()

            // Optional: Change button text when it's pressed
            text: app.isRecording ? "Release to Stop" : "Press and hold to talk"
        }

    Rectangle {
        id: rec
        anchors.centerIn: parent
        width: 50
        height: 50
        color: "white"
    }

}
