#ifndef AST_COMMON_H
#define AST_COMMON_H

#include <memory>
#include <vector>

namespace mpc {

// 支持的操作类型
enum class OpType {
    ADD,
    SUB,
    MUL,
    NEGATE,
    CONST,
    VAR,
    DIV_CONST
};

// 抽象语法树节点
struct AstNode {
    OpType type;
    std::shared_ptr<AstNode> left = nullptr;
    std::shared_ptr<AstNode> right = nullptr;

    double value = 0.0;    // 用于 CONST 节点的值
    int32_t party_id = 0;  // 用于 VAR 节点的 party 标识
};

} // namespace mpc

#endif // AST_COMMON_H