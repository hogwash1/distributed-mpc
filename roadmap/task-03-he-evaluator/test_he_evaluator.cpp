// test_he_evaluator.cpp -- Unit tests for HeEvaluator
// Task 03, 5 test cases, single-process CKKS (no gRPC needed)
//
// Test 1: two-party addition         data1 + data2
// Test 2: three-party average        (data1+data2+data3)/3
// Test 3: polynomial                 data1^2 + data1*2 + 1
// Test 4: depth tracking             2-layer multiplication => depth == 2
// Test 5: missing variable           throw HeEvalError

#include "he_evaluator.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cassert>

using namespace lbcrypto;
using namespace mpc;

// ---------------------------------------------------------------------------
// Helper: set up CKKS CryptoContext with MULTIPARTY
// ---------------------------------------------------------------------------
CryptoContext<DCRTPoly> setup_ckks(uint32_t multDepth = 4,
                                   uint32_t scaleModSize = 50,
                                   uint32_t batchSize = 16) {
    CCParams<CryptoContextCKKSRNS> params;
    params.SetMultiplicativeDepth(multDepth);
    params.SetScalingModSize(scaleModSize);
    params.SetBatchSize(batchSize);

    auto cc = GenCryptoContext(params);
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);
    return cc;
}

// ---------------------------------------------------------------------------
// Helper: generate evaluation keys (needed for EvalMult)
// ---------------------------------------------------------------------------
void generate_eval_keys(CryptoContext<DCRTPoly> cc,
                        PrivateKey<DCRTPoly> sk) {
    cc->EvalMultKeyGen(sk);
    cc->EvalSumKeyGen(sk);
}

// ---------------------------------------------------------------------------
// Helper: encrypt a vector with a given public key
// ---------------------------------------------------------------------------
Ciphertext<DCRTPoly> encrypt_vector(CryptoContext<DCRTPoly> cc,
                                     PublicKey<DCRTPoly> pk,
                                     const std::vector<double>& data) {
    auto pt = cc->MakeCKKSPackedPlaintext(data);
    return cc->Encrypt(pk, pt);
}

// ---------------------------------------------------------------------------
// Helper: decrypt a ciphertext (single-key, non-threshold)
// ---------------------------------------------------------------------------
std::vector<double> decrypt_vector(CryptoContext<DCRTPoly> cc,
                                    PrivateKey<DCRTPoly> sk,
                                    const Ciphertext<DCRTPoly>& ct) {
    Plaintext pt;
    cc->Decrypt(sk, ct, &pt);
    pt->SetLength(cc->GetRingDimension() / 2);
    return pt->GetRealPackedValue();
}

// ---------------------------------------------------------------------------
// Helper: max error between two vectors
// ---------------------------------------------------------------------------
double max_error(const std::vector<double>& a, const std::vector<double>& b) {
    double max_err = 0.0;
    size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; i++) {
        double err = std::abs(a[i] - b[i]);
        if (err > max_err) max_err = err;
    }
    return max_err;
}

// ---------------------------------------------------------------------------
// Helper: print pass/fail
// ---------------------------------------------------------------------------
void test_header(const std::string& name) {
    std::cout << "\n==========================================================\n";
    std::cout << "  TEST: " << name << "\n";
    std::cout << "==========================================================\n";
}

bool report(const std::string& name, double error, double threshold = 1e-4) {
    bool pass = error < threshold;
    std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] " << name
              << "  max_error = " << std::scientific << std::setprecision(2)
              << error << "  (threshold=" << threshold << ")\n";
    return pass;
}

// ===================================================================
// Test 1: Two-party addition  data1 + data2
// ===================================================================
bool test_two_party_add() {
    test_header("Two-party Addition: data1 + data2");

    auto cc = setup_ckks();
    auto kp = cc->KeyGen();
    generate_eval_keys(cc, kp.secretKey);
    uint32_t batchSize = 16;

    std::vector<double> data1 = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,
                                  9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0};
    std::vector<double> data2 = {10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0,
                                  90.0, 100.0, 110.0, 120.0, 130.0, 140.0, 150.0, 160.0};

    auto ct1 = encrypt_vector(cc, kp.publicKey, data1);
    auto ct2 = encrypt_vector(cc, kp.publicKey, data2);

    // Build AST: data1 + data2
    auto var1 = AstNode::make_var(1);
    auto var2 = AstNode::make_var(2);
    auto root = AstNode::make_binary(OpType::ADD, var1, var2);

    // Evaluate
    std::map<int32_t, Ciphertext<DCRTPoly>> vars = {{1, ct1}, {2, ct2}};
    HeEvaluator eval(cc, kp.publicKey, vars);
    auto result_ct = eval.evaluate(root);

    std::cout << "  depth_consumed = " << eval.get_depth_consumed()
              << ", op_count = " << eval.get_op_count() << "\n";

    // Decrypt & verify
    auto result = decrypt_vector(cc, kp.secretKey, result_ct);
    std::vector<double> expected(batchSize);
    for (size_t i = 0; i < batchSize; i++) expected[i] = data1[i] + data2[i];

    double err = max_error(result, expected);
    return report("Two-party addition", err);
}

// ===================================================================
// Test 2: Three-party average  (data1 + data2 + data3) / 3
// ===================================================================
bool test_three_party_average() {
    test_header("Three-party Average: (data1+data2+data3)/3");

    auto cc = setup_ckks();
    auto kp = cc->KeyGen();
    generate_eval_keys(cc, kp.secretKey);
    uint32_t batchSize = 16;

    std::vector<double> data1 = {10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0,
                                  90.0, 100.0, 110.0, 120.0, 130.0, 140.0, 150.0, 160.0};
    std::vector<double> data2 = {5.0, 15.0, 25.0, 35.0, 45.0, 55.0, 65.0, 75.0,
                                  85.0, 95.0, 105.0, 115.0, 125.0, 135.0, 145.0, 155.0};
    std::vector<double> data3 = {15.0, 25.0, 35.0, 45.0, 55.0, 65.0, 75.0, 85.0,
                                  95.0, 105.0, 115.0, 125.0, 135.0, 145.0, 155.0, 165.0};

    auto ct1 = encrypt_vector(cc, kp.publicKey, data1);
    auto ct2 = encrypt_vector(cc, kp.publicKey, data2);
    auto ct3 = encrypt_vector(cc, kp.publicKey, data3);

    // Build AST: (data1 + data2 + data3) / 3
    auto var1 = AstNode::make_var(1);
    auto var2 = AstNode::make_var(2);
    auto var3 = AstNode::make_var(3);
    auto add12 = AstNode::make_binary(OpType::ADD, var1, var2);
    auto add123 = AstNode::make_binary(OpType::ADD, add12, var3);
    auto root = AstNode::make_div_const(add123, 3.0);

    // Evaluate
    std::map<int32_t, Ciphertext<DCRTPoly>> vars = {
        {1, ct1}, {2, ct2}, {3, ct3}
    };
    HeEvaluator eval(cc, kp.publicKey, vars);
    auto result_ct = eval.evaluate(root);

    std::cout << "  depth_consumed = " << eval.get_depth_consumed()
              << ", op_count = " << eval.get_op_count() << "\n";

    // Decrypt & verify
    auto result = decrypt_vector(cc, kp.secretKey, result_ct);
    std::vector<double> expected(batchSize);
    for (size_t i = 0; i < batchSize; i++)
        expected[i] = (data1[i] + data2[i] + data3[i]) / 3.0;

    double err = max_error(result, expected);
    return report("Three-party average", err);
}

// ===================================================================
// Test 3: Polynomial  data1^2 + data1*2 + 1
// AST: ADD( ADD( MUL(VAR1, VAR1), MUL(VAR1, CONST2) ), CONST1 )
// ===================================================================
bool test_polynomial() {
    test_header("Polynomial: data1^2 + data1*2 + 1");

    auto cc = setup_ckks(4);  // need depth >= 2 for data1^2 + second MUL
    auto kp = cc->KeyGen();
    generate_eval_keys(cc, kp.secretKey);
    uint32_t batchSize = 16;

    std::vector<double> data1 = {0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0,
                                  4.5, 5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0};
    auto ct1 = encrypt_vector(cc, kp.publicKey, data1);

    // Build AST: data1^2 + data1*2 + 1
    auto var1   = AstNode::make_var(1);
    auto const1 = AstNode::make_const(1.0);
    auto const2 = AstNode::make_const(2.0);

    auto mul_sq   = AstNode::make_binary(OpType::MUL, var1, var1);           // data1^2
    auto mul_2x   = AstNode::make_binary(OpType::MUL, var1, const2);          // data1*2
    auto add_sq2x = AstNode::make_binary(OpType::ADD, mul_sq, mul_2x);       // data1^2 + data1*2
    auto root     = AstNode::make_binary(OpType::ADD, add_sq2x, const1);     // + 1

    // Evaluate
    std::map<int32_t, Ciphertext<DCRTPoly>> vars = {{1, ct1}};
    HeEvaluator eval(cc, kp.publicKey, vars);
    auto result_ct = eval.evaluate(root);

    std::cout << "  depth_consumed = " << eval.get_depth_consumed()
              << ", op_count = " << eval.get_op_count() << "\n";

    // Decrypt & verify
    auto result = decrypt_vector(cc, kp.secretKey, result_ct);
    std::vector<double> expected(batchSize);
    for (size_t i = 0; i < batchSize; i++)
        expected[i] = data1[i] * data1[i] + data1[i] * 2.0 + 1.0;

    double err = max_error(result, expected);
    return report("Polynomial", err);
}

// ===================================================================
// Test 4: Depth tracking -- 2-layer multiplication
// AST: (data1 * data2) * data3   =>  MUL(MUL(VAR1, VAR2), VAR3)
// Expect: depth_consumed_ == 2
// ===================================================================
bool test_depth_tracking() {
    test_header("Depth Tracking: (data1*data2)*data3  =>  depth == 2");

    auto cc = setup_ckks(3);
    auto kp = cc->KeyGen();
    generate_eval_keys(cc, kp.secretKey);
    uint32_t batchSize = 16;

    std::vector<double> d1(batchSize, 2.0);
    std::vector<double> d2(batchSize, 3.0);
    std::vector<double> d3(batchSize, 4.0);

    auto ct1 = encrypt_vector(cc, kp.publicKey, d1);
    auto ct2 = encrypt_vector(cc, kp.publicKey, d2);
    auto ct3 = encrypt_vector(cc, kp.publicKey, d3);

    // Build AST: (data1 * data2) * data3
    auto var1 = AstNode::make_var(1);
    auto var2 = AstNode::make_var(2);
    auto var3 = AstNode::make_var(3);
    auto mul12 = AstNode::make_binary(OpType::MUL, var1, var2);
    auto root  = AstNode::make_binary(OpType::MUL, mul12, var3);

    std::map<int32_t, Ciphertext<DCRTPoly>> vars = {{1, ct1}, {2, ct2}, {3, ct3}};
    HeEvaluator eval(cc, kp.publicKey, vars);
    auto result_ct = eval.evaluate(root);

    std::cout << "  depth_consumed = " << eval.get_depth_consumed()
              << ", op_count = " << eval.get_op_count() << "\n";

    bool pass = (eval.get_depth_consumed() == 2);
    auto result = decrypt_vector(cc, kp.secretKey, result_ct);
    double err = max_error(result, std::vector<double>(batchSize, 2.0 * 3.0 * 4.0));
    pass = pass && (err < 1e-4);

    std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] depth_tracking"
              << "  depth=" << eval.get_depth_consumed()
              << " (expected 2)\n";
    return pass;
}

// ===================================================================
// Test 5: Missing variable -- throw HeEvalError
// AST: data1 + data9  (party 9 not in vars_)
// ===================================================================
bool test_missing_variable() {
    test_header("Missing Variable: data1 + data9 (party 9 not in vars)");

    auto cc = setup_ckks();
    auto kp = cc->KeyGen();
    generate_eval_keys(cc, kp.secretKey);

    std::vector<double> d1(16, 1.0);
    auto ct1 = encrypt_vector(cc, kp.publicKey, d1);

    // Build AST: data1 + data9 (but we only put party 1 in vars)
    auto var1 = AstNode::make_var(1);
    auto var9 = AstNode::make_var(9);
    auto root = AstNode::make_binary(OpType::ADD, var1, var9);

    std::map<int32_t, Ciphertext<DCRTPoly>> vars = {{1, ct1}};
    HeEvaluator eval(cc, kp.publicKey, vars);

    bool threw = false;
    try {
        auto result = eval.evaluate(root);
    } catch (const HeEvalError& e) {
        threw = true;
        std::cout << "  Caught expected exception: " << e.what() << "\n";
    } catch (...) {
        // wrong exception type
    }

    std::cout << "  [" << (threw ? "PASS" : "FAIL") << "] missing_variable"
              << "  threw HeEvalError=" << (threw ? "yes" : "no") << "\n";
    return threw;
}

// ===================================================================
// Main: run all tests
// ===================================================================
int main() {
    std::cout << "==========================================================\n";
    std::cout << "  Task-03 HeEvaluator Unit Tests\n";
    std::cout << "==========================================================\n";

    int passed = 0, total = 5;

    if (test_two_party_add())      passed++;
    if (test_three_party_average()) passed++;
    if (test_polynomial())         passed++;
    if (test_depth_tracking())     passed++;
    if (test_missing_variable())   passed++;

    std::cout << "\n==========================================================\n";
    std::cout << "  RESULTS: " << passed << " / " << total << " tests passed\n";
    std::cout << "==========================================================\n";

    return (passed == total) ? 0 : 1;
}
