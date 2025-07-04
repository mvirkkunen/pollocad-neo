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

        auto result = executor.execute(code.toStdString());
        if (!result.messages.empty()) {
            for (const auto &msg : result.messages) {
                std::cerr << msg.span << ": " << msg.message;
            }
        }

        auto actual = result.result;

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

        QTest::newRow("decimal_number") //
            << "1.5"
            << Value{1.5};

        QTest::newRow("addition") //
            << "1 + 1"
            << Value{2.0};

        QTest::newRow("list") //
            << "[1, 2]"
            << Value{ValueList{1.0, 2.0}};

        QTest::newRow("if_true") //
            << "if (1) { 1 } else { 2 }"
            << Value{1.0};

        QTest::newRow("if_false") //
            << "if (0) { 1 } else { 2 }"
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
            << Value{ValueList{2.0, 3.0, 1.0}};

        QTest::newRow("def_none") //
            << "def pollo() { 1 } pollo();"
            << Value{1.0};

        QTest::newRow("def_one") //
            << "def pollo(a) { a + 1 } pollo(2);"
            << Value{3.0};

        QTest::newRow("if_chain_true")
            << "if (1) 1"
            << Value{1.0};

        QTest::newRow("if_chain_false")
            << "if (0) 1"
            << Value{undefined};

        QTest::newRow("ternary_true") << "1 ? 2 : 3" << Value{2};
        QTest::newRow("ternary_false") << "0 ? 2 : 3" << Value{3};
        QTest::newRow("nested_ternary_true") << "0 ? 1 : 2 ? 3 : 4" << Value{3};
        QTest::newRow("nested_ternary_false") << "0 ? 1 : 0 ? 3 : 4" << Value{4};
    }
};

QTEST_APPLESS_MAIN(ExecutorTest)

#include "tst_executortest.moc"
