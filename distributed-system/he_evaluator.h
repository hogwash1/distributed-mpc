// he_evaluator.h -- Homomorphic Evaluation Engine
// Task 03: walks AST, calls OpenFHE Eval* operations, returns final ciphertext

#ifndef HE_EVALUATOR_H
#define HE_EVALUATOR_H

#include "ast_common.h"
#include <openfhe.h>
#include <map>
#include <stdexcept>
#include <string>

using namespace lbcrypto;

namespace mpc {

class HeEvaluator {
public:
    // cc:   CryptoContext (CKKS, MULTIPARTY enabled)
    // pk:   public key for encrypting CONST nodes (Strategy B fallback)
    // vars: party_id -> Ciphertext mapping (e.g. {1 -> ct1, 2 -> ct2, 3 -> ct3})
    HeEvaluator(CryptoContext<DCRTPoly> cc,
                PublicKey<DCRTPoly> pk,
                const std::map<int32_t, Ciphertext<DCRTPoly>>& vars);

    // Walk the AST and return the final ciphertext result
    Ciphertext<DCRTPoly> evaluate(const std::shared_ptr<AstNode>& root);

    // Multiplicative depth consumed during evaluation
    int get_depth_consumed() const { return depth_consumed_; }

    // Total homomorphic operations executed
    int get_op_count() const { return op_count_; }

private:
    CryptoContext<DCRTPoly> cc_;
    PublicKey<DCRTPoly> pk_;
    std::map<int32_t, Ciphertext<DCRTPoly>> vars_;
    int depth_consumed_ = 0;
    int op_count_ = 0;
    uint32_t batch_size_;

    // Recursive evaluation helper
    // current_depth = number of MUL ancestors above this node
    Ciphertext<DCRTPoly> eval_node(const std::shared_ptr<AstNode>& node, int current_depth);

    // Create a packed plaintext with scalar replicated across all slots
    Plaintext make_scalar_plaintext(double value) const;

    // Get string name for an OpType (for logging)
    static const char* op_name(OpType op);
};

class HeEvalError : public std::runtime_error {
public:
    explicit HeEvalError(const std::string& msg)
        : std::runtime_error(msg) {}
};

} // namespace mpc

#endif // HE_EVALUATOR_H
