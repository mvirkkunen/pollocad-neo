import QtQuick
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts
import pollocadgui

ApplicationWindow {
    property bool shapeOutOfDate: false

    id: window
    width: 1800
    height: 900
    visible: true
    title: (code.textDocument.modified ? "* " : "") + qsTr("pollocad NEO")

    menuBar: MenuBar {
        Menu {
            title: "&File"
            Action { text: "&Open..."; onTriggered: window.openFile(); }
            MenuSeparator {}
            Action { text: "&Save"; onTriggered: window.saveFile(); }
            Action { text: "S&ave As"; onTriggered: saveAsDialog.open(); }
            Action { text: "&Export..."; onTriggered: exportDialog.open() }
            MenuSeparator {}
            Action { text: "E&xit"; onTriggered: window.close() }
        }
        /*Menu {
            title: "&View"
        }*/
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

    MessageDialog {
        id: confirmOpenDialog
        text: "The file has been modified"
        informativeText: "Discard changes?"
        buttons: MessageDialog.Ok | MessageDialog.Cancel
        onAccepted: {
            code.textDocument.modified = false;
            window.openFile();
        }
    }

    FileDialog {
        id: openDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["PolloCad files (*.pc)", "Text files (*.txt)", "All files (*)"]
        onAccepted: code.textDocument.source = selectedFile;
    }

    FileDialog {
        id: saveAsDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["PolloCad files (*.pc)"]
        defaultSuffix: ".pc"
        onAccepted: code.textDocument.saveAs(selectedFile);
    }

    FileDialog {
        id: exportDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["STEP file (*.step)"]
        defaultSuffix: ".step"
        onAccepted: occtView.exportResult(selectedFile);
    }

    Component.onCompleted: {
        if (String(fileToLoad)) {
            code.textDocument.source = fileToLoad;
        } else {
            code.text = "pollo();\n";
            executor.execute(code.text);
        }
    }

    Connections {
        target: code.textDocument

        function onStatusChanged() {

        }
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

    Shortcut {
        sequences: [StandardKey.Save]
        onActivated: window.saveFile();
    }

    function openFile() {
        if (code.textDocument.modified) {
            confirmOpenDialog.open();
        } else {
            openDialog.open();
        }
    }

    function saveFile() {
        if (code.textDocument.source) {
            code.textDocument.save();
        } else {
            saveAsDialog.open();
        }
    }
}
