import QtQuick
import QtQuick.Controls
import pollocad

Window {
    width: 1024
    height: 786
    visible: true
    title: qsTr("pollocad NEO")

    SplitView {
        id: splitView
        anchors.fill: parent

        handle: Rectangle {
            id: handleDelegate
            implicitWidth: 4

            color: SplitHandle.pressed ? "#81e889"
                : (SplitHandle.hovered ? Qt.lighter("#c2f4c6", 1.1) : "#c2f4c6")

            containmentMask: Item {
                x: (handleDelegate.width - width) / 2
                width: 8
                height: splitView.height
            }
        }

        TextArea {
            id: code
            SplitView.preferredWidth: 300
            text: `
move([10, 0, 0]) {
    box([50,10,10]);
    box([100, 100, 1]);
}
`.trim()

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
            }

            Connections {
                function onTextChanged() {
                    executor.execute(code.text);
                }
            }
        }

        OcctView {
            id: viewer
        }
    }

    Component.onCompleted: {
        executor.execute(code.text);
    }

    Connections {
        target: executor

        function onResult(res) {
            console.log("pollo", res);

            if (res.shape()) {
                viewer.setShape(res.shape());
            }
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

        console.log("no dot num", num);

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

        /*let mut cursor = buf.iter_at_mark(&buf.get_insert());

        let mut start = cursor.clone();
        start.backward_find_char(is_non_numeric, None);
        if !start.is_start() {
            start.forward_char();
        }

        if start == cursor {
            return false;
        }

        let mut num = buf.slice(&start, &cursor, true).to_string();

        let dot = num.find('.').map(|dot| num.len() - dot);
        if let Some(dot) = dot {
            num.remove(num.len() - dot);
        }

        let Some(num) = num
            .parse::<i128>()
            .ok()
            .and_then(|n| n.checked_add(delta)) else { return false; };

        let idx = if num < 0 { 1 } else { 0 };

        let mut num = num.to_string();
        if let Some(dot) = dot {
            while num.len() < dot + idx {
                num.insert(idx, '0');
            }

            num.insert(num.len() - dot + 1, '.');
        }

        buf.begin_user_action();
        buf.delete(&mut start, &mut cursor);
        buf.insert(&mut buf.iter_at_mark(&buf.get_insert()), &num.to_string());
        buf.end_user_action();

        return true;*/
    }
}
