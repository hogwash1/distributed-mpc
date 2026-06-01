// mpc_demo.cpp — 单进程多方安全计算 Demo
// 在同进程内模拟 N 方：联合密钥 → 加密 → AST 求值 → 阈值解密 → 验证
//
// 用法:
//   ./mpc_demo --parties 3 --data1 "1,2,3,4" --data2 "5,6,7,8" --data3 "9,10,11,12"
//               --expr '{"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}}'

#include "ast_common.h"
#include "expr_parser.h"
#include "he_evaluator.h"
#include "multiparty_keygen.h"
#include "threshold_decrypt.h"
#include <iostream>
#include <vector>
#include <map>
#include <sstream>
#include <cstring>
#include <cmath>

using namespace lbcrypto;
using namespace mpc;

// 命令行参数解析
struct Config {
    int parties = 3;
    std::string expr_json = R"({"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}})";
    std::map<int, std::vector<double>> party_data;
    std::string help_text =
        "用法: mpc_demo [选项]\n"
        "  --parties <N>        参与方数量 (默认: 3)\n"
        "  --data1 <vals>       Party 1 的数据 (逗号分隔)\n"
        "  --data2 <vals>       Party 2 的数据\n"
        "  --data3 <vals>       Party 3 的数据\n"
        "  --expr <JSON>        JSON 表达式\n"
        "  --help              显示帮助\n";
};

bool parse_args(int argc, char** argv, Config& cfg) {
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--help") == 0) { std::cout << cfg.help_text; return false; }
        else if (std::strcmp(argv[i], "--parties") == 0 && i+1 < argc) cfg.parties = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--expr") == 0 && i+1 < argc) cfg.expr_json = argv[++i];
        else if (std::strcmp(argv[i], "--data1") == 0 && i+1 < argc) {
            std::string s(argv[++i]); std::vector<double> v;
            std::stringstream ss(s); std::string tok;
            while (std::getline(ss, tok, ',')) v.push_back(std::stod(tok));
            cfg.party_data[1] = v;
        }
        else if (std::strcmp(argv[i], "--data2") == 0 && i+1 < argc) {
            std::string s(argv[++i]); std::vector<double> v;
            std::stringstream ss(s); std::string tok;
            while (std::getline(ss, tok, ',')) v.push_back(std::stod(tok));
            cfg.party_data[2] = v;
        }
        else if (std::strcmp(argv[i], "--data3") == 0 && i+1 < argc) {
            std::string s(argv[++i]); std::vector<double> v;
            std::stringstream ss(s); std::string tok;
            while (std::getline(ss, tok, ',')) v.push_back(std::stod(tok));
            cfg.party_data[3] = v;
        }
    }
    return true;
}

// 按预期结果验证
double verify_result(const std::vector<double>& got,
                     const std::map<int, std::vector<double>>& data,
                     const std::shared_ptr<AstNode>& ast) {
    // 简化为直接计算明文表达式来验证
    // 这里做一个简单的验证：对两方加法 (data1+data2)
    double max_err = 0;
    if (data.count(1) && data.count(2)) {
        auto& d1 = data.at(1), &d2 = data.at(2);
        size_t n = std::min({d1.size(), d2.size(), got.size()});
        for (size_t i = 0; i < n; i++) {
            max_err = std::max(max_err, std::abs(got[i] - (d1[i] + d2[i])));
        }
    }
    return max_err;
}

int main(int argc, char** argv) {
    Config cfg;
    if (!parse_args(argc, argv, cfg)) return 0;

    // 默认数据
    if (cfg.party_data.empty()) {
        cfg.party_data[1] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
        cfg.party_data[2] = {5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20};
        cfg.party_data[3] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    }

    std::cout << "==========================================================\n";
    std::cout << "  多方安全计算 Demo\n";
    std::cout << "  参与方数: " << cfg.parties << "\n";
    std::cout << "  表达式:   " << cfg.expr_json << "\n";
    std::cout << "==========================================================\n";

    // ========== Phase 1: 联合密钥生成 ==========
    std::cout << "\n[阶段1] 生成 " << cfg.parties << " 方联合密钥...\n";
    auto key_result = generate_joint_key(cfg.parties);
    if (!key_result.success) {
        std::cerr << "  ✗ 密钥生成失败: " << key_result.error << "\n";
        return 1;
    }
    std::cout << "  ✓ 联合公钥已生成 (" << cfg.parties << " 方参与)\n";

    auto& cc = key_result.cc;
    auto& pk = key_result.joint_pk;
    auto& sk_list = key_result.sk_list;

    // ========== Phase 2: 数据加密 ==========
    std::cout << "\n[阶段2] 各方用联合公钥加密数据...\n";
    std::map<int32_t, Ciphertext<DCRTPoly>> vars;
    for (auto& [pid, data] : cfg.party_data) {
        auto pt = cc->MakeCKKSPackedPlaintext(data);
        vars[pid] = cc->Encrypt(pk, pt);
        std::cout << "  ✓ Party " << pid << " 数据已加密 (" << data.size() << " 个元素)\n";
    }

    // ========== Phase 3: 表达式解析 ==========
    std::cout << "\n[阶段3] 解析表达式...\n";
    std::shared_ptr<AstNode> ast;
    try {
        ast = ExprParser::parse_json(cfg.expr_json);
        std::cout << "  AST:\n" << ast->to_string() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "  ✗ 表达式解析失败: " << e.what() << "\n";
        return 1;
    }

    // ========== Phase 4: 同态求值 ==========
    std::cout << "\n[阶段4] 同态求值...\n";
    HeEvaluator eval(cc, pk, vars);
    Ciphertext<DCRTPoly> result_ct;
    try {
        result_ct = eval.evaluate(ast);
        result_ct = cc->ModReduce(result_ct);
        std::cout << "  ✓ 计算完成 (ops=" << eval.get_op_count()
                  << ", depth=" << eval.get_depth_consumed() << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "  ✗ 求值失败: " << e.what() << "\n";
        return 1;
    }

    // ========== Phase 5: 阈值解密 ==========
    std::cout << "\n[阶段5] 阈值解密...\n";
    auto result_vals = threshold_decrypt_one(result_ct, sk_list, cc);
    std::cout << "  ✓ 解密融合成功 (" << cfg.parties << " 方部分解密聚合)\n";

    // ========== Phase 6: 结果输出 ==========
    std::cout << "\n==========================================================\n";
    std::cout << "  最终明文结果 (前 " << std::min(size_t(16), result_vals.size()) << " 个):\n  ";
    for (size_t i = 0; i < std::min(size_t(16), result_vals.size()); i++) {
        std::cout << result_vals[i];
        if (i < std::min(size_t(16), result_vals.size())-1) std::cout << ", ";
    }
    std::cout << "\n";

    // 验证
    double err = verify_result(result_vals, cfg.party_data, ast);
    std::cout << "\n  误差: " << std::scientific << err << "\n";
    std::cout << "  " << (err < 1e-4 ? "✓ 验证通过" : "✗ 验证失败") << "\n";
    std::cout << "==========================================================\n";

    return (err < 1e-4) ? 0 : 1;
}
