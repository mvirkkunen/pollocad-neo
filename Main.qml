import QtQuick
import QtQuick.Controls
import pollocad

Window {
    width: 1280
    height: 800
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
`.trim()

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
            if (res.shape()) {
                viewer.setShape(res.shape());
            }
        }
    }
}
