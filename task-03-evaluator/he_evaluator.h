// he_evaluator.h — 同态计算引擎
// 递归遍历 AST 树，调用 OpenFHE Eval* 操作，返回最终密文结果

#ifndef HE_EVALUATOR_H
#define HE_EVALUATOR_H

#include "ast_common.h"
#include <openfhe.h>
#include <map>
#include <stdexcept>

using namespace lbcrypto;

namespace mpc {

class HeEvaluator {
public:
    // cc: CryptoContext, pk: 公钥(加密 CONST 用), vars: 参与方编号→密文
    HeEvaluator(CryptoContext<DCRTPoly> cc, PublicKey<DCRTPoly> pk,
                const std::map<int32_t, Ciphertext<DCRTPoly>>& vars);

    // 遍历 AST 返回最终密文
    Ciphertext<DCRTPoly> evaluate(const std::shared_ptr<AstNode>& root);

    int get_depth_consumed() const { return depth_consumed_; }
    int get_op_count() const { return op_count_; }

private:
    CryptoContext<DCRTPoly> cc_;
    PublicKey<DCRTPoly> pk_;
    std::map<int32_t, Ciphertext<DCRTPoly>> vars_;
    int depth_consumed_ = 0, op_count_ = 0;
    uint32_t batch_size_;

    Ciphertext<DCRTPoly> eval_node(const std::shared_ptr<AstNode>& node, int current_depth);
    Plaintext make_scalar_plaintext(double value) const;
    static const char* op_name(OpType op);
};

class HeEvalError : public std::runtime_error {
public: explicit HeEvalError(const std::string& msg) : std::runtime_error(msg) {}
};

} // namespace mpc
#endif
