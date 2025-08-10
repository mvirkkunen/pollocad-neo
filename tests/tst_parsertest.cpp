#include <algorithm>
#include <QtTest>
#include "helpers.h"
#include "parser.h"

namespace ast
{
/*char *toString(const std::optional<ExprList> &exprs) {
    return exprs ? toAllocatedString(*exprs) : qstrdup("<parse error>");
}*/

char *toString(const BlockExpr &exprs) {
    return toAllocatedString(Expr(exprs));
}

char *toString(const Expr &ex) {
    return toAllocatedString(ex);
}
}

#define TESTCASE(NAME, ACTUAL, EXPECTED) \
    void test_##NAME() { \
        auto actual = parse(ACTUAL); \
        ast::BlockExpr expected EXPECTED; \
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
        QFETCH(ast::BlockExpr, expected);

        std::cout << "==== TEST ====\n";

        auto actual = parse(code.toStdString(), false);
        
        if (!actual.result) {
            std::cerr << "   Failing code: " << code.toStdString() << "\n";

            for (const auto &err : actual.errors) {
                std::cerr << err.message;
            }

            QFAIL("parse error");
        }

        if (actual.result != expected) {
            std::cerr << "   Failing code: " << code.toStdString() << "\n";
        }

        QCOMPARE(*actual.result, expected);
    }

    void testParser_data() {
        using namespace ast;

        QTest::addColumn<QString>("code");
        QTest::addColumn<ast::BlockExpr>("expected");

        QTest::newRow("empty") //
            << ""
            << BlockExpr{};

        QTest::newRow("simple") //
            << "1 + 2"
            << BlockExpr{CallExpr{
                   "+",
                   {
                       LiteralExpr{1},
                       LiteralExpr{2},
                   },
               }};

        QTest::newRow("wat") << "for (x = [0: 8]) { }" << BlockExpr{};

        QTest::newRow("two") //
            << "1 + 2; 3 + 4;"
            << BlockExpr{
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
            << "if (1) { 2 } else { 3 }"
            << BlockExpr{CallExpr{
                   "if",
                    {
                        LiteralExpr{1},
                        LambdaExpr{Expr{BlockExpr{Expr{{LiteralExpr(2)}}}}},
                        LambdaExpr{Expr{BlockExpr{Expr{{LiteralExpr(3)}}}}}
                    }}};

        QTest::newRow("if_and") //
            << "if (1) { 2; } 3;"
            << BlockExpr{
                CallExpr{
                    "if",
                    {
                        LiteralExpr{1},
                        LambdaExpr{Expr{BlockExpr{Expr{LiteralExpr(2)}}}},
                    },
                },
                LiteralExpr{3}
            };

        QTest::newRow("if_else") //
            << "(if (1) { 2; } else { 3; }) + 4;"
            << BlockExpr{CallExpr{
                   "+",
                   {
                       CallExpr{
                           "if",
                           {
                               LiteralExpr{1},
                               LambdaExpr{Expr{BlockExpr{Expr{LiteralExpr(2)}}}},
                               LambdaExpr{Expr{BlockExpr{Expr{LiteralExpr(3)}}}},
                           },
                       },
                       LiteralExpr{4},
                   }}};

        QTest::newRow("var") //
            << "1 + pollo;"
            << BlockExpr{CallExpr{
                   "+",
                   {
                       LiteralExpr{1},
                       VarExpr{"pollo"},
                   }}};

        QTest::newRow("assign") //
            << "pollo = 2; perro + 1;"
            << BlockExpr{
                   LetExpr{"pollo", Expr{LiteralExpr{2}}},
                   CallExpr{"+", {VarExpr{"perro"}, LiteralExpr{1}}}};

        /*QTest::newRow("return") //
            << "pollo + 1"
            << BlockExpr{{CallExpr{"+", {VarExpr{"pollo"}, LiteralExpr{1}}}}};*/

        QTest::newRow("call_func") //
            << "pollo();"
            << BlockExpr{CallExpr{"pollo"}};

        QTest::newRow("call_func_one") //
            << "pollo(1);"
            << BlockExpr{CallExpr{"pollo", {LiteralExpr{1}}}};

        QTest::newRow("call_func_two") //
            << "pollo(1, 2);"
            << BlockExpr{CallExpr{"pollo", {LiteralExpr{1}, {LiteralExpr{2}}}}};

        QTest::newRow("call_func_named") //
            << "pollo(a=1);"
            << BlockExpr{CallExpr{"pollo", {}, {{"a", Expr{LiteralExpr{1}}}}}};

        QTest::newRow("call_func_mixed") //
            << "pollo(2, a=1);"
            << BlockExpr{CallExpr{"pollo", {LiteralExpr{2}}, {{"a", Expr{LiteralExpr{1}}}}}};

        QTest::newRow("call_func_block") //
            << "pollo() { 1; }"
            << BlockExpr{CallExpr{"pollo", {}, {{"$children", Expr{LambdaExpr{Expr{BlockExpr{Expr{LiteralExpr{1}}}}}}}}}};

        QTest::newRow("call_func_block_func") //
            << "pollo() { perro(); }"
            << BlockExpr{CallExpr{"pollo", {}, {{"$children", Expr{LambdaExpr{Expr{BlockExpr{Expr{CallExpr{"perro"}}}}}}}}}};

        QTest::newRow("call_func_nested_children_block") //
            << "pollo() perro() { 1; }"
            << BlockExpr{CallExpr{
                   "pollo",
                   {},
                   {{"$children",
                     Expr{LambdaExpr{Expr{CallExpr{
                         "perro", {}, {{"$children", Expr{LambdaExpr{Expr{BlockExpr{Expr{LiteralExpr{1}}}}}}}}}}}}}}}};

        QTest::newRow("call_func_nested_children_no_block") //
            << "pollo() perro();"
            << BlockExpr{CallExpr{
                   "pollo",
                   {},
                   {{"$children", Expr{LambdaExpr{Expr{CallExpr{"perro", {}, {}}}}}}}}};

        QTest::newRow("call_two_func") //
            << "pollo(); perro();"
            << BlockExpr{Expr{CallExpr{"pollo"}}, Expr{CallExpr{"perro"}}};

        QTest::newRow("for_no_step") //
            << "for (x = [0 : 2]) 3;"
            << BlockExpr{Expr{
                CallExpr{"for", {LiteralExpr{"x"}, LiteralExpr{0.0}, LiteralExpr{1.0}, LiteralExpr{2.0}}, {{"$children", Expr{LambdaExpr{Expr{LiteralExpr{3}}}}}}}}};
    }

    void testParserSuccessOnly() {
        QFETCH(QString, code);

        auto actual = parse(code.toStdString(), false);
        
        if (!actual.result) {
            std::cerr << "   Failing code: " << code.toStdString() << "\n";

            for (const auto &err : actual.errors) {
                std::cerr << err.message;
            }

            QFAIL("parse error");
        }
    }

    void testParserSuccessOnly_data() {
        using namespace ast;

        QTest::addColumn<QString>("code");

        QTest::newRow("function_and_function") << "pollo() 1;";
        QTest::newRow("function_and_function_in_block_in_expr") << "({pollo() 1;})";
        QTest::newRow("function_and_number") << "pollo() perro();";
        QTest::newRow("if_as_first") << "if (1) pollo();";
        QTest::newRow("if_else_no_brace") << "if (1) 1; else 2;";
        QTest::newRow("if_else_else_if") << "if (1) 1; else if (2) 3; else 4;";
        QTest::newRow("if_else_with_brace") << "if (1) { 1; } else { 2; }";
        QTest::newRow("if_else_with_brace_in_expr") << "pollo(if (1) { 1; } else { 2; })";
    }
};

QTEST_APPLESS_MAIN(ParserTest)

#include "tst_parsertest.moc"
