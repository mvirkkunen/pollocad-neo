import QtQuick
import QtQuick.Controls

TextEdit {
    id: code

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

    Component.onCompleted: {
        util.setupTextDocument(code.textDocument);
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
    }

    function returnIndent() {
        let startOfLine = code.text.lastIndexOf("\n", code.selectionStart - 1) + 1;
        console.log("super pollo", startOfLine);

        const spaces = code.text.substring(startOfLine, code.selectionStart).match(/^[ \t]*/)[0];
        console.log("spaces", "'" + code.text.substring(startOfLine, code.selectionStart) + "'", spaces.length);

        code.remove(code.selectionStart, code.selectionEnd);
        code.insert(code.selectionStart, "\n" + spaces);
    }

    function tabIndent(back) {
        code.remove(code.selectionStart, code.selectionEnd);
        code.insert(code.selectionStart, "    ");
    }
}
