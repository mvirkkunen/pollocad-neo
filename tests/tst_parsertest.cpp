#include <algorithm>
#include <QtTest>
#include "helpers.h"
#include "parser.h"

namespace ast
{
char *toString(const std::optional<ExprList> &exprs) {
    return exprs ? toAllocatedString(*exprs) : qstrdup("<parse error>");
}

char *toString(const ExprList &exprs) {
    return toAllocatedString(exprs);
}

char *toString(const Expr &ex) {
    return toAllocatedString(ex);
}
}

#define TESTCASE(NAME, ACTUAL, EXPECTED) \
    void test_##NAME() { \
        auto actual = parse(ACTUAL); \
        ast::ExprList expected EXPECTED; \
        QCOMPARE(actual, expected); \
    }

class ParserTest : public QObject
{
    Q_OBJECT

private slots:
    //void initTestCase();
    //void cleanupTestCase();

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

    void testParser() {
        QFETCH(QString, code);
        QFETCH(ast::ExprList, expected);

        auto actual = parse(code.toStdString());

        QCOMPARE(actual, expected);
    }

    void testParser_data() {
        using namespace ast;

        QTest::addColumn<QString>("code");
        QTest::addColumn<ast::ExprList>("expected");

        QTest::newRow("empty") //
            << ""
            << ExprList{};

        QTest::newRow("simple") //
            << "1 + 2;"
            << ExprList{CallExpr{
                   "+",
                   {
                       NumberExpr{1},
                       NumberExpr{2},
                   },
               }};

        QTest::newRow("two") //
            << "1 + 2; 3 + 4;"
            << ExprList{
                   CallExpr{
                       "+",
                       {
                           NumberExpr{1},
                           NumberExpr{2},
                       },
                   },
                   CallExpr{
                       "+",
                       {
                           NumberExpr{3},
                           NumberExpr{4},
                       },
                   }};

        QTest::newRow("if_return") //
            << "if 1 { 2 } else { 3 }"
            << ExprList{CallExpr{
                   "if", {NumberExpr{1}, BlockExpr{{NumberExpr(2)}}, BlockExpr{{NumberExpr(3)}}}}};

        QTest::newRow("if_and") //
            << "if 1 { 2; } 3;"
            << ExprList{
                   CallExpr{
                       "if",
                       {
                           NumberExpr{1},
                           BlockExpr{{NumberExpr(2)}},
                       },
                   },
                   NumberExpr{3},
               };

        QTest::newRow("if_else") //
            << "(if 1 { 2; } else { 3; }) + 4;"
            << ExprList{CallExpr{
                   "+",
                   {
                       CallExpr{
                           "if",
                           {
                               NumberExpr{1},
                               BlockExpr{{NumberExpr(2)}},
                               BlockExpr{{NumberExpr(3)}},
                           },
                       },
                       NumberExpr{4},
                   }}};

        QTest::newRow("var") //
            << "1 + pollo;"
            << ExprList{CallExpr{
                   "+",
                   {
                       NumberExpr{1},
                       VarExpr{"pollo"},
                   }}};

        QTest::newRow("assign") //
            << "pollo = 2; perro + 1;"
            << ExprList{AssignExpr{
                   "pollo",
                   Expr{NumberExpr{2}},
                   ExprList{//
                            CallExpr{"+", {VarExpr{"perro"}, NumberExpr{1}}}}}};

        /*QTest::newRow("return") //
            << "pollo + 1"
            << ExprList{{CallExpr{"+", {VarExpr{"pollo"}, NumberExpr{1}}}}};*/

        QTest::newRow("call_func") //
            << "pollo();"
            << ExprList{CallExpr{"pollo"}};

        QTest::newRow("call_func_one") //
            << "pollo(1);"
            << ExprList{CallExpr{"pollo", {NumberExpr{1}}}};

        QTest::newRow("call_func_two") //
            << "pollo(1, 2);"
            << ExprList{CallExpr{"pollo", {NumberExpr{1}, {NumberExpr{2}}}}};

        QTest::newRow("call_func_named") //
            << "pollo(a=1);"
            << ExprList{CallExpr{"pollo", {}, {{"a", Expr{NumberExpr{1}}}}}};

        QTest::newRow("call_func_mixed") //
            << "pollo(2, a=1);"
            << ExprList{CallExpr{"pollo", {NumberExpr{2}}, {{"a", Expr{NumberExpr{1}}}}}};

        QTest::newRow("call_func_block") //
            << "pollo() { 1; }"
            << ExprList{CallExpr{"pollo", {}, {{"$children", Expr{BlockExpr{{NumberExpr{1}}}}}}}};

        QTest::newRow("call_func_block_func") //
            << "pollo() { perro(); }"
            << ExprList{CallExpr{"pollo", {}, {{"$children", Expr{BlockExpr{{CallExpr{"perro"}}}}}}}};

        QTest::newRow("call_func_nested_children_block") //
            << "pollo() perro() { 1; }"
            << ExprList{CallExpr{
                   "pollo",
                   {},
                   {{"$children",
                     Expr{BlockExpr{ExprList{CallExpr{
                         "perro", {}, {{"$children", Expr{BlockExpr{{NumberExpr{1}}}}}}}}}}}}}};

        QTest::newRow("call_func_nested_children_no_block") //
            << "pollo() perro();"
            << ExprList{CallExpr{
                   "pollo",
                   {},
                   {{"$children", Expr{BlockExpr{ExprList{CallExpr{"perro", {}, {}}}}}}}}};

        QTest::newRow("call_two_func") //
            << "pollo(); perro();"
            << ExprList{CallExpr{"pollo"}, CallExpr{"perro"}};
    }
};

//void ParserTest::initTestCase() {}

//void ParserTest::cleanupTestCase() {}



QTEST_APPLESS_MAIN(ParserTest)

#include "tst_parsertest.moc"
