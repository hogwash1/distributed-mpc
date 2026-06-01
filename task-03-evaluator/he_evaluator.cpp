// he_evaluator.cpp — 同态计算引擎实现
// 策略 A：CONST 节点明文优化，避免无谓加密

#include "he_evaluator.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace mpc {

HeEvaluator::HeEvaluator(CryptoContext<DCRTPoly> cc, PublicKey<DCRTPoly> pk,
                         const std::map<int32_t, Ciphertext<DCRTPoly>>& vars)
    : cc_(cc), pk_(pk), vars_(vars), depth_consumed_(0), op_count_(0),
      batch_size_(cc->GetEncodingParams()->GetBatchSize()) {}

Plaintext HeEvaluator::make_scalar_plaintext(double value) const {
    return cc_->MakeCKKSPackedPlaintext(std::vector<double>(batch_size_, value));
}

const char* HeEvaluator::op_name(OpType op) {
    switch (op) {
        case OpType::ADD: return "加法"; case OpType::SUB: return "减法";
        case OpType::MUL: return "乘法"; case OpType::NEGATE: return "取负";
        case OpType::CONST: return "常数"; case OpType::VAR: return "变量";
        case OpType::DIV_CONST: return "除常数"; default: return "未知";
    }
}

Ciphertext<DCRTPoly> HeEvaluator::evaluate(const std::shared_ptr<AstNode>& root) {
    depth_consumed_ = 0; op_count_ = 0; return eval_node(root, 0);
}

Ciphertext<DCRTPoly> HeEvaluator::eval_node(const std::shared_ptr<AstNode>& node, int current_depth) {
    if (!node) throw HeEvalError("eval_node: 遇到空节点");
    Ciphertext<DCRTPoly> result;
    switch (node->op) {
    case OpType::ADD: {
        if (node->lhs->op == OpType::CONST && node->rhs->op == OpType::CONST) {
            auto ct = cc_->Encrypt(pk_, make_scalar_plaintext(node->lhs->const_value));
            auto pt = make_scalar_plaintext(node->rhs->const_value);
            op_count_++; std::cout << "[HeEvaluator] 加法(常数+常数) depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
            return cc_->EvalAdd(ct, pt);
        }
        if (node->lhs->op == OpType::CONST) {
            auto rhs_ct = eval_node(node->rhs, current_depth);
            auto pt = make_scalar_plaintext(node->lhs->const_value);
            op_count_++; std::cout << "[HeEvaluator] 加法(常数+密文) depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
            return cc_->EvalAdd(rhs_ct, pt);
        }
        if (node->rhs->op == OpType::CONST) {
            auto lhs_ct = eval_node(node->lhs, current_depth);
            auto pt = make_scalar_plaintext(node->rhs->const_value);
            op_count_++; std::cout << "[HeEvaluator] 加法(密文+常数) depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
            return cc_->EvalAdd(lhs_ct, pt);
        }
        auto l = eval_node(node->lhs, current_depth), r = eval_node(node->rhs, current_depth);
        op_count_++; std::cout << "[HeEvaluator] 加法 depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
        result = cc_->EvalAdd(l, r); break;
    }
    case OpType::SUB: {
        if (node->rhs->op == OpType::CONST) {
            auto lhs_ct = eval_node(node->lhs, current_depth);
            auto pt = make_scalar_plaintext(node->rhs->const_value);
            op_count_++; std::cout << "[HeEvaluator] 减法(密文-常数) depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
            return cc_->EvalSub(lhs_ct, pt);
        }
        if (node->lhs->op == OpType::CONST) {
            auto ct = cc_->Encrypt(pk_, make_scalar_plaintext(node->lhs->const_value));
            auto rhs_ct = eval_node(node->rhs, current_depth);
            op_count_++; std::cout << "[HeEvaluator] 减法(常数-密文) depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
            return cc_->EvalSub(ct, rhs_ct);
        }
        auto l = eval_node(node->lhs, current_depth), r = eval_node(node->rhs, current_depth);
        op_count_++; std::cout << "[HeEvaluator] 减法 depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
        result = cc_->EvalSub(l, r); break;
    }
    case OpType::MUL: {
        int cd = current_depth + 1;
        if (node->lhs->op == OpType::CONST) {
            auto rhs_ct = eval_node(node->rhs, cd); depth_consumed_ = std::max(depth_consumed_, cd);
            op_count_++; std::cout << "[HeEvaluator] 乘法(常数×密文) depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
            return cc_->EvalMult(rhs_ct, node->lhs->const_value);
        }
        if (node->rhs->op == OpType::CONST) {
            auto lhs_ct = eval_node(node->lhs, cd); depth_consumed_ = std::max(depth_consumed_, cd);
            op_count_++; std::cout << "[HeEvaluator] 乘法(密文×" << node->rhs->const_value << ") depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
            return cc_->EvalMult(lhs_ct, node->rhs->const_value);
        }
        auto l = eval_node(node->lhs, cd), r = eval_node(node->rhs, cd);
        depth_consumed_ = std::max(depth_consumed_, cd); op_count_++;
        std::cout << "[HeEvaluator] 乘法 depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
        result = cc_->EvalMult(l, r); break;
    }
    case OpType::NEGATE: {
        auto v = eval_node(node->lhs, current_depth); op_count_++;
        std::cout << "[HeEvaluator] 取负 depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
        result = cc_->EvalNegate(v); break;
    }
    case OpType::CONST: {
        op_count_++; std::cout << "[HeEvaluator] 常数(" << node->const_value << ") depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
        result = cc_->Encrypt(pk_, make_scalar_plaintext(node->const_value)); break;
    }
    case OpType::VAR: {
        auto it = vars_.find(node->var_party_id);
        if (it == vars_.end()) { std::ostringstream oss; oss << "HeEvalError: 参与方" << node->var_party_id << "未在 vars 中找到"; throw HeEvalError(oss.str()); }
        op_count_++; std::cout << "[HeEvaluator] 变量(参与方" << node->var_party_id << ") depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
        result = std::make_shared<CiphertextImpl<DCRTPoly>>(*it->second); break;
    }
    case OpType::DIV_CONST: {
        if (std::abs(node->const_value) < 1e-15) throw HeEvalError("DIV_CONST: 除零错误");
        auto v = eval_node(node->lhs, current_depth); op_count_++;
        std::cout << "[HeEvaluator] 除常数(÷" << node->const_value << ") depth=" << depth_consumed_ << " ops=" << op_count_ << std::endl;
        result = cc_->EvalMult(v, 1.0 / node->const_value); break;
    }
    default: throw HeEvalError("不支持的 OpType");
    }
    return result;
}

} // namespace mpc
