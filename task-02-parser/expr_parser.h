// expr_parser.h — JSON 表达式解析器
// Task 02：JSON 字符串 → AST 语法树，零外部依赖

#ifndef EXPR_PARSER_H
#define EXPR_PARSER_H

#include "ast_common.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace mpc {

class ExprParser {
public:
    // JSON → AST
    static std::shared_ptr<AstNode> parse_json(const std::string& json);
    // AST → JSON
    static std::string to_json(const std::shared_ptr<AstNode>& node);
    // 校验参与方编号范围
    static std::string validate(const std::shared_ptr<AstNode>& node, int num_parties);
    // 计算最大乘法深度
    static int compute_depth(const std::shared_ptr<AstNode>& node);
    // 收集所有唯一变量编号
    static std::vector<int32_t> collect_vars(const std::shared_ptr<AstNode>& node);
};

class ExprParseError : public std::runtime_error {
public:
    explicit ExprParseError(const std::string& msg) : std::runtime_error(msg) {}
};

} // namespace mpc
#endif
