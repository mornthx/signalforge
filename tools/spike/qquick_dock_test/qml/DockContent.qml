import QtQuick

Item {
    id: root

    property string dockId: "unknown"
    property string dockState: "docked"

    signal contextMenuRequested(point position)

    Rectangle {
        anchors.fill: parent
        color: "#202830"
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        property real phase: 0.0

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.strokeStyle = "#4fd1c5"
            ctx.lineWidth = 2
            ctx.beginPath()
            for (var x = 0; x < width; x++) {
                var y = height / 2 + Math.sin((x * 0.05) + phase) * height * 0.25
                if (x === 0) {
                    ctx.moveTo(x, y)
                } else {
                    ctx.lineTo(x, y)
                }
            }
            ctx.stroke()
        }

        Timer {
            interval: 33  // ~30 Hz
            repeat: true
            running: true
            onTriggered: {
                canvas.phase += 0.12
                canvas.requestPaint()
            }
        }
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 8
        color: "#e2e8f0"
        font.pointSize: 11
        text: root.dockId + " — " + root.dockState
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: function(mouse) {
            root.contextMenuRequested(Qt.point(mouse.x, mouse.y))
        }
    }
}
