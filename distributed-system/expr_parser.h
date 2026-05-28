#ifndef EXPR_PARSER_H
#define EXPR_PARSER_H

#include "ast_common.h"
#include <string>
#include <stdexcept>
#include <vector>
#include <memory>

namespace mpc {

class ExprParseError : public std::runtime_error {
public:
    explicit ExprParseError(const std::string& msg)
        : std::runtime_error(msg) {}
};

class ExprParser {
public:
    // Day 1: 解析与反序列化
    static std::shared_ptr<AstNode> parse_json(const std::string& json_str);
    static std::string to_json(const std::shared_ptr<AstNode>& node);

    // Day 2: 验证与属性计算
    static bool validate(const std::shared_ptr<AstNode>& node, int num_parties);
    static int compute_depth(const std::shared_ptr<AstNode>& node);
    static std::vector<int32_t> collect_vars(const std::shared_ptr<AstNode>& node);
};

} // namespace mpc

#endif // EXPR_PARSER_H