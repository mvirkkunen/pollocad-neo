import QtQuick
import QtQuick.Controls
import pollocad

Window {
    property bool pendingExecute: false
    property bool shapeOutOfDate: false

    width: 1800
    height: 900
    visible: true
    title: qsTr("pollocad NEO")

    SplitView {
        id: hSplit
        anchors.fill: parent

        handle: Rectangle {
            id: hHandle
            implicitWidth: 1
            color: SplitHandle.pressed ? "#aaa" : "#000"

            containmentMask: Item {
                x: (hHandle.width - width) / 2
                width: 12
                height: hSplit.height
            }
        }

        CodeEditor {
            id: code
            SplitView.preferredWidth: 600

            text: `
move([10, 0, 0]) {
    box([50,10,10]);
    box([100, 100, 1]);
}


move([0, 0, 50]) {
    repeat(25) move([25 * floor($i / 10), 25 * ($i % 10)]) cyl(r=10, h=50);
}

def thin_cyl(x, y, height) {
    move([x, y]) cyl(r=1, h=height);
}

thin_cyl(100, 100, 100);
thin_cyl(100, 110, 50);

`.trim()

            onCodeChanged: {
                typingTimeout.restart();

                if (executor.isBusy) {
                    pendingExecute = true;
                } else {
                    pendingExecute = false;
                    executor.execute(code.text);
                }
            }
        }

        SplitView {
            id: vSplit
            //height: hSplit.height
            orientation: Qt.Vertical

            handle: Rectangle {
                id: vHandle
                implicitHeight: 1
                color: SplitHandle.pressed ? "#aaa" : "#000"

                containmentMask: Item {
                    y: (vHandle.height - height) / 2
                    width: vSplit.width
                    height: 12
                }
            }

            Item {
                SplitView.preferredHeight: 800

                OcctView {
                    id: viewer
                    anchors.fill: parent
                }

                BusyIndicator {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.top: parent.top
                    anchors.topMargin: 10

                    running: executor.isBusy
                }

                Rectangle {
                    anchors.fill: parent

                    border.color: "#fff"
                    border.width: 12
                    color: "transparent"

                    visible: shapeOutOfDate && !typingTimeout.running
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4

                    border.color: "#f00"
                    border.width: 4
                    color: "transparent"

                    visible: shapeOutOfDate && !typingTimeout.running
                }
            }

           ScrollView {
                background: Rectangle {
                    color: "#ddd"
                }

                ListView {
                    id: messages
                    clip: true
                    spacing: 2

                    delegate: Rectangle {
                        width: messages.width
                        height: text.implicitHeight
                        color: "#fff"

                        MouseArea {
                            anchors.fill: parent

                            onClicked: {
                                code.cursorPosition = location;
                            }
                        }

                        Text {
                            id: text
                            text: message
                            clip: true
                            font.family: "monospace"
                        }
                    }
                }
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Refresh]
        onActivated: {
            pendingExecute = false;
            executor.execute(code.text);
        }
    }

    Component.onCompleted: {
        executor.execute(code.text);
    }

    Connections {
        target: executor

        function onResult(res) {
            viewer.setResult(res);
            messages.model = res.messages();
            shapeOutOfDate = !res.hasShape;
        }

        function onIsBusyChanged() {
            if (pendingExecute && !executor.isBusy) {
                pendingExecute = false;
                executor.execute(code.text);
            }
        }
    }

    Timer {
        id: typingTimeout
        interval: 1000
    }
}
