#include "expr_parser.h"
#include <iostream>
#include <cassert>

using namespace mpc;

int main() {
    std::cout << "Running ExprParser Tests..." << std::endl;

    // 测试用例 1: 解析简单的 data1 + data2
    std::string json1 = R"({"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}})";
    auto ast1 = ExprParser::parse_json(json1);
    assert(ast1->type == OpType::ADD);
    assert(ast1->left->type == OpType::VAR && ast1->left->party_id == 1);
    assert(ast1->right->type == OpType::VAR && ast1->right->party_id == 2);
    std::cout << "Test 1 Passed: Simple Add AST" << std::endl;

    // 测试用例 2: 解析 (data1 * 2.5 + data2) / 3.0，验证嵌套
    std::string json2 = R"({
        "op": "div_const",
        "lhs": {
            "op": "add",
            "lhs": { "op": "mul", "lhs": { "op": "var", "party": 1 }, "rhs": { "op": "const", "value": 2.5 } },
            "rhs": { "op": "var", "party": 2 }
        },
        "value": 3.0
    })";
    auto ast2 = ExprParser::parse_json(json2);
    assert(ast2->type == OpType::DIV_CONST);
    assert(ast2->right->value == 3.0);
    assert(ast2->left->type == OpType::ADD);
    assert(ast2->left->left->type == OpType::MUL);
    std::cout << "Test 2 Passed: Nested AST Parsing" << std::endl;

    // 测试用例 3: to_json 输出的 JSON 可以被 parse_json 重新解析
    std::string dumped_json = ExprParser::to_json(ast2);
    auto ast3 = ExprParser::parse_json(dumped_json);
    assert(ast3->type == OpType::DIV_CONST && ast3->right->value == 3.0);
    std::cout << "Test 3 Passed: Serialization Cycle (parse -> dump -> parse)" << std::endl;

    // 测试用例 4: validate 对非法 party_id 抛出异常 / 返回 false
    assert(ExprParser::validate(ast1, 3) == true); // 3个参与方，1和2合法
    assert(ExprParser::validate(ast1, 1) == false); // 只有1个参与方，party=2非法
    std::cout << "Test 4 Passed: Validation Logic" << std::endl;

    // 测试用例 5: compute_depth 正确计算
    assert(ExprParser::compute_depth(ast1) == 0); // 纯加法深度 0

    // 创建一个包含两个密文相乘的 AST 来测试深度为 1
    std::string json_mul = R"({"op":"mul","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}})";
    auto ast_mul = ExprParser::parse_json(json_mul);
    assert(ExprParser::compute_depth(ast_mul) == 1); // 密文乘密文，深度1

    // (data1 * 2.5 + data2) / 3.0，这里 2.5 和 3.0 是常数，同态深度不变，应为 0
    assert(ExprParser::compute_depth(ast2) == 0);
    std::cout << "Test 5 Passed: Depth Computation" << std::endl;

    // 测试用例 6: collect_vars 收集到正确的变量集合
    auto vars = ExprParser::collect_vars(ast2);
    assert(vars.size() == 2);
    assert(vars[0] == 1 && vars[1] == 2);
    std::cout << "Test 6 Passed: Variable Collection" << std::endl;

    std::cout << "All 6 tests passed successfully!" << std::endl;
    return 0;
}