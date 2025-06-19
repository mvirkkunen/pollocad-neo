import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts
import pollocad

ApplicationWindow {
    property bool shapeOutOfDate: false

    id: window
    width: 1800
    height: 900
    visible: true
    title: qsTr("pollocad NEO")

    menuBar: MenuBar {
        Menu {
            title: "&File"
            Action { text: "E&xit"; onTriggered: window.close() }
        }
        Menu {
            title: "&View"
        }
    }

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
combine() {
  fillet("z", 5) move([-25, -25]) box([50, 50, 2]);
  tag("remove") # {
    cyl(r=5, h=4);

    for (a = [45 : 90 : 315]) {
      rot(z=a) move(x=22) cyl(r=3, h=2);
    }
  }
}
`.trim()

            onCodeChanged: {
                if (!typingTimeout.running) {
                    typingTimeout.restart();
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
                SplitView.preferredHeight: 700

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

            ColumnLayout {
                RowLayout {
                    Switch {
                        text: "Show '#' highlights"
                        checked: viewer.showHighlightedShapes
                        onClicked: {
                            viewer.showHighlightedShapes = checked;
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    background: Rectangle {
                        color: "#ddd"
                    }

                    ListView {
                        id: messages
                        clip: true
                        spacing: 2

                        delegate: Rectangle {
                            width: parent.width
                            height: text.implicitHeight

                            Text {
                                id: text
                                color: level === "error" ? "#800" : level === "warning" ? "#880" : "#000"
                                text: message
                                clip: true
                                padding: 8
                                font.family: "monospace"
                            }

                            MouseArea {
                                id: ma
                                anchors.fill: parent

                                onClicked: {
                                    code.cursorPosition = location;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Shortcut {
        sequences: [StandardKey.Refresh]
        onActivated: {
            typingTimeout.stop();
            executor.execute(code.text);
        }
    }

    Component.onCompleted: {
        highlighter.setTextDocument(code.textDocument);
        executor.execute(code.text);
    }

    Connections {
        target: executor

        function onResult(res) {
            viewer.setResult(res);
            highlighter.setResult(res);
            messages.model = res.messagesModel();
            shapeOutOfDate = !res.hasShapes;
        }
    }

    Timer {
        id: typingTimeout
        interval: 1000
        onTriggered: executor.execute(code.text);
    }
}
