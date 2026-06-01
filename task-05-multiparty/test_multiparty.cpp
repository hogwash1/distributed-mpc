// test_multiparty.cpp — 多方密钥+阈值解密测试 (5 tests)

#include "ast_common.h"
#include "he_evaluator.h"
#include "multiparty_keygen.h"
#include "threshold_decrypt.h"
#include <iostream>
#include <vector>
#include <cmath>

using namespace lbcrypto;
using namespace mpc;

void test_header(const std::string& n) {
    std::cout << "\n==========================================================\n  测试: " << n << "\n==========================================================\n";
}

double max_err(const std::vector<double>& a, const std::vector<double>& b) {
    double e = 0;
    for (size_t i = 0; i < std::min(a.size(), b.size()); i++)
        e = std::max(e, std::abs(a[i] - b[i]));
    return e;
}

// 测试1：2 方密钥生成
bool test_2party_keygen() {
    test_header("2 方联合密钥生成");
    auto r = generate_joint_key(2);
    bool pass = r.success && r.sk_list.size() == 2;
    std::cout << "  [" << (pass ? "✓ 通过" : "✗ 失败") << "] 2 方密钥生成 (sk_count=" << r.sk_list.size() << ")\n";
    return pass;
}

// 测试2：3 方密钥生成
bool test_3party_keygen() {
    test_header("3 方联合密钥生成");
    auto r = generate_joint_key(3);
    bool pass = r.success && r.sk_list.size() == 3;
    std::cout << "  [" << (pass ? "✓ 通过" : "✗ 失败") << "] 3 方密钥生成 (sk_count=" << r.sk_list.size() << ")\n";
    return pass;
}

// 测试3：5 方密钥生成
bool test_5party_keygen() {
    test_header("5 方联合密钥生成");
    auto r = generate_joint_key(5);
    bool pass = r.success && r.sk_list.size() == 5;
    std::cout << "  [" << (pass ? "✓ 通过" : "✗ 失败") << "] 5 方密钥生成 (sk_count=" << r.sk_list.size() << ")\n";
    return pass;
}

// 测试4：多方加密+计算+解密
bool test_multiparty_compute() {
    test_header("3 方加密+计算+解密");
    auto r = generate_joint_key(3);
    if (!r.success) { std::cout << "  ✗ 密钥生成失败\n"; return false; }

    auto d1 = std::vector<double>(16, 10.0);
    auto d2 = std::vector<double>(16, 30.0);
    auto d3 = std::vector<double>(16, 5.0);
    auto ct1 = r.cc->Encrypt(r.joint_pk, r.cc->MakeCKKSPackedPlaintext(d1));
    auto ct2 = r.cc->Encrypt(r.joint_pk, r.cc->MakeCKKSPackedPlaintext(d2));
    auto ct3 = r.cc->Encrypt(r.joint_pk, r.cc->MakeCKKSPackedPlaintext(d3));

    std::map<int32_t, Ciphertext<DCRTPoly>> vars;
    vars[1] = ct1; vars[2] = ct2; vars[3] = ct3;
    HeEvaluator eval(r.cc, r.joint_pk, vars);
    auto ast = AstNode::make_binary(OpType::ADD,
        AstNode::make_var(1), AstNode::make_var(2));
    auto result_ct = r.cc->ModReduce(eval.evaluate(ast));

    auto vals = threshold_decrypt_one(result_ct, r.sk_list, r.cc);
    double err = max_err(vals, std::vector<double>(16, 40.0));
    bool pass = err < 1e-4;
    std::cout << "  [" << (pass ? "✓ 通过" : "✗ 失败") << "] 3 方计算 data1+data2  err=" << std::scientific << err << "\n";
    return pass;
}

// 测试5：混合表达式 (A×B + C)
bool test_multiparty_mixed() {
    test_header("3 方混合 A×B+C");
    auto r = generate_joint_key(3);
    if (!r.success) { std::cout << "  ✗ 密钥生成失败\n"; return false; }

    auto encrypt = [&](auto& data) {
        return r.cc->Encrypt(r.joint_pk, r.cc->MakeCKKSPackedPlaintext(data));
    };

    std::vector<double> d1(16, 2.0), d2(16, 3.0), d3(16, 10.0);
    auto ct1 = encrypt(d1), ct2 = encrypt(d2), ct3 = encrypt(d3);

    HeEvaluator eval(r.cc, r.joint_pk, {{1, ct1}, {2, ct2}, {3, ct3}});
    auto ast = AstNode::make_binary(OpType::ADD,
        AstNode::make_binary(OpType::MUL, AstNode::make_var(1), AstNode::make_var(2)),
        AstNode::make_var(3));
    auto result_ct = r.cc->ModReduce(eval.evaluate(ast));

    auto vals = threshold_decrypt_one(result_ct, r.sk_list, r.cc);
    double err = max_err(vals, std::vector<double>(16, 16.0));
    bool pass = err < 1e-4;
    std::cout << "  [" << (pass ? "✓ 通过" : "✗ 失败") << "] A×B+C  err=" << std::scientific << err << "\n";
    return pass;
}

int main() {
    std::cout << "==========================================================\n  Task-05 多方计算测试\n==========================================================\n";
    int passed = 0, total = 5;
    if (test_2party_keygen()) passed++;
    if (test_3party_keygen()) passed++;
    if (test_5party_keygen()) passed++;
    if (test_multiparty_compute()) passed++;
    if (test_multiparty_mixed()) passed++;
    std::cout << "\n==========================================================\n  结果: " << passed << " / " << total << " 个测试通过\n==========================================================\n";
    return (passed == total) ? 0 : 1;
}
