// ast_serializer.h -- AST ↔ Proto bidirectional conversion
// Task 01: protocol extension — serialization layer
//
// Converts between C++ AstNode tree and Protobuf AstNodeProto message
// for gRPC transport across the distributed MPC system.

#ifndef AST_SERIALIZER_H
#define AST_SERIALIZER_H

#include "ast_common.h"
#include "distributed_mpc.pb.h"

namespace mpc {

class AstSerializer {
public:
    // -------------------------------------------------------------------
    // C++ AST → Proto message (serialize)
    // Recursively walks the AST tree and populates an AstNodeProto.
    // -------------------------------------------------------------------
    static distributed_mpc::AstNodeProto serialize(
        const std::shared_ptr<AstNode>& node);

    // -------------------------------------------------------------------
    // Proto message → C++ AST (deserialize)
    // Recursively rebuilds the AST tree from an AstNodeProto.
    // -------------------------------------------------------------------
    static std::shared_ptr<AstNode> deserialize(
        const distributed_mpc::AstNodeProto& proto);

private:
    // Map C++ OpType → Proto OpType
    static distributed_mpc::OpType to_proto_op(OpType op);

    // Map Proto OpType → C++ OpType
    static OpType from_proto_op(distributed_mpc::OpType op);
};

// =======================================================================
// Implementation (header-only for simplicity)
// =======================================================================

inline distributed_mpc::OpType AstSerializer::to_proto_op(OpType op) {
    switch (op) {
        case OpType::ADD:       return distributed_mpc::OP_ADD;
        case OpType::SUB:       return distributed_mpc::OP_SUB;
        case OpType::MUL:       return distributed_mpc::OP_MUL;
        case OpType::NEGATE:    return distributed_mpc::OP_NEGATE;
        case OpType::CONST:     return distributed_mpc::OP_CONST;
        case OpType::VAR:       return distributed_mpc::OP_VAR;
        case OpType::DIV_CONST: return distributed_mpc::OP_DIV_CONST;
        default:
            throw std::runtime_error("AstSerializer: unknown OpType in to_proto_op");
    }
}

inline OpType AstSerializer::from_proto_op(distributed_mpc::OpType op) {
    switch (op) {
        case distributed_mpc::OP_ADD:       return OpType::ADD;
        case distributed_mpc::OP_SUB:       return OpType::SUB;
        case distributed_mpc::OP_MUL:       return OpType::MUL;
        case distributed_mpc::OP_NEGATE:    return OpType::NEGATE;
        case distributed_mpc::OP_CONST:     return OpType::CONST;
        case distributed_mpc::OP_VAR:       return OpType::VAR;
        case distributed_mpc::OP_DIV_CONST: return OpType::DIV_CONST;
        default:
            throw std::runtime_error("AstSerializer: unknown proto OpType in from_proto_op");
    }
}

inline distributed_mpc::AstNodeProto AstSerializer::serialize(
    const std::shared_ptr<AstNode>& node) {
    distributed_mpc::AstNodeProto proto;
    if (!node) return proto;  // empty proto for null node

    proto.set_op(to_proto_op(node->op));

    switch (node->op) {
    case OpType::CONST:
    case OpType::DIV_CONST:
        proto.set_const_value(node->const_value);
        break;
    case OpType::VAR:
        proto.set_var_party_id(node->var_party_id);
        break;
    default:
        break;
    }

    // Recursively serialize children
    if (node->lhs) {
        *proto.mutable_lhs() = serialize(node->lhs);
    }
    if (node->rhs && node->op != OpType::NEGATE) {
        // NEGATE ignores rhs, but we still serialize if present
        *proto.mutable_rhs() = serialize(node->rhs);
    }

    return proto;
}

inline std::shared_ptr<AstNode> AstSerializer::deserialize(
    const distributed_mpc::AstNodeProto& proto) {
    auto node = std::make_shared<AstNode>();
    node->op = from_proto_op(proto.op());

    switch (node->op) {
    case OpType::CONST:
    case OpType::DIV_CONST:
        node->const_value = proto.const_value();
        break;
    case OpType::VAR:
        node->var_party_id = proto.var_party_id();
        break;
    default:
        break;
    }

    // Recursively deserialize children
    if (proto.has_lhs()) {
        node->lhs = deserialize(proto.lhs());
    }
    if (proto.has_rhs()) {
        node->rhs = deserialize(proto.rhs());
    }

    return node;
}

} // namespace mpc

#endif // AST_SERIALIZER_H
