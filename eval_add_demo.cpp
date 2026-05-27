#include <fideslib.hpp>
#include <iostream>
#include <iomanip>

using namespace fideslib;

int main() {
    std::cout << "===== FIDESlib CKKS 加密加法演示 =====" << std::endl;
    std::cout << "计算: EvalAdd(Enc(2), Enc(3))" << std::endl;
    std::cout << std::endl;

    // Step 1: 创建 CKKS 加密上下文
    uint32_t multDepth    = 1;
    uint32_t scaleModSize = 50;
    uint32_t batchSize    = 8;

    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetBatchSize(batchSize);
    parameters.SetDevices({ 0 });
    parameters.SetPlaintextAutoload(false);
    parameters.SetCiphertextAutoload(true);

    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

    // Step 1.1: 启用方案特性
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);

    std::cout << "CKKS 环维度: " << cc->GetRingDimension() << std::endl;
    std::cout << std::endl;

    // Step 2: 密钥生成
    auto keys = cc->KeyGen();
    cc->LoadContext(keys.publicKey);

    std::cout << "密钥生成完成" << std::endl;

    // Step 3: 编码与加密
    // CKKS 是 SIMD 方案，需要填满 batch，第一个元素为我们的输入值
    std::vector<double> inputA = { 2.0, 0, 0, 0, 0, 0, 0, 0 };
    std::vector<double> inputB = { 3.0, 0, 0, 0, 0, 0, 0, 0 };

    Plaintext ptxtA = cc->MakeCKKSPackedPlaintext(inputA);
    Plaintext ptxtB = cc->MakeCKKSPackedPlaintext(inputB);

    std::cout << "明文 A: " << std::fixed << std::setprecision(1) << 2.0 << std::endl;
    std::cout << "明文 B: " << std::fixed << std::setprecision(1) << 3.0 << std::endl;

    // 加密: Enc(2) 和 Enc(3)
    auto cipherA = cc->Encrypt(keys.publicKey, ptxtA);  // Enc(2)
    auto cipherB = cc->Encrypt(keys.publicKey, ptxtB);  // Enc(3)

    std::cout << "加密完成: cipherA = Enc(2), cipherB = Enc(3)" << std::endl;
    std::cout << std::endl;

    // Step 4: 同态加法 — EvalAdd(Enc(2), Enc(3))
    auto cipherResult = cc->EvalAdd(cipherA, cipherB);

    std::cout << "同态加法完成: EvalAdd(Enc(2), Enc(3))" << std::endl;
    std::cout << std::endl;

    // Step 5: 解密并输出结果
    Plaintext result;
    cc->Decrypt(keys.secretKey, cipherResult, &result);
    result->SetLength(batchSize);

    std::cout.precision(8);
    std::cout << "===== 解密结果 =====" << std::endl;
    std::cout << "EvalAdd(Enc(2), Enc(3)) = " << result->GetRealPackedValue()[0] << std::endl;
    std::cout << "期望值: 5.0" << std::endl;
    std::cout << "估计精度: " << result->GetLogPrecision() << " bits" << std::endl;

    // 验证
    double decrypted = result->GetRealPackedValue()[0];
    double error = std::abs(decrypted - 5.0);
    std::cout << std::endl;
    if (error < 0.01) {
        std::cout << "验证通过! 解密结果 " << decrypted
                  << " 接近期望值 5.0 (误差: " << error << ")" << std::endl;
    } else {
        std::cout << "验证失败! 解密结果 " << decrypted
                  << " 偏离期望值 5.0 (误差: " << error << ")" << std::endl;
    }

    return 0;
}
