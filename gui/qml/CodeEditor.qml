import QtQuick
import QtQuick.Controls

Item {
    property alias text: code.text
    property alias textDocument: code.textDocument
    property alias cursorPosition: code.cursorPosition
    property alias highlightedSpans: decorator.highlightedSpans
    property int hoveredPosition: -1
    property int lineNumberWidth: hiddenLineNumber.width * Math.max(Math.ceil(Math.log(code.lineCount + 1) / Math.LN10), 4)

    property string prevText: ""

    signal codeChanged()

    id: root

    Flickable {
        id: flickable
        anchors.fill: parent

        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.horizontal: ScrollBar {
            policy: ScrollBar.AlwaysOn
            minimumSize: 12
        }
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AlwaysOn
            minimumSize: 12

            contentItem: Rectangle {
                implicitWidth: 12
                radius: width / 2
                //color: "#000"

                // Animate the changes in opacity (default duration is 250 ms).
                Behavior on opacity {
                    NumberAnimation {}
                }
            }
        }

        Text {
            id: hiddenLineNumber
            text: "0"
            font: code.font
            visible: false
        }

        Rectangle {
            x: flickable.contentX - 1
            y: -1
            width: lineNumberWidth + 10
            height: code.height + 2

            border.width: 1
            border.color: "#ccc"
            color: "#ddd"
        }

        ListView {
            anchors.left: parent.left
            anchors.leftMargin: 2 + flickable.contentX
            anchors.top: parent.top
            anchors.topMargin: code.topPadding
            width: 50
            height: code.height
            model: code.lineCount
            reuseItems: true

            delegate: Text {
                width: lineNumberWidth
                text: String(modelData + 1)
                horizontalAlignment: Text.AlignRight
                font: code.font
                color: "#999"
            }
        }

        TextArea.flickable: TextArea {
            id: code
            leftPadding: lineNumberWidth + 20
            font.family: "Monospace"

            MouseArea {
                anchors.fill: parent
                z: -1
                hoverEnabled: true
                cursorShape: undefined

                onPositionChanged: (ev) => root.hoveredPosition = code.positionAt(ev.x, ev.y);
            }

            onTextChanged: {
                if (code.text !== prevText) {
                    prevText = code.text;
                    typingTimeout.restart();
                }
            }

            Keys.onPressed: (ev) => {
                switch (ev.key) {
                    case Qt.Key_Up: {
                        if (ev.modifiers & Qt.AltModifier) {
                            decorator.adjustNumber(+1);
                            updateNow();
                            ev.accepted = true;
                        }
                        break;
                    }

                    case Qt.Key_Down: {
                        if (ev.modifiers & Qt.AltModifier) {
                            decorator.adjustNumber(-1);
                            updateNow();
                            ev.accepted = true;
                        }
                        break;
                    }

                    case Qt.Key_Return:
                        ev.accepted = decorator.handleReturn();
                        break;

                    case Qt.Key_Backspace:
                        ev.accepted = decorator.handleBackspace();
                        break;

                    case Qt.Key_Tab:
                        ev.accepted = decorator.handleTab(ev.modifiers & Qt.ShiftModifier ? -1 : 1);
                        break;

                    case Qt.Key_Backtab:
                        ev.accepted = decorator.handleTab(-1);
                        break;
                    
                    case Qt.Key_Home:
                        code.cursorPosition = decorator.handleHome();
                        ev.accepted = true;
                        break;
                }
            }
        }
    }

    function setResult(result) {
        decorator.setResult(result);
    }

    function updateNow() {
        typingTimeout.stop();
        codeChanged();
    }

    CodeDecorator {
        id: decorator
        textDocument: code.textDocument
        cursorPosition: code.cursorPosition
        selectionStart: code.selectionStart
        selectionEnd: code.selectionEnd
    }

    Shortcut {
        sequences: [StandardKey.Refresh]
        onActivated: updateNow();
    }

    Timer {
        id: typingTimeout
        interval: 1000
        onTriggered: codeChanged();
    }
}
