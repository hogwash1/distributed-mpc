// he_evaluator.cpp -- Homomorphic Evaluation Engine Implementation
// Task 03: recursive AST evaluation with Strategy A (plaintext optimization)

#include "he_evaluator.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace mpc {

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
HeEvaluator::HeEvaluator(CryptoContext<DCRTPoly> cc,
                         PublicKey<DCRTPoly> pk,
                         const std::map<int32_t, Ciphertext<DCRTPoly>>& vars)
    : cc_(cc), pk_(pk), vars_(vars), depth_consumed_(0), op_count_(0),
      batch_size_(cc->GetEncodingParams()->GetBatchSize()) {}

// ---------------------------------------------------------------------------
// Helper: create packed plaintext with scalar replicated across all slots
// ---------------------------------------------------------------------------
Plaintext HeEvaluator::make_scalar_plaintext(double value) const {
    return cc_->MakeCKKSPackedPlaintext(
        std::vector<double>(batch_size_, value));
}

// ---------------------------------------------------------------------------
// OpType -> string helper
// ---------------------------------------------------------------------------
const char* HeEvaluator::op_name(OpType op) {
    switch (op) {
        case OpType::ADD:       return "ADD";
        case OpType::SUB:       return "SUB";
        case OpType::MUL:       return "MUL";
        case OpType::NEGATE:    return "NEGATE";
        case OpType::CONST:     return "CONST";
        case OpType::VAR:       return "VAR";
        case OpType::DIV_CONST: return "DIV_CONST";
        default:                return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
Ciphertext<DCRTPoly> HeEvaluator::evaluate(const std::shared_ptr<AstNode>& root) {
    depth_consumed_ = 0;
    op_count_ = 0;
    return eval_node(root, 0);
}

// ---------------------------------------------------------------------------
// Recursive evaluation -- Strategy A: plaintext optimization for CONST
// ---------------------------------------------------------------------------
Ciphertext<DCRTPoly> HeEvaluator::eval_node(const std::shared_ptr<AstNode>& node,
                                            int current_depth) {
    if (!node) {
        throw HeEvalError("eval_node: null node encountered");
    }

    Ciphertext<DCRTPoly> result;

    switch (node->op) {

    // ==================================================================
    // ADD: lhs + rhs
    // Strategy A: if one child is CONST, use EvalAdd(ct, plaintext)
    // ==================================================================
    case OpType::ADD: {
        // If both children are CONST, encrypt one then EvalAdd(ct, plain)
        if (node->lhs->op == OpType::CONST && node->rhs->op == OpType::CONST) {
            auto pt_l = make_scalar_plaintext(node->lhs->const_value);
            auto ct_l = cc_->Encrypt(pk_, pt_l);
            auto pt_r = make_scalar_plaintext(node->rhs->const_value);
            op_count_++;
            std::cout << "[HeEvaluator] ADD (const+const): depth="
                      << depth_consumed_ << ", ops=" << op_count_ << std::endl;
            return cc_->EvalAdd(ct_l, pt_r);
        }
        // CONST on left
        if (node->lhs->op == OpType::CONST) {
            auto rhs_ct = eval_node(node->rhs, current_depth);
            auto pt = make_scalar_plaintext(node->lhs->const_value);
            op_count_++;
            std::cout << "[HeEvaluator] ADD (const+ct): depth="
                      << depth_consumed_ << ", ops=" << op_count_ << std::endl;
            return cc_->EvalAdd(rhs_ct, pt);
        }
        // CONST on right
        if (node->rhs->op == OpType::CONST) {
            auto lhs_ct = eval_node(node->lhs, current_depth);
            auto pt = make_scalar_plaintext(node->rhs->const_value);
            op_count_++;
            std::cout << "[HeEvaluator] ADD (ct+const): depth="
                      << depth_consumed_ << ", ops=" << op_count_ << std::endl;
            return cc_->EvalAdd(lhs_ct, pt);
        }
        // General: ct + ct
        auto lhs_ct = eval_node(node->lhs, current_depth);
        auto rhs_ct = eval_node(node->rhs, current_depth);
        op_count_++;
        std::cout << "[HeEvaluator] ADD: depth=" << depth_consumed_
                  << ", ops=" << op_count_ << std::endl;
        result = cc_->EvalAdd(lhs_ct, rhs_ct);
        break;
    }

    // ==================================================================
    // SUB: lhs - rhs
    // Strategy A: if rhs is CONST, use EvalSub(ct, plaintext)
    // ==================================================================
    case OpType::SUB: {
        // CONST on right: EvalSub(ct, plain)
        if (node->rhs->op == OpType::CONST) {
            auto lhs_ct = eval_node(node->lhs, current_depth);
            auto pt = make_scalar_plaintext(node->rhs->const_value);
            op_count_++;
            std::cout << "[HeEvaluator] SUB (ct-const): depth="
                      << depth_consumed_ << ", ops=" << op_count_ << std::endl;
            return cc_->EvalSub(lhs_ct, pt);
        }
        // CONST on left: EvalSub(plain, ct) -- encrypt the const
        if (node->lhs->op == OpType::CONST) {
            auto rhs_ct = eval_node(node->rhs, current_depth);
            auto pt = make_scalar_plaintext(node->lhs->const_value);
            auto ct = cc_->Encrypt(pk_, pt);
            op_count_++;
            std::cout << "[HeEvaluator] SUB (const-ct): depth="
                      << depth_consumed_ << ", ops=" << op_count_ << std::endl;
            return cc_->EvalSub(ct, rhs_ct);
        }
        // General: ct - ct
        auto lhs_ct = eval_node(node->lhs, current_depth);
        auto rhs_ct = eval_node(node->rhs, current_depth);
        op_count_++;
        std::cout << "[HeEvaluator] SUB: depth=" << depth_consumed_
                  << ", ops=" << op_count_ << std::endl;
        result = cc_->EvalSub(lhs_ct, rhs_ct);
        break;
    }

    // ==================================================================
    // MUL: lhs * rhs -- increments multiplication depth
    // Strategy A: if one child is CONST, use EvalMult(ct, const_value)
    // ==================================================================
    case OpType::MUL: {
        int child_depth = current_depth + 1;
        // CONST on left
        if (node->lhs->op == OpType::CONST) {
            auto rhs_ct = eval_node(node->rhs, child_depth);
            depth_consumed_ = std::max(depth_consumed_, child_depth);
            op_count_++;
            std::cout << "[HeEvaluator] MUL (const*ct): depth="
                      << depth_consumed_ << ", ops=" << op_count_ << std::endl;
            return cc_->EvalMult(rhs_ct, node->lhs->const_value);
        }
        // CONST on right
        if (node->rhs->op == OpType::CONST) {
            auto lhs_ct = eval_node(node->lhs, child_depth);
            depth_consumed_ = std::max(depth_consumed_, child_depth);
            op_count_++;
            std::cout << "[HeEvaluator] MUL (ct*" << node->rhs->const_value
                      << "): depth=" << depth_consumed_ << ", ops=" << op_count_ << std::endl;
            return cc_->EvalMult(lhs_ct, node->rhs->const_value);
        }
        // General: ct * ct
        auto lhs_ct = eval_node(node->lhs, child_depth);
        auto rhs_ct = eval_node(node->rhs, child_depth);
        depth_consumed_ = std::max(depth_consumed_, child_depth);
        op_count_++;
        std::cout << "[HeEvaluator] MUL: depth=" << depth_consumed_
                  << ", ops=" << op_count_ << std::endl;
        result = cc_->EvalMult(lhs_ct, rhs_ct);
        break;
    }

    // ==================================================================
    // NEGATE: -lhs (unary)
    // ==================================================================
    case OpType::NEGATE: {
        auto val = eval_node(node->lhs, current_depth);
        op_count_++;
        std::cout << "[HeEvaluator] NEGATE: depth=" << depth_consumed_
                  << ", ops=" << op_count_ << std::endl;
        result = cc_->EvalNegate(val);
        break;
    }

    // ==================================================================
    // CONST: plaintext constant -- encrypt with public key (Strategy B)
    // This path only reached when CONST is standalone (should be caught
    // by the ADD/SUB/MUL optimisations above for binary ops).
    // ==================================================================
    case OpType::CONST: {
        auto pt = make_scalar_plaintext(node->const_value);
        op_count_++;
        std::cout << "[HeEvaluator] CONST (" << node->const_value
                  << "): depth=" << depth_consumed_
                  << ", ops=" << op_count_ << std::endl;
        result = cc_->Encrypt(pk_, pt);
        break;
    }

    // ==================================================================
    // VAR: party variable -- lookup in vars_ map, return deep copy
    // to prevent EvalMult aliasing from corrupting shared ciphertext.
    // ==================================================================
    case OpType::VAR: {
        auto it = vars_.find(node->var_party_id);
        if (it == vars_.end()) {
            std::ostringstream oss;
            oss << "HeEvalError: variable for party_id="
                << node->var_party_id << " not found in vars map";
            throw HeEvalError(oss.str());
        }
        op_count_++;
        std::cout << "[HeEvaluator] VAR (party " << node->var_party_id
                  << "): depth=" << depth_consumed_
                  << ", ops=" << op_count_ << std::endl;
        // Deep copy to prevent EvalMult from mutating shared ciphertext in vars_
        result = std::make_shared<CiphertextImpl<DCRTPoly>>(*it->second);
        break;
    }

    // ==================================================================
    // DIV_CONST: lhs / const_value  =>  lhs * (1/const_value)
    // Uses EvalMult(ct, scalar) -- NO extra multiplication depth
    // ==================================================================
    case OpType::DIV_CONST: {
        if (std::abs(node->const_value) < 1e-15) {
            throw HeEvalError("DIV_CONST: division by zero");
        }
        auto val = eval_node(node->lhs, current_depth);
        double inv = 1.0 / node->const_value;
        op_count_++;
        std::cout << "[HeEvaluator] DIV_CONST (/" << node->const_value
                  << "): depth=" << depth_consumed_
                  << ", ops=" << op_count_ << std::endl;
        result = cc_->EvalMult(val, inv);
        break;
    }

    default:
        throw HeEvalError("Unknown OpType in eval_node");
    }

    return result;
}

} // namespace mpc
