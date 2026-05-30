// ast_common.h -- AST interface contract for all Task 1/2/3/4 modules
// Task 01: protocol extension — canonical version with to_string() & clone()
//
// Defines OpType enumeration, AstNode structure, factory methods,
// recursive to_string() for debugging, and deep-copy clone().

#ifndef AST_COMMON_H
#define AST_COMMON_H

#include <cstdint>
#include <memory>
#include <string>
#include <sstream>

namespace mpc {

enum class OpType : int32_t {
    ADD       = 0,   // lhs + rhs
    SUB       = 1,   // lhs - rhs
    MUL       = 2,   // lhs * rhs
    NEGATE    = 3,   // -operand  (unary, uses lhs only)
    CONST     = 4,   // plaintext constant (const_value)
    VAR       = 5,   // party variable   (var_party_id)
    DIV_CONST = 6,   // lhs / constant  => lhs * (1/const_value)
};

struct AstNode {
    OpType op;
    double  const_value   = 0.0;   // CONST / DIV_CONST value
    int32_t var_party_id  = 0;     // VAR party ID

    std::shared_ptr<AstNode> lhs;   // left child
    std::shared_ptr<AstNode> rhs;   // right child (ignored for NEGATE)

    // ================================================================
    // Factory methods
    // ================================================================

    static std::shared_ptr<AstNode> make_const(double v) {
        auto node = std::make_shared<AstNode>();
        node->op = OpType::CONST;
        node->const_value = v;
        return node;
    }

    static std::shared_ptr<AstNode> make_var(int32_t party) {
        auto node = std::make_shared<AstNode>();
        node->op = OpType::VAR;
        node->var_party_id = party;
        return node;
    }

    static std::shared_ptr<AstNode> make_binary(
        OpType op, std::shared_ptr<AstNode> l, std::shared_ptr<AstNode> r) {
        auto node = std::make_shared<AstNode>();
        node->op = op;
        node->lhs = l;
        node->rhs = r;
        return node;
    }

    static std::shared_ptr<AstNode> make_unary(
        OpType op, std::shared_ptr<AstNode> operand) {
        auto node = std::make_shared<AstNode>();
        node->op = op;
        node->lhs = operand;
        return node;
    }

    static std::shared_ptr<AstNode> make_div_const(
        std::shared_ptr<AstNode> operand, double divisor) {
        auto node = std::make_shared<AstNode>();
        node->op = OpType::DIV_CONST;
        node->const_value = divisor;
        node->lhs = operand;
        return node;
    }

    // ================================================================
    // to_string() — recursive debug dump
    // ================================================================
    std::string to_string(int indent = 0) const {
        std::ostringstream oss;
        std::string pad(indent * 2, ' ');
        switch (op) {
        case OpType::ADD:
            oss << pad << "ADD(\n"
                << lhs->to_string(indent + 1) << ",\n"
                << rhs->to_string(indent + 1) << "\n"
                << pad << ")";
            break;
        case OpType::SUB:
            oss << pad << "SUB(\n"
                << lhs->to_string(indent + 1) << ",\n"
                << rhs->to_string(indent + 1) << "\n"
                << pad << ")";
            break;
        case OpType::MUL:
            oss << pad << "MUL(\n"
                << lhs->to_string(indent + 1) << ",\n"
                << rhs->to_string(indent + 1) << "\n"
                << pad << ")";
            break;
        case OpType::NEGATE:
            oss << pad << "NEGATE(\n"
                << (lhs ? lhs->to_string(indent + 1) : "null") << "\n"
                << pad << ")";
            break;
        case OpType::CONST:
            oss << pad << "CONST(" << const_value << ")";
            break;
        case OpType::VAR:
            oss << pad << "VAR(party=" << var_party_id << ")";
            break;
        case OpType::DIV_CONST:
            oss << pad << "DIV_CONST(\n"
                << lhs->to_string(indent + 1) << ",\n"
                << pad << "  /" << const_value << "\n"
                << pad << ")";
            break;
        default:
            oss << pad << "UNKNOWN";
        }
        return oss.str();
    }

    // ================================================================
    // clone() — deep copy the entire AST
    // ================================================================
    std::shared_ptr<AstNode> clone() const {
        auto node = std::make_shared<AstNode>();
        node->op           = op;
        node->const_value  = const_value;
        node->var_party_id = var_party_id;
        if (lhs) node->lhs = lhs->clone();
        if (rhs) node->rhs = rhs->clone();
        return node;
    }
};

} // namespace mpc

#endif // AST_COMMON_H
