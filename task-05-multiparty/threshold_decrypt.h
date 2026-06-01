// threshold_decrypt.h — 阈值解密
// 多方各自产生部分解密，聚合后得到明文结果

#ifndef THRESHOLD_DECRYPT_H
#define THRESHOLD_DECRYPT_H

#include <openfhe.h>
#include <vector>
#include <stdexcept>
#include <cmath>

using namespace lbcrypto;

namespace mpc {

// 便捷封装：解密单个密文（所有方贡献部分解密）
inline std::vector<double> threshold_decrypt_one(
    Ciphertext<DCRTPoly> ct,
    const std::vector<PrivateKey<DCRTPoly>>& sk_list,
    CryptoContext<DCRTPoly> cc) {

    // 收集所有方的部分解密
    std::vector<Ciphertext<DCRTPoly>> partials;
    for (size_t i = 0; i < sk_list.size(); i++) {
        std::vector<Ciphertext<DCRTPoly>> pd;
        if (i == 0) {
            pd = cc->MultipartyDecryptLead({ct}, sk_list[i]);
        } else {
            pd = cc->MultipartyDecryptMain({ct}, sk_list[i]);
        }
        partials.insert(partials.end(), pd.begin(), pd.end());
    }

    // 融合
    Plaintext pt;
    cc->MultipartyDecryptFusion(partials, &pt);
    pt->SetLength(cc->GetEncodingParams()->GetBatchSize());
    return pt->GetRealPackedValue();
}

} // namespace mpc
#endif
