// multiparty_keygen.h — 多方联合密钥生成
// 链式 MultipartyKeyGen：Party 1 → Party 2 → ... → Party N 生成联合公钥
// 所有参与方用同一把 pk_joint 加密数据

#ifndef MULTIPARTY_KEYGEN_H
#define MULTIPARTY_KEYGEN_H

#include <openfhe.h>
#include <vector>
#include <stdexcept>

using namespace lbcrypto;

namespace mpc {

// 联合密钥生成结果
struct JointKeyResult {
    CryptoContext<DCRTPoly> cc;             // 共享的 CryptoContext
    PublicKey<DCRTPoly>  joint_pk;          // 联合公钥（所有方共用加密）
    std::vector<PrivateKey<DCRTPoly>> sk_list;  // 各方的私钥（用于阈值解密）
    bool success = false;
    std::string error;
};

// 生成 N 方联合密钥（同一进程内模拟）
// 用法: auto result = generate_joint_key(3 /* 3 个参与方 */);
inline JointKeyResult generate_joint_key(int num_parties,
    uint32_t multDepth = 4, uint32_t batchSize = 16, uint32_t scaleModSize = 50) {

    JointKeyResult result;
    try {
        // 1. 初始化 CKKS (uniform parameters for multiparty)
        CCParams<CryptoContextCKKSRNS> params;
        params.SetMultiplicativeDepth(multDepth);
        params.SetScalingModSize(scaleModSize);
        params.SetBatchSize(batchSize);
        params.SetRingDim(16384);  // 符合HE标准的环维度
        params.SetSecurityLevel(HEStd_128_classic);

        auto cc = GenCryptoContext(params);
        cc->Enable(PKE);
        cc->Enable(KEYSWITCH);
        cc->Enable(LEVELEDSHE);
        cc->Enable(ADVANCEDSHE);
        cc->Enable(MULTIPARTY);
        result.cc = cc;

        // 2. Party 1: KeyGen + eval keys
        auto kp1 = cc->KeyGen();
        cc->EvalMultKeyGen(kp1.secretKey);
        cc->EvalSumKeyGen(kp1.secretKey);
        auto pk_prev = kp1.publicKey;
        result.sk_list.push_back(kp1.secretKey);

        // 3. Party 2..N: MultipartyKeyGen + EvalKeys
        for (int i = 2; i <= num_parties; i++) {
            auto kp = cc->MultipartyKeyGen(pk_prev);
            pk_prev = kp.publicKey;
            result.sk_list.push_back(kp.secretKey);
            // Generate eval keys for this party's contribution
            cc->EvalMultKeyGen(kp.secretKey);
            cc->EvalSumKeyGen(kp.secretKey);
        }

        // 5. Joint public key
        result.joint_pk = pk_prev;
        result.success = true;

    } catch (const std::exception& e) {
        result.error = e.what();
    }
    return result;
}

} // namespace mpc
#endif