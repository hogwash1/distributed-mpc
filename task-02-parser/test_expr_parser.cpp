// test_expr_parser.cpp — ExprParser 单元测试 (6 tests)

#include "expr_parser.h"
#include <iostream>
#include <set>
using namespace mpc;

void test_header(const std::string& n) { std::cout << "\n==========================================================\n  测试: " << n << "\n==========================================================\n"; }

bool test_simple_add() {
    test_header("两方加法");
    auto ast = ExprParser::parse_json(R"({"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}})");
    std::cout << "AST:\n" << ast->to_string() << "\n";
    bool pass = ast->op == OpType::ADD && ast->lhs->var_party_id == 1 && ast->rhs->var_party_id == 2;
    std::cout << "  [" << (pass?"✓ 通过":"✗ 失败") << "] 简单加法\n"; return pass;
}
bool test_nested_complex() {
    test_header("嵌套复杂");
    auto ast = ExprParser::parse_json(R"({"op":"div_const","value":3.0,"lhs":{"op":"add","lhs":{"op":"mul","lhs":{"op":"var","party":1},"rhs":{"op":"const","value":2.5}},"rhs":{"op":"var","party":2}}})");
    bool pass = ast->op == OpType::DIV_CONST && ast->lhs->op == OpType::ADD;
    std::cout << "  [" << (pass?"✓ 通过":"✗ 失败") << "] 嵌套复杂\n"; return pass;
}
bool test_roundtrip() {
    test_header("往返转换");
    auto a=AstNode::make_var(1), b=AstNode::make_var(2), c=AstNode::make_var(3);
    auto mul=AstNode::make_binary(OpType::MUL, AstNode::make_binary(OpType::ADD,a,b), c);
    std::string j1=ExprParser::to_json(mul), j2=ExprParser::to_json(ExprParser::parse_json(j1));
    bool pass=(j1==j2); std::cout<<"  ["<<(pass?"✓ 通过":"✗ 失败")<<"] 往返\n"; return pass;
}
bool test_illegal_party() {
    test_header("非法参与方");
    auto add=AstNode::make_binary(OpType::ADD, AstNode::make_var(1), AstNode::make_var(3));
    std::string err=ExprParser::validate(add, 2);
    bool pass=!err.empty(); std::cout<<"  validate: \""<<err<<"\"\n  ["<<(pass?"✓ 通过":"✗ 失败")<<"] 非法参与方\n"; return pass;
}
bool test_depth() {
    test_header("深度计算");
    bool all=true;
    auto d0=ExprParser::compute_depth(AstNode::make_binary(OpType::ADD, AstNode::make_var(1), AstNode::make_var(2)));
    bool p1=(d0==0); std::cout<<"  ["<<(p1?"✓ 通过":"✗ 失败")<<"] 纯加法 depth="<<d0<<" (期望 0)\n"; all&=p1;
    auto m1=AstNode::make_binary(OpType::MUL, AstNode::make_var(1), AstNode::make_var(2));
    int d1=ExprParser::compute_depth(AstNode::make_binary(OpType::ADD, m1, AstNode::make_var(1)));
    bool p2=(d1==1); std::cout<<"  ["<<(p2?"✓ 通过":"✗ 失败")<<"] 1次乘法 depth="<<d1<<" (期望 1)\n"; all&=p2;
    auto m2=AstNode::make_binary(OpType::MUL, m1, AstNode::make_var(3));
    int d2=ExprParser::compute_depth(m2);
    bool p3=(d2==2); std::cout<<"  ["<<(p3?"✓ 通过":"✗ 失败")<<"] 嵌套乘法 depth="<<d2<<" (期望 2)\n"; all&=p3;
    return all;
}
bool test_collect_vars() {
    test_header("变量收集");
    auto add1=AstNode::make_binary(OpType::ADD, AstNode::make_var(1), AstNode::make_var(2));
    auto add2=AstNode::make_binary(OpType::ADD, AstNode::make_var(1), AstNode::make_var(3));
    auto mul=AstNode::make_binary(OpType::MUL, add1, add2);
    auto vars=ExprParser::collect_vars(mul);
    std::set<int32_t> expected_set={1,2,3}, actual(vars.begin(), vars.end());
    bool pass=(expected_set==actual); std::cout<<"  ["<<(pass?"✓ 通过":"✗ 失败")<<"] 收集变量\n"; return pass;
}

int main() {
    std::cout<<"==========================================================\n  Task-02 ExprParser 单元测试\n==========================================================\n";
    int passed=0,total=6;
    if(test_simple_add())passed++;if(test_nested_complex())passed++;if(test_roundtrip())passed++;
    if(test_illegal_party())passed++;if(test_depth())passed++;if(test_collect_vars())passed++;
    std::cout<<"\n==========================================================\n  结果: "<<passed<<" / "<<total<<" 个测试通过\n==========================================================\n";
    return (passed==total)?0:1;
}
