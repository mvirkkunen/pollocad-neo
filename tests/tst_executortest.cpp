#include <algorithm>
#include <QtTest>
#include "helpers.h"
#include "parser.h"
#include "executor.h"

char *toString(const Value &val) {
    return toAllocatedString(val);
}

class ExecutorTest : public QObject
{
    Q_OBJECT

private slots:
    // clang-format off
    /*TESTCASE(simple,
        "1 + 2",
        ({ast::CallExpr{
            "+",
            {
                ast::NumberExpr{1},
                ast::NumberExpr{2},
            },
                                              }}));*/
    // clang-format on

    void testExecutor() {
        QFETCH(QString, code);
        QFETCH(Value, expected);

        Executor executor;
        auto actual = executor.execute(code.toStdString()).result;

        QCOMPARE(actual, expected);
    }

    void testExecutor_data() {
        using namespace ast;

        QTest::addColumn<QString>("code");
        QTest::addColumn<Value>("expected");

        QTest::newRow("empty") //
            << ""
            << Value{undefined};

        QTest::newRow("number") //
            << "1"
            << Value{1.0};

        QTest::newRow("addition") //
            << "1 + 1"
            << Value{2.0};

        QTest::newRow("list") //
            << "[1, 2]"
            << Value{List{1.0, 2.0}};

        QTest::newRow("if_true") //
            << "if 1 { 1 } else { 2 }"
            << Value{1.0};

        QTest::newRow("if_false") //
            << "if 0 { 1 } else { 2 }"
            << Value{2.0};

        QTest::newRow("list_index") //
            << "[1, 2, 3][1]"
            << Value{2.0};

        QTest::newRow("list_index_nested") //
            << "[1, [2, 3, 4], 5][1][2]"
            << Value{4.0};

        QTest::newRow("list_index_nested2") //
            << "[1, [2, 3, [4, 5]], 6][1].z[1]"
            << Value{5.0};

        QTest::newRow("list_swizzle") //
            << "[1, 2, 3].yzx"
            << Value{List{2.0, 3.0, 1.0}};

        QTest::newRow("def_none") //
            << "def pollo() { 1 } pollo();"
            << Value{1.0};

        QTest::newRow("def_one") //
            << "def pollo(a) { a + 1 } pollo(2);"
            << Value{3.0};
    }
};

QTEST_APPLESS_MAIN(ExecutorTest)

#include "tst_executortest.moc"
