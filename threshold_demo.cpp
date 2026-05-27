//==================================================================================
// 阈值同态加密演示 - 直接使用 OpenFHE
// 演示内容: 分布式密钥生成、联合公钥构造、阈值解密
//==================================================================================

#include <openfhe.h>
#include <iostream>
#include <iomanip>
#include <vector>

using namespace lbcrypto;

void PrintSeparator(const std::string& title) {
    std::cout << "\n===== " << title << " =====\n" << std::endl;
}

int main() {
    PrintSeparator("阈值同态加密演示 - CKKS 方案 (OpenFHE)");
    std::cout << "本演示展示以下内容:\n";
    std::cout << "  1. 分布式密钥生成 (2个参与方)\n";
    std::cout << "  2. 联合公钥构造\n";
    std::cout << "  3. 阈值解密 (需要双方协作)\n";

    ////////////////////////////////////////////////////////////
    // 步骤 1: 设置加密上下文
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 1: 设置加密上下文");

    uint32_t multDepth    = 3;   // 乘法深度
    uint32_t scaleModSize = 50;  // 缩放模大小
    uint32_t batchSize    = 16;  // 批处理大小

    CCParams<CryptoContextCKKSRNS> parameters;
    parameters.SetMultiplicativeDepth(multDepth);
    parameters.SetScalingModSize(scaleModSize);
    parameters.SetBatchSize(batchSize);

    CryptoContext<DCRTPoly> cc = GenCryptoContext(parameters);

    // 启用功能特性
    cc->Enable(PKE);           // 公钥加密
    cc->Enable(KEYSWITCH);     // 密钥切换
    cc->Enable(LEVELEDSHE);    // 分层同态加密
    cc->Enable(ADVANCEDSHE);   // 高级同态加密
    cc->Enable(MULTIPARTY);    // 多方计算

    std::cout << "CKKS 环维度: " << cc->GetRingDimension() << std::endl;
    std::cout << "多方计算功能已启用." << std::endl;

    ////////////////////////////////////////////////////////////
    // 步骤 2: 分布式密钥生成
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 2: 分布式密钥生成");

    // 第一轮: 参与方 A 生成自己的密钥对
    std::cout << "第一轮: 参与方 A 生成自己的密钥对...\n";
    KeyPair<DCRTPoly> kp1 = cc->KeyGen();
    std::cout << "  参与方 A 的公钥已生成.\n";
    std::cout << "  参与方 A 的私钥份额 s_a 已生成.\n";

    // 为参与方 A 生成评估乘法密钥
    auto evalMultKey = cc->KeySwitchGen(kp1.secretKey, kp1.secretKey);
    std::cout << "  参与方 A 的 evalMultKey 已生成.\n";

    // 为参与方 A 生成评估求和密钥
    cc->EvalSumKeyGen(kp1.secretKey);
    auto evalSumKeys = std::make_shared<std::map<usint, EvalKey<DCRTPoly>>>(cc->GetEvalSumKeyMap(kp1.secretKey->GetKeyTag()));
    std::cout << "  参与方 A 的 evalSumKeys 已生成.\n";

    // 第二轮: 参与方 B 使用参与方 A 的公钥生成自己的密钥份额
    std::cout << "\n第二轮: 参与方 B 生成自己的密钥份额...\n";
    std::cout << "  参与方 B 调用 MultipartyKeyGen() 使用参与方 A 的公钥...\n";

    KeyPair<DCRTPoly> kp2 = cc->MultipartyKeyGen(kp1.publicKey);
    std::cout << "  参与方 B 的私钥份额 s_b 已生成.\n";
    std::cout << "  联合公钥 pk_joint (对应 s_a + s_b) 已构造.\n";

    ////////////////////////////////////////////////////////////
    // 步骤 3: 联合公钥构造 (聚合)
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 3: 联合公钥构造");

    std::cout << "联合公钥 pk_joint = pk_a + pk_b 在 MultipartyKeyGen() 过程中\n";
    std::cout << "已经自动构造完成.\n";
    std::cout << "该密钥可用于加密需要双方协作才能解密的消息.\n";

    // 生成联合评估密钥
    std::cout << "\n生成联合评估密钥...\n";

    // 联合评估乘法密钥
    auto evalMultKey2 = cc->MultiKeySwitchGen(kp2.secretKey, kp2.secretKey, evalMultKey);
    auto evalMultAB = cc->MultiAddEvalKeys(evalMultKey, evalMultKey2, kp2.publicKey->GetKeyTag());
    std::cout << "  联合 evalMultKey (对应 s_a + s_b) 已生成.\n";

    // 转换为 (s_a + s_b)^2 密钥
    auto evalMultBAB = cc->MultiMultEvalKey(kp2.secretKey, evalMultAB, kp2.publicKey->GetKeyTag());
    auto evalMultAAB = cc->MultiMultEvalKey(kp1.secretKey, evalMultAB, kp2.publicKey->GetKeyTag());
    auto evalMultFinal = cc->MultiAddEvalMultKeys(evalMultAAB, evalMultBAB, evalMultAB->GetKeyTag());
    cc->InsertEvalMultKey({evalMultFinal});
    std::cout << "  最终 evalMultKey 已插入上下文.\n";

    // 联合评估求和密钥
    auto evalSumKeysB = cc->MultiEvalSumKeyGen(kp2.secretKey, evalSumKeys, kp2.publicKey->GetKeyTag());
    auto evalSumKeysJoin = cc->MultiAddEvalSumKeys(evalSumKeys, evalSumKeysB, kp2.publicKey->GetKeyTag());
    cc->InsertEvalSumKey(evalSumKeysJoin);
    std::cout << "  联合 evalSumKeys 已插入上下文.\n";

    ////////////////////////////////////////////////////////////
    // 步骤 4: 使用联合公钥加密
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 4: 使用联合公钥加密");

    std::vector<double> vectorOfValues1 = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0};
    std::vector<double> vectorOfValues2 = {0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 6.5, 7.0, 7.5, 8.0};

    Plaintext plaintext1 = cc->MakeCKKSPackedPlaintext(vectorOfValues1);
    Plaintext plaintext2 = cc->MakeCKKSPackedPlaintext(vectorOfValues2);

    std::cout << "明文 1: ";
    for (size_t i = 0; i < 8 && i < vectorOfValues1.size(); i++) {
        std::cout << vectorOfValues1[i] << " ";
    }
    std::cout << "...\n";

    std::cout << "明文 2: ";
    for (size_t i = 0; i < 8 && i < vectorOfValues2.size(); i++) {
        std::cout << vectorOfValues2[i] << " ";
    }
    std::cout << "...\n";

    // 使用联合公钥加密 (kp2.publicKey 是联合密钥)
    Ciphertext<DCRTPoly> ciphertext1 = cc->Encrypt(kp2.publicKey, plaintext1);
    Ciphertext<DCRTPoly> ciphertext2 = cc->Encrypt(kp2.publicKey, plaintext2);

    std::cout << "\n已使用联合公钥加密.\n";
    std::cout << "双方必须协作才能解密.\n";

    ////////////////////////////////////////////////////////////
    // 步骤 5: 同态运算
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 5: 同态运算");

    // 加法
    auto ciphertextAdd = cc->EvalAdd(ciphertext1, ciphertext2);
    std::cout << "EvalAdd(ciphertext1, ciphertext2) 完成.\n";

    // 乘法
    auto ciphertextMult = cc->EvalMult(ciphertext1, ciphertext2);
    auto ciphertextMultRescaled = cc->ModReduce(ciphertextMult);
    std::cout << "EvalMult(ciphertext1, ciphertext2) 完成.\n";

    ////////////////////////////////////////////////////////////
    // 步骤 6: 阈值解密
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 6: 阈值解密");

    std::cout << "解密加法结果...\n\n";

    // 参与方 A 执行部分解密 (主导方)
    std::cout << "参与方 A 执行 MultipartyDecryptLead()...\n";
    auto ciphertextPartial1 = cc->MultipartyDecryptLead({ciphertextAdd}, kp1.secretKey);

    // 参与方 B 执行部分解密 (参与方)
    std::cout << "参与方 B 执行 MultipartyDecryptMain()...\n";
    auto ciphertextPartial2 = cc->MultipartyDecryptMain({ciphertextAdd}, kp2.secretKey);

    // 收集部分解密结果
    std::vector<Ciphertext<DCRTPoly>> partialCiphertextVec;
    partialCiphertextVec.push_back(ciphertextPartial1[0]);
    partialCiphertextVec.push_back(ciphertextPartial2[0]);

    // 融合部分解密结果
    std::cout << "融合部分解密结果...\n";
    Plaintext plaintextResult;
    DecryptResult result = cc->MultipartyDecryptFusion(partialCiphertextVec, &plaintextResult);

    if (result.isValid) {
        plaintextResult->SetLength(batchSize);
        std::cout << "\n解密成功!\n";
        std::cout << "结果 (前8个元素): ";
        for (size_t i = 0; i < 8 && i < plaintextResult->GetRealPackedValue().size(); i++) {
            std::cout << std::fixed << std::setprecision(4) << plaintextResult->GetRealPackedValue()[i] << " ";
        }
        std::cout << "...\n";

        // 验证
        std::cout << "\n期望值 (前8个元素): ";
        for (size_t i = 0; i < 8 && i < vectorOfValues1.size(); i++) {
            std::cout << std::fixed << std::setprecision(4) << (vectorOfValues1[i] + vectorOfValues2[i]) << " ";
        }
        std::cout << "...\n";
    } else {
        std::cout << "解密失败!\n";
    }

    ////////////////////////////////////////////////////////////
    // 步骤 7: 验证乘法结果
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 7: 验证乘法结果");

    std::cout << "解密乘法结果...\n\n";

    // 参与方 A 执行部分解密 (主导方)
    auto partialMult1 = cc->MultipartyDecryptLead({ciphertextMultRescaled}, kp1.secretKey);
    // 参与方 B 执行部分解密 (参与方)
    auto partialMult2 = cc->MultipartyDecryptMain({ciphertextMultRescaled}, kp2.secretKey);

    std::vector<Ciphertext<DCRTPoly>> partialMultVec;
    partialMultVec.push_back(partialMult1[0]);
    partialMultVec.push_back(partialMult2[0]);

    Plaintext plaintextMultResult;
    result = cc->MultipartyDecryptFusion(partialMultVec, &plaintextMultResult);

    if (result.isValid) {
        plaintextMultResult->SetLength(batchSize);
        std::cout << "乘法结果 (前8个元素): ";
        for (size_t i = 0; i < 8 && i < plaintextMultResult->GetRealPackedValue().size(); i++) {
            std::cout << std::fixed << std::setprecision(4) << plaintextMultResult->GetRealPackedValue()[i] << " ";
        }
        std::cout << "...\n";

        std::cout << "期望值 (前8个元素): ";
        for (size_t i = 0; i < 8 && i < vectorOfValues1.size(); i++) {
            std::cout << std::fixed << std::setprecision(4) << (vectorOfValues1[i] * vectorOfValues2[i]) << " ";
        }
        std::cout << "...\n";
    }

    ////////////////////////////////////////////////////////////
    // 总结
    ////////////////////////////////////////////////////////////
    PrintSeparator("总结");
    std::cout << "本演示成功展示了:\n";
    std::cout << "  1. 分布式密钥生成: 参与方 A 和参与方 B 各自生成了\n";
    std::cout << "     自己的私钥份额 (s_a 和 s_b).\n";
    std::cout << "  2. 联合公钥构造: 在 MultipartyKeyGen() 过程中构造了\n";
    std::cout << "     对应 (s_a + s_b) 的联合公钥.\n";
    std::cout << "  3. 阈值解密: 双方执行了部分解密,\n";
    std::cout << "     并融合结果得到明文.\n";
    std::cout << "\n阈值方案确保任何单方都无法单独解密消息 -\n";
    std::cout << "双方必须协作才能完成解密.\n";

    return 0;
}
