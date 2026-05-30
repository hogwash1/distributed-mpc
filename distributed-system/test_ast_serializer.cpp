// test_ast_serializer.cpp — Unit tests for AstSerializer
// Task 01: protocol extension
//
// Test 1: (A+B)*C roundtrip
// Test 2: 5-level deep nested expression
// Test 3: Leaf nodes (CONST / VAR / DIV_CONST)

#include "ast_serializer.h"
#include <iostream>
#include <cmath>
#include <string>

using namespace mpc;

// ---------------------------------------------------------------------------
// Compare two AST nodes recursively
// ---------------------------------------------------------------------------
bool ast_equals(const std::shared_ptr<AstNode>& a,
                const std::shared_ptr<AstNode>& b) {
    if (!a && !b) return true;
    if (!a || !b) return false;

    if (a->op != b->op) {
        std::cerr << "  op mismatch: " << (int)a->op << " vs " << (int)b->op << "\n";
        return false;
    }

    if (a->op == OpType::CONST || a->op == OpType::DIV_CONST) {
        if (std::abs(a->const_value - b->const_value) > 1e-12) {
            std::cerr << "  const_value mismatch\n";
            return false;
        }
    }
    if (a->op == OpType::VAR) {
        if (a->var_party_id != b->var_party_id) {
            std::cerr << "  var_party_id mismatch\n";
            return false;
        }
    }

    return ast_equals(a->lhs, b->lhs) && ast_equals(a->rhs, b->rhs);
}

// ---------------------------------------------------------------------------
void test_header(const std::string& name) {
    std::cout << "\n==========================================================\n";
    std::cout << "  TEST: " << name << "\n";
    std::cout << "==========================================================\n";
}

// ===================================================================
// Test 1: (A+B)*C roundtrip
// ===================================================================
bool test_simple_roundtrip() {
    test_header("(A+B)*C Roundtrip");

    auto a   = AstNode::make_var(1);
    auto b   = AstNode::make_var(2);
    auto c   = AstNode::make_var(3);
    auto add = AstNode::make_binary(OpType::ADD, a, b);
    auto mul = AstNode::make_binary(OpType::MUL, add, c);

    std::cout << "Original:\n" << mul->to_string() << "\n";

    auto proto    = AstSerializer::serialize(mul);
    auto restored = AstSerializer::deserialize(proto);

    std::cout << "Restored:\n" << restored->to_string() << "\n";

    bool pass = ast_equals(mul, restored);
    std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] (A+B)*C roundtrip\n";
    return pass;
}

// ===================================================================
// Test 2: 5-level deep nesting
// Expression: ((-A - B + 5) * C) / 2
// ===================================================================
bool test_deep_nesting() {
    test_header("5-Level Deep Nesting: ((-A-B+5)*C)/2");

    auto v1  = AstNode::make_var(1);
    auto v2  = AstNode::make_var(2);
    auto v3  = AstNode::make_var(3);
    auto c5  = AstNode::make_const(5.0);

    auto neg = AstNode::make_unary(OpType::NEGATE, v1);
    auto sub = AstNode::make_binary(OpType::SUB, neg, v2);
    auto add = AstNode::make_binary(OpType::ADD, sub, c5);
    auto mul = AstNode::make_binary(OpType::MUL, add, v3);
    auto div = AstNode::make_div_const(mul, 2.0);

    std::cout << "Original:\n" << div->to_string() << "\n";

    auto proto    = AstSerializer::serialize(div);
    auto restored = AstSerializer::deserialize(proto);

    std::cout << "Restored:\n" << restored->to_string() << "\n";

    bool pass = ast_equals(div, restored);
    std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] 5-level deep nesting\n";
    return pass;
}

// ===================================================================
// Test 3: Leaf nodes — CONST, VAR, DIV_CONST
// ===================================================================
bool test_leaf_nodes() {
    test_header("Leaf Nodes: CONST / VAR / DIV_CONST");

    bool all = true;

    // CONST
    auto c3 = AstNode::make_const(3.14159);
    auto cp = AstSerializer::serialize(c3);
    auto cr = AstSerializer::deserialize(cp);
    bool p1 = ast_equals(c3, cr);
    std::cout << "  [" << (p1 ? "PASS" : "FAIL") << "] CONST(" << c3->const_value << ")\n";
    all = all && p1;

    // VAR
    auto v7 = AstNode::make_var(7);
    auto vp = AstSerializer::serialize(v7);
    auto vr = AstSerializer::deserialize(vp);
    bool p2 = ast_equals(v7, vr);
    std::cout << "  [" << (p2 ? "PASS" : "FAIL") << "] VAR(party=" << v7->var_party_id << ")\n";
    all = all && p2;

    // DIV_CONST
    auto d2 = AstNode::make_div_const(nullptr, 7.0);
    auto dp = AstSerializer::serialize(d2);
    auto dr = AstSerializer::deserialize(dp);
    bool p3 = ast_equals(d2, dr);
    std::cout << "  [" << (p3 ? "PASS" : "FAIL") << "] DIV_CONST(/7.0)\n";
    all = all && p3;

    return all;
}

// ===================================================================
int main() {
    std::cout << "==========================================================\n";
    std::cout << "  Task-01 AstSerializer Unit Tests\n";
    std::cout << "==========================================================\n";

    int passed = 0, total = 3;

    if (test_simple_roundtrip()) passed++;
    if (test_deep_nesting())     passed++;
    if (test_leaf_nodes())       passed++;

    std::cout << "\n==========================================================\n";
    std::cout << "  RESULTS: " << passed << " / " << total << " tests passed\n";
    std::cout << "==========================================================\n";

    return (passed == total) ? 0 : 1;
}
