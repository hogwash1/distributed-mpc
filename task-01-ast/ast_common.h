// ast_common.h — AST 接口契约
// 定义 7 种 OpType、AstNode 结构体、工厂方法、调试输出和深拷贝

#ifndef AST_COMMON_H
#define AST_COMMON_H

#include <cstdint>
#include <memory>
#include <string>
#include <sstream>

namespace mpc {

// 操作类型 — 覆盖所有基础算术运算
enum class OpType : int32_t {
    ADD       = 0,   // 加法
    SUB       = 1,   // 减法
    MUL       = 2,   // 乘法
    NEGATE    = 3,   // 取负（一元）
    CONST     = 4,   // 明文常数
    VAR       = 5,   // 参与方变量
    DIV_CONST = 6,   // 除以常数 = ×(1/常数)
};

// AST 节点 — 递归定义的抽象语法树
struct AstNode {
    OpType op;
    double  const_value   = 0.0;
    int32_t var_party_id  = 0;
    std::shared_ptr<AstNode> lhs, rhs;

    // 工厂方法
    static std::shared_ptr<AstNode> make_const(double v) {
        auto n = std::make_shared<AstNode>();
        n->op = OpType::CONST; n->const_value = v; return n;
    }
    static std::shared_ptr<AstNode> make_var(int32_t party) {
        auto n = std::make_shared<AstNode>();
        n->op = OpType::VAR; n->var_party_id = party; return n;
    }
    static std::shared_ptr<AstNode> make_binary(OpType op, std::shared_ptr<AstNode> l, std::shared_ptr<AstNode> r) {
        auto n = std::make_shared<AstNode>();
        n->op = op; n->lhs = l; n->rhs = r; return n;
    }
    static std::shared_ptr<AstNode> make_unary(OpType op, std::shared_ptr<AstNode> operand) {
        auto n = std::make_shared<AstNode>();
        n->op = op; n->lhs = operand; return n;
    }
    static std::shared_ptr<AstNode> make_div_const(std::shared_ptr<AstNode> operand, double divisor) {
        auto n = std::make_shared<AstNode>();
        n->op = OpType::DIV_CONST; n->const_value = divisor; n->lhs = operand; return n;
    }

    // 递归缩进打印
    std::string to_string(int indent = 0) const {
        std::ostringstream oss;
        std::string pad(indent * 2, ' ');
        switch (op) {
        case OpType::ADD:
            oss << pad << "ADD(\n" << lhs->to_string(indent+1) << ",\n" << rhs->to_string(indent+1) << "\n" << pad << ")"; break;
        case OpType::SUB:
            oss << pad << "SUB(\n" << lhs->to_string(indent+1) << ",\n" << rhs->to_string(indent+1) << "\n" << pad << ")"; break;
        case OpType::MUL:
            oss << pad << "MUL(\n" << lhs->to_string(indent+1) << ",\n" << rhs->to_string(indent+1) << "\n" << pad << ")"; break;
        case OpType::NEGATE:
            oss << pad << "NEGATE(\n" << (lhs ? lhs->to_string(indent+1) : "null") << "\n" << pad << ")"; break;
        case OpType::CONST:
            oss << pad << "CONST(" << const_value << ")"; break;
        case OpType::VAR:
            oss << pad << "VAR(参与方" << var_party_id << ")"; break;
        case OpType::DIV_CONST:
            oss << pad << "DIV_CONST(\n" << lhs->to_string(indent+1) << ",\n" << pad << "  ÷" << const_value << "\n" << pad << ")"; break;
        default: oss << pad << "未知";
        }
        return oss.str();
    }

    // 深拷贝
    std::shared_ptr<AstNode> clone() const {
        auto n = std::make_shared<AstNode>();
        n->op = op; n->const_value = const_value; n->var_party_id = var_party_id;
        if (lhs) n->lhs = lhs->clone();
        if (rhs) n->rhs = rhs->clone();
        return n;
    }
};

} // namespace mpc
#endif
