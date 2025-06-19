import QtQuick
import QtQuick.Controls

Item {
    property alias text: code.text
    property alias textDocument: code.textDocument
    property alias cursorPosition: code.cursorPosition
    property int lineNumberWidth: hiddenLineNumber.width * Math.max(Math.ceil(Math.log(code.lineCount + 1) / Math.LN10), 4)

    property string prevText: ""

    signal codeChanged()

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
            leftPadding: lineNumberWidth + 20
            id: code

            font.family: "Monospace"

            MouseArea {
                anchors.fill: parent
                z: -1
                hoverEnabled: true

                onPositionChanged: (ev) => {
                    const pos = code.positionAt(ev.x, ev.y);
                    viewer.setHoveredPosition(pos);
                }
            }

            onTextChanged: {
                if (code.text !== prevText) {
                    prevText = code.text;
                    codeChanged();
                }
            }

            Keys.onPressed: (ev) => {
                if (ev.modifiers & Qt.AltModifier) {
                    if (ev.key === Qt.Key_Up) {
                        adjustNumber(+1);
                        ev.accepted = true;
                    } else if (ev.key === Qt.Key_Down) {
                        adjustNumber(-1);
                        ev.accepted = true;
                    }
                }

                if (ev.key === Qt.Key_Return) {
                    returnIndent();
                    ev.accepted = true;
                }

                /*if (ev.key === Qt.Key_Backspace) {
                    backspaceIndent();
                    ev.accepted = true;
                }*/

                if (ev.key === Qt.Key_Tab) {
                    tabIndent(ev.modifiers & Qt.ShiftModifier);
                    ev.accepted = true;
                }
            }

            function adjustNumber(delta) {
                const text = code.text;
                const cursor = code.cursorPosition;

                let start = cursor - 1;
                for (; start >= 0; start--) {
                    if (!text.charAt(start).match(/[0-9.-]/)) {
                        break;
                    }
                }

                if (start > 0) {
                    start++;
                }

                if (start === cursor) {
                    return;
                }

                let num = text.substring(start, cursor);

                let dot = num.indexOf(".");
                if (dot !== -1) {
                    dot = num.length - dot;
                    num = num.substring(0, num.length - dot) + num.substring(num.length - dot + 1);
                }

                num = parseInt(num) + delta;

                let idx = (num < 0) ? 1 : 0;

                num = num.toString();

                if (dot !== -1) {
                    while (num.length < dot + idx) {
                        num = num.substr(0, idx) + "0" + num.substr(idx);
                    }

                    num = num.substr(0, num.length - dot + 1) + "." + num.substr(num.length - dot + 1);
                }

                code.remove(start, cursor);
                code.insert(start, num);

                textChanged();
            }

            function returnIndent() {
                let startOfLine = code.text.lastIndexOf("\n", code.selectionStart - 1) + 1;
                console.log("super pollo", startOfLine);

                const spaces = code.text.substring(startOfLine, code.selectionStart).match(/^[ \t]*/)[0];
                console.log("spaces", "'" + code.text.substring(startOfLine, code.selectionStart) + "'", spaces.length);

                code.remove(code.selectionStart, code.selectionEnd);
                code.insert(code.selectionStart, "\n" + spaces);

                textChanged();
            }

            function tabIndent(back) {
                code.remove(code.selectionStart, code.selectionEnd);
                code.insert(code.selectionStart, "    ");

                textChanged();
            }
        }
    }
}
