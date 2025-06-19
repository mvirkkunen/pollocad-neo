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
                ast::LiteralExpr{1},
                ast::LiteralExpr{2},
            },
                                              }}));*/
    // clang-format on

    void testParser() {
        QFETCH(QString, code);
        QFETCH(ast::ExprList, expected);

        auto actual = parse(code.toStdString(), false);

        if (!actual.result) {
            for (const auto &err : actual.errors) {
                std::cerr << err.message;
            }
        }

        if (actual.result != expected) {
            std::cerr << "   Failing code: " << code.toStdString() << "\n";
        }

        QCOMPARE(actual.result, expected);
    }

    void testParser_data() {
        using namespace ast;

        QTest::addColumn<QString>("code");
        QTest::addColumn<ast::ExprList>("expected");

        QTest::newRow("empty") //
            << ""
            << ExprList{};

        QTest::newRow("simple") //
            << "1 + 2"
            << ExprList{CallExpr{
                   "+",
                   {
                       LiteralExpr{1},
                       LiteralExpr{2},
                   },
               }};

        QTest::newRow("two") //
            << "1 + 2; 3 + 4;"
            << ExprList{
                   CallExpr{
                       "+",
                       {
                           LiteralExpr{1},
                           LiteralExpr{2},
                       },
                   },
                   CallExpr{
                       "+",
                       {
                           LiteralExpr{3},
                           LiteralExpr{4},
                       },
                   }};

        QTest::newRow("if_return") //
            << "if 1 { 2 } else { 3 }"
            << ExprList{CallExpr{
                   "if", {LiteralExpr{1}, LambdaExpr{{LiteralExpr(2)}}, LambdaExpr{{LiteralExpr(3)}}}}};

        QTest::newRow("if_and") //
            << "if 1 { 2; } 3;"
            << ExprList{
                   CallExpr{
                       "if",
                       {
                           LiteralExpr{1},
                           LambdaExpr{{LiteralExpr(2)}},
                       },
                   },
                   LiteralExpr{3},
               };

        QTest::newRow("if_else") //
            << "(if 1 { 2; } else { 3; }) + 4;"
            << ExprList{CallExpr{
                   "+",
                   {
                       CallExpr{
                           "if",
                           {
                               LiteralExpr{1},
                               LambdaExpr{{LiteralExpr(2)}},
                               LambdaExpr{{LiteralExpr(3)}},
                           },
                       },
                       LiteralExpr{4},
                   }}};

        QTest::newRow("var") //
            << "1 + pollo;"
            << ExprList{CallExpr{
                   "+",
                   {
                       LiteralExpr{1},
                       VarExpr{"pollo"},
                   }}};

        QTest::newRow("assign") //
            << "pollo = 2; perro + 1;"
            << ExprList{
                   LetExpr{"pollo", Expr{LiteralExpr{2}}},
                   CallExpr{"+", {VarExpr{"perro"}, LiteralExpr{1}}}};

        /*QTest::newRow("return") //
            << "pollo + 1"
            << ExprList{{CallExpr{"+", {VarExpr{"pollo"}, LiteralExpr{1}}}}};*/

        QTest::newRow("call_func") //
            << "pollo();"
            << ExprList{CallExpr{"pollo"}};

        QTest::newRow("call_func_one") //
            << "pollo(1);"
            << ExprList{CallExpr{"pollo", {LiteralExpr{1}}}};

        QTest::newRow("call_func_two") //
            << "pollo(1, 2);"
            << ExprList{CallExpr{"pollo", {LiteralExpr{1}, {LiteralExpr{2}}}}};

        QTest::newRow("call_func_named") //
            << "pollo(a=1);"
            << ExprList{CallExpr{"pollo", {}, {{"a", Expr{LiteralExpr{1}}}}}};

        QTest::newRow("call_func_mixed") //
            << "pollo(2, a=1);"
            << ExprList{CallExpr{"pollo", {LiteralExpr{2}}, {{"a", Expr{LiteralExpr{1}}}}}};

        QTest::newRow("call_func_block") //
            << "pollo() { 1; }"
            << ExprList{CallExpr{"pollo", {}, {{"$children", Expr{LambdaExpr{{LiteralExpr{1}}}}}}}};

        QTest::newRow("call_func_block_func") //
            << "pollo() { perro(); }"
            << ExprList{CallExpr{"pollo", {}, {{"$children", Expr{LambdaExpr{{CallExpr{"perro"}}}}}}}};

        QTest::newRow("call_func_nested_children_block") //
            << "pollo() perro() { 1; }"
            << ExprList{CallExpr{
                   "pollo",
                   {},
                   {{"$children",
                     Expr{LambdaExpr{ExprList{CallExpr{
                         "perro", {}, {{"$children", Expr{LambdaExpr{{LiteralExpr{1}}}}}}}}}}}}}};

        QTest::newRow("call_func_nested_children_no_block") //
            << "pollo() perro();"
            << ExprList{CallExpr{
                   "pollo",
                   {},
                   {{"$children", Expr{LambdaExpr{ExprList{CallExpr{"perro", {}, {}}}}}}}}};

        QTest::newRow("call_two_func") //
            << "pollo(); perro();"
            << ExprList{CallExpr{"pollo"}, CallExpr{"perro"}};

        /*std::cerr << *parse(R"(
move([10, 0, 0]) {
    box([50,10,10]);
    box([100, 100, 1]);
}


move([0, 0, 50]) {
    repeat(25) move([25 * floor($i / 10), 25 * ($i % 10)]) cyl(r=10, h=50);
}

def thin_cyl(x, y, height) {
    move([x, y]) cyl(r=1, h=height);
}

thin_cyl(100, 100, 100);
thin_cyl(100, 110, 50);

)") << "\n";*/
    }
};

//void ParserTest::initTestCase() {}

//void ParserTest::cleanupTestCase() {}



QTEST_APPLESS_MAIN(ParserTest)

#include "tst_parsertest.moc"
