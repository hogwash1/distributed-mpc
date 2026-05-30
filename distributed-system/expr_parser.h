// expr_parser.h -- JSON Expression Parser
// Task 02: converts JSON expression strings to AST, with validation & analysis

#ifndef EXPR_PARSER_H
#define EXPR_PARSER_H

#include "ast_common.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace mpc {

class ExprParser {
public:
    // Parse a JSON expression string into an AST
    static std::shared_ptr<AstNode> parse_json(const std::string& json);

    // Serialize an AST back to a compact JSON string
    static std::string to_json(const std::shared_ptr<AstNode>& node);

    // Validate AST: check var_party_id range, structural completeness
    // Returns empty string on success, error message on failure
    static std::string validate(const std::shared_ptr<AstNode>& node, int num_parties);

    // Compute maximum multiplication depth of the AST
    static int compute_depth(const std::shared_ptr<AstNode>& node);

    // Collect all unique variable party_ids used in the AST (sorted)
    static std::vector<int32_t> collect_vars(const std::shared_ptr<AstNode>& node);
};

class ExprParseError : public std::runtime_error {
public:
    explicit ExprParseError(const std::string& msg)
        : std::runtime_error(msg) {}
};

} // namespace mpc

#endif // EXPR_PARSER_H
