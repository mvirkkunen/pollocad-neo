import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Layouts
import pollocadgui

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

            text: loadedCode
            highlightedSpans: occtView.hoveredSpans

            onCodeChanged: {
                executor.execute(code.text);
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
                    id: occtView
                    anchors.fill: parent
                    hoveredPosition: code.hoveredPosition
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

                    visible: shapeOutOfDate
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4

                    border.color: "#f00"
                    border.width: 4
                    color: "transparent"

                    visible: shapeOutOfDate
                }

                Text {
                    x: 50
                    y: 50
                }
            }

            ColumnLayout {
                RowLayout {
                    Switch {
                        text: "Show '#' highlights"
                        checked: occtView.showHighlightedShapes
                        onClicked: {
                            occtView.showHighlightedShapes = checked;
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true

                    height: 1
                    color: "#000"
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

    Component.onCompleted: {
        executor.execute(code.text);
    }

    Connections {
        target: executor

        function onResult(res) {
            occtView.setResult(res);
            code.setResult(res);
            messages.model = res.messagesModel();
            shapeOutOfDate = !res.hasShapes;
        }
    }
}
