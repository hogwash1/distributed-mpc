// test_expr_parser.cpp — Unit tests for ExprParser
// Task 02: expression parser
//
// Test 1: simple two-party addition  data1 + data2
// Test 2: nested complex expression  (data1*2.5 + data2) / 3.0
// Test 3: roundtrip  to_json → parse_json → identical
// Test 4: illegal party_id validation
// Test 5: multiplication depth computation
// Test 6: variable collection

#include "expr_parser.h"
#include <iostream>
#include <cmath>
#include <set>

using namespace mpc;

// ---------------------------------------------------------------------------
void test_header(const std::string& name) {
    std::cout << "\n==========================================================\n"
              << "  TEST: " << name << "\n"
              << "==========================================================\n";
}

// ===================================================================
// Test 1: Simple two-party addition
// JSON: {"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}}
// ===================================================================
bool test_simple_add() {
    test_header("Simple Two-Party Addition: data1 + data2");

    std::string json = R"({"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}})";
    std::cout << "Input JSON: " << json << "\n";

    auto ast = ExprParser::parse_json(json);
    std::cout << "Parsed AST:\n" << ast->to_string() << "\n";

    bool pass = true;
    if (ast->op != OpType::ADD) { std::cerr << "  FAIL: root not ADD\n"; pass = false; }
    if (!ast->lhs || ast->lhs->op != OpType::VAR || ast->lhs->var_party_id != 1)
        { std::cerr << "  FAIL: lhs not VAR(1)\n"; pass = false; }
    if (!ast->rhs || ast->rhs->op != OpType::VAR || ast->rhs->var_party_id != 2)
        { std::cerr << "  FAIL: rhs not VAR(2)\n"; pass = false; }

    std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] simple_add\n";
    return pass;
}

// ===================================================================
// Test 2: Nested complex expression (data1 * 2.5 + data2) / 3.0
// ===================================================================
bool test_nested_complex() {
    test_header("Nested Complex: (data1*2.5 + data2)/3.0");

    std::string json = R"(
        {"op":"div_const","value":3.0,
         "lhs":{"op":"add",
                "lhs":{"op":"mul",
                       "lhs":{"op":"var","party":1},
                       "rhs":{"op":"const","value":2.5}},
                "rhs":{"op":"var","party":2}}})";
    std::cout << "Input JSON: " << json << "\n";

    auto ast = ExprParser::parse_json(json);
    std::cout << "Parsed AST:\n" << ast->to_string() << "\n";

    bool pass = true;
    if (ast->op != OpType::DIV_CONST || std::abs(ast->const_value - 3.0) > 1e-9)
        { std::cerr << "  FAIL: root not DIV_CONST/3.0\n"; pass = false; }

    auto add_node = ast->lhs;
    if (!add_node || add_node->op != OpType::ADD)
        { std::cerr << "  FAIL: lhs not ADD\n"; pass = false; }

    auto mul_node = add_node ? add_node->lhs : nullptr;
    if (!mul_node || mul_node->op != OpType::MUL)
        { std::cerr << "  FAIL: ADD.lhs not MUL\n"; pass = false; }

    auto var2_node = add_node ? add_node->rhs : nullptr;
    if (!var2_node || var2_node->op != OpType::VAR || var2_node->var_party_id != 2)
        { std::cerr << "  FAIL: ADD.rhs not VAR(2)\n"; pass = false; }

    if (mul_node) {
        if (!mul_node->lhs || mul_node->lhs->op != OpType::VAR || mul_node->lhs->var_party_id != 1)
            { std::cerr << "  FAIL: MUL.lhs not VAR(1)\n"; pass = false; }
        if (!mul_node->rhs || mul_node->rhs->op != OpType::CONST || std::abs(mul_node->rhs->const_value - 2.5) > 1e-9)
            { std::cerr << "  FAIL: MUL.rhs not CONST(2.5)\n"; pass = false; }
    }

    std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] nested_complex\n";
    return pass;
}

// ===================================================================
// Test 3: Roundtrip — to_json → parse_json → identical AST
// ===================================================================
bool test_roundtrip() {
    test_header("Roundtrip: to_json → parse_json → to_json identical");

    // Build AST: (A + B) * C
    auto a   = AstNode::make_var(1);
    auto b   = AstNode::make_var(2);
    auto c   = AstNode::make_var(3);
    auto add = AstNode::make_binary(OpType::ADD, a, b);
    auto mul = AstNode::make_binary(OpType::MUL, add, c);

    std::string json1 = ExprParser::to_json(mul);
    std::cout << "to_json: " << json1 << "\n";

    auto parsed = ExprParser::parse_json(json1);
    std::string json2 = ExprParser::to_json(parsed);

    std::cout << "re-parse to_json: " << json2 << "\n";

    bool pass = (json1 == json2);
    if (!pass) {
        std::cerr << "  FAIL: JSON strings differ\n";
        std::cerr << "    orig: " << json1 << "\n";
        std::cerr << "    rtrip:" << json2 << "\n";
    }
    std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] roundtrip\n";
    return pass;
}

// ===================================================================
// Test 4: Illegal party_id validation
// ===================================================================
bool test_illegal_party() {
    test_header("Illegal Party ID Validation");

    // AST with party_id=3, but num_parties=2
    auto v1 = AstNode::make_var(1);
    auto v3 = AstNode::make_var(3);
    auto add = AstNode::make_binary(OpType::ADD, v1, v3);

    std::string err = ExprParser::validate(add, 2);
    bool pass = !err.empty();
    std::cout << "  validate(add(VAR1,VAR3), num_parties=2): \"" << err << "\"\n";
    std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] illegal_party\n";
    return pass;
}

// ===================================================================
// Test 5: Multiplication depth computation
// ===================================================================
bool test_depth_computation() {
    test_header("Multiplication Depth Computation");

    bool all = true;

    // Pure add: depth = 0
    auto a1 = AstNode::make_var(1);
    auto a2 = AstNode::make_var(2);
    auto add_only = AstNode::make_binary(OpType::ADD, a1, a2);
    int d1 = ExprParser::compute_depth(add_only);
    bool p1 = (d1 == 0);
    std::cout << "  [" << (p1 ? "PASS" : "FAIL") << "] pure ADD depth=" << d1 << " (expected 0)\n";
    all = all && p1;

    // One mul: depth = 1
    auto m1 = AstNode::make_binary(OpType::MUL, a1, a2);
    auto add_mul = AstNode::make_binary(OpType::ADD, m1, a1);
    int d2 = ExprParser::compute_depth(add_mul);
    bool p2 = (d2 == 1);
    std::cout << "  [" << (p2 ? "PASS" : "FAIL") << "] one MUL depth=" << d2 << " (expected 1)\n";
    all = all && p2;

    // Nested mul: depth = 2
    auto b1 = AstNode::make_var(1);
    auto b2 = AstNode::make_var(2);
    auto b3 = AstNode::make_var(3);
    auto inner = AstNode::make_binary(OpType::MUL, b1, b2);
    auto outer = AstNode::make_binary(OpType::MUL, inner, b3);
    int d3 = ExprParser::compute_depth(outer);
    bool p3 = (d3 == 2);
    std::cout << "  [" << (p3 ? "PASS" : "FAIL") << "] nested MUL depth=" << d3 << " (expected 2)\n";
    all = all && p3;

    return all;
}

// ===================================================================
// Test 6: Variable collection
// ===================================================================
bool test_collect_vars() {
    test_header("Variable Collection");

    // Expression: (A+B) * (A+C) — uses vars 1,2,3 with 1 appearing twice
    auto a1 = AstNode::make_var(1);
    auto a2 = AstNode::make_var(2);
    auto a3 = AstNode::make_var(3);
    auto add1 = AstNode::make_binary(OpType::ADD, a1, a2);         // A+B
    auto a1b = AstNode::make_var(1);
    auto add2 = AstNode::make_binary(OpType::ADD, a1b, a3);        // A+C
    auto mul  = AstNode::make_binary(OpType::MUL, add1, add2);     // (A+B)*(A+C)

    auto vars = ExprParser::collect_vars(mul);
    std::cout << "  collected vars: ";
    for (auto v : vars) std::cout << v << " ";
    std::cout << "\n";

    std::set<int32_t> expected = {1, 2, 3};
    std::set<int32_t> actual(vars.begin(), vars.end());
    bool pass = (expected == actual);
    std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] collect_vars (expected 1,2,3 sorted)\n";
    return pass;
}

// ===================================================================
// Main
// ===================================================================
int main() {
    std::cout << "==========================================================\n"
              << "  Task-02 ExprParser Unit Tests\n"
              << "==========================================================\n";

    int passed = 0, total = 6;

    if (test_simple_add())       passed++;
    if (test_nested_complex())   passed++;
    if (test_roundtrip())        passed++;
    if (test_illegal_party())    passed++;
    if (test_depth_computation()) passed++;
    if (test_collect_vars())     passed++;

    std::cout << "\n==========================================================\n"
              << "  RESULTS: " << passed << " / " << total << " tests passed\n"
              << "==========================================================\n";

    return (passed == total) ? 0 : 1;
}
