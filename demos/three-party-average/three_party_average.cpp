//==================================================================================
// 三方阈值同态加密求平均值演示
// 演示内容: 三方分布式密钥生成、联合公钥构造、同态求平均值、三方阈值解密
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
    PrintSeparator("三方阈值同态加密求平均值演示 - CKKS 方案");
    std::cout << "本演示展示以下内容:\n";
    std::cout << "  1. 三方分布式密钥生成 (Party A, B, C)\n";
    std::cout << "  2. 联合公钥构造\n";
    std::cout << "  3. 三方各自加密数据\n";
    std::cout << "  4. 同态计算: 平均值 = (数据A + 数据B + 数据C) / 3\n";
    std::cout << "  5. 三方阈值解密\n";

    ////////////////////////////////////////////////////////////
    // 步骤 1: 设置加密上下文
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 1: 设置加密上下文");

    uint32_t multDepth    = 3;   // 乘法深度 (除以1/3需要一次乘法)
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
    // 步骤 2: 三方分布式密钥生成
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 2: 三方分布式密钥生成");

    // 第一轮: Party A 生成初始密钥对
    std::cout << "第一轮: Party A 生成初始密钥对...\n";
    KeyPair<DCRTPoly> kpA = cc->KeyGen();
    std::cout << "  Party A 的公钥 pk_a 已生成.\n";
    std::cout << "  Party A 的私钥份额 s_a 已生成.\n";

    // 为 Party A 生成评估乘法密钥和求和密钥
    auto evalMultKeyA = cc->KeySwitchGen(kpA.secretKey, kpA.secretKey);
    cc->EvalSumKeyGen(kpA.secretKey);
    auto evalSumKeysA = std::make_shared<std::map<usint, EvalKey<DCRTPoly>>>(
        cc->GetEvalSumKeyMap(kpA.secretKey->GetKeyTag()));
    std::cout << "  Party A 的评估密钥已生成.\n";

    // 第二轮: Party B 使用 Party A 的公钥生成自己的密钥份额
    std::cout << "\n第二轮: Party B 使用 Party A 的公钥生成密钥份额...\n";
    KeyPair<DCRTPoly> kpB = cc->MultipartyKeyGen(kpA.publicKey);
    std::cout << "  Party B 的私钥份额 s_b 已生成.\n";
    std::cout << "  联合公钥 pk_ab (对应 s_a + s_b) 已构造.\n";

    // 第三轮: Party C 使用 A+B 的联合公钥生成自己的密钥份额
    std::cout << "\n第三轮: Party C 使用联合公钥 pk_ab 生成密钥份额...\n";
    KeyPair<DCRTPoly> kpC = cc->MultipartyKeyGen(kpB.publicKey);
    std::cout << "  Party C 的私钥份额 s_c 已生成.\n";
    std::cout << "  联合公钥 pk_abc (对应 s_a + s_b + s_c) 已构造.\n";

    ////////////////////////////////////////////////////////////
    // 步骤 3: 联合公钥构造 (聚合评估密钥)
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 3: 联合评估密钥构造");

    std::cout << "生成三方联合评估密钥...\n\n";

    // --- Party B 的评估密钥 ---
    auto evalMultKeyB = cc->MultiKeySwitchGen(kpB.secretKey, kpB.secretKey, evalMultKeyA);
    auto evalMultAB = cc->MultiAddEvalKeys(evalMultKeyA, evalMultKeyB, kpB.publicKey->GetKeyTag());
    auto evalMultBAB = cc->MultiMultEvalKey(kpB.secretKey, evalMultAB, kpB.publicKey->GetKeyTag());
    auto evalMultAAB = cc->MultiMultEvalKey(kpA.secretKey, evalMultAB, kpB.publicKey->GetKeyTag());
    auto evalMultFinalAB = cc->MultiAddEvalMultKeys(evalMultAAB, evalMultBAB, evalMultAB->GetKeyTag());

    auto evalSumKeysB = cc->MultiEvalSumKeyGen(kpB.secretKey, evalSumKeysA, kpB.publicKey->GetKeyTag());
    auto evalSumKeysAB = cc->MultiAddEvalSumKeys(evalSumKeysA, evalSumKeysB, kpB.publicKey->GetKeyTag());

    std::cout << "  Party A + Party B 的联合评估密钥已生成.\n";

    // --- Party C 的评估密钥 ---
    auto evalMultKeyC = cc->MultiKeySwitchGen(kpC.secretKey, kpC.secretKey, evalMultFinalAB);
    auto evalMultABC = cc->MultiAddEvalKeys(evalMultFinalAB, evalMultKeyC, kpC.publicKey->GetKeyTag());
    auto evalMultCBC = cc->MultiMultEvalKey(kpC.secretKey, evalMultABC, kpC.publicKey->GetKeyTag());
    auto evalMultAAC = cc->MultiMultEvalKey(kpA.secretKey, evalMultABC, kpC.publicKey->GetKeyTag());
    auto evalMultBBC = cc->MultiMultEvalKey(kpB.secretKey, evalMultABC, kpC.publicKey->GetKeyTag());
    auto evalMultFinalABC = cc->MultiAddEvalMultKeys(evalMultAAC, evalMultCBC, evalMultAAC->GetKeyTag());
    evalMultFinalABC = cc->MultiAddEvalMultKeys(evalMultFinalABC, evalMultBBC, evalMultFinalABC->GetKeyTag());
    cc->InsertEvalMultKey({evalMultFinalABC});

    auto evalSumKeysC = cc->MultiEvalSumKeyGen(kpC.secretKey, evalSumKeysAB, kpC.publicKey->GetKeyTag());
    auto evalSumKeysABC = cc->MultiAddEvalSumKeys(evalSumKeysAB, evalSumKeysC, kpC.publicKey->GetKeyTag());
    cc->InsertEvalSumKey(evalSumKeysABC);

    std::cout << "  Party A + Party B + Party C 的联合评估密钥已生成.\n";
    std::cout << "  联合公钥 pk_abc 可用于加密, 需要三方协作才能解密.\n";

    ////////////////////////////////////////////////////////////
    // 步骤 4: 三方各自加密数据
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 4: 三方各自加密数据");

    // 三方各自拥有不同的数据
    std::vector<double> dataA = {10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0,
                                  90.0, 100.0, 110.0, 120.0, 130.0, 140.0, 150.0, 160.0};
    std::vector<double> dataB = {5.0,  15.0, 25.0, 35.0, 45.0, 55.0, 65.0, 75.0,
                                  85.0, 95.0,  105.0, 115.0, 125.0, 135.0, 145.0, 155.0};
    std::vector<double> dataC = {15.0, 25.0, 35.0, 45.0, 55.0, 65.0, 75.0, 85.0,
                                  95.0, 105.0, 115.0, 125.0, 135.0, 145.0, 155.0, 165.0};

    Plaintext ptA = cc->MakeCKKSPackedPlaintext(dataA);
    Plaintext ptB = cc->MakeCKKSPackedPlaintext(dataB);
    Plaintext ptC = cc->MakeCKKSPackedPlaintext(dataC);

    std::cout << "Party A 的数据 (前8个): ";
    for (size_t i = 0; i < 8; i++) std::cout << dataA[i] << " ";
    std::cout << "...\n";

    std::cout << "Party B 的数据 (前8个): ";
    for (size_t i = 0; i < 8; i++) std::cout << dataB[i] << " ";
    std::cout << "...\n";

    std::cout << "Party C 的数据 (前8个): ";
    for (size_t i = 0; i < 8; i++) std::cout << dataC[i] << " ";
    std::cout << "...\n";

    // 使用联合公钥 kpC.publicKey 加密 (这是三方的联合公钥)
    Ciphertext<DCRTPoly> ctA = cc->Encrypt(kpC.publicKey, ptA);
    Ciphertext<DCRTPoly> ctB = cc->Encrypt(kpC.publicKey, ptB);
    Ciphertext<DCRTPoly> ctC = cc->Encrypt(kpC.publicKey, ptC);

    std::cout << "\n三方数据已使用联合公钥 pk_abc 加密.\n";
    std::cout << "任何单方都无法单独解密 - 需要三方协作.\n";

    ////////////////////////////////////////////////////////////
    // 步骤 5: 同态计算平均值
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 5: 同态计算平均值");

    std::cout << "计算公式: 平均值 = (数据A + 数据B + 数据C) / 3\n\n";

    // 同态加法: sum = Enc(dataA) + Enc(dataB) + Enc(dataC)
    std::cout << "执行同态加法: sum = Enc(A) + Enc(B) + Enc(C)...\n";
    auto sum = cc->EvalAdd(cc->EvalAdd(ctA, ctB), ctC);
    std::cout << "  同态加法完成.\n";

    // 同态标量乘法: avg = sum * (1/3)
    double inv3 = 1.0 / 3.0;
    std::cout << "执行同态乘法: avg = sum * (1.0/3.0)...\n";
    auto avg = cc->EvalMult(sum, inv3);
    auto avgRescaled = cc->ModReduce(avg);
    std::cout << "  同态乘法 (除以3) 完成.\n";

    ////////////////////////////////////////////////////////////
    // 步骤 6: 三方阈值解密
    ////////////////////////////////////////////////////////////
    PrintSeparator("步骤 6: 三方阈值解密");

    std::cout << "解密平均值结果...\n\n";

    // Party A 执行部分解密 (主导方 Lead)
    std::cout << "Party A 执行 MultipartyDecryptLead()...\n";
    auto partialA = cc->MultipartyDecryptLead({avgRescaled}, kpA.secretKey);

    // Party B 执行部分解密 (参与方 Main)
    std::cout << "Party B 执行 MultipartyDecryptMain()...\n";
    auto partialB = cc->MultipartyDecryptMain({avgRescaled}, kpB.secretKey);

    // Party C 执行部分解密 (参与方 Main)
    std::cout << "Party C 执行 MultipartyDecryptMain()...\n";
    auto partialC = cc->MultipartyDecryptMain({avgRescaled}, kpC.secretKey);

    // 收集三方部分解密结果
    std::vector<Ciphertext<DCRTPoly>> partialVec;
    partialVec.push_back(partialA[0]);
    partialVec.push_back(partialB[0]);
    partialVec.push_back(partialC[0]);

    // 融合三方部分解密结果
    std::cout << "融合三方部分解密结果...\n";
    Plaintext plaintextResult;
    DecryptResult result = cc->MultipartyDecryptFusion(partialVec, &plaintextResult);

    if (result.isValid) {
        plaintextResult->SetLength(batchSize);
        std::cout << "\n解密成功!\n\n";

        // 输出结果
        std::cout << "同态计算的平均值结果 (前8个元素):\n  ";
        for (size_t i = 0; i < 8 && i < plaintextResult->GetRealPackedValue().size(); i++) {
            std::cout << std::fixed << std::setprecision(4) << plaintextResult->GetRealPackedValue()[i] << " ";
        }
        std::cout << "...\n\n";

        // 输出期望值
        std::cout << "期望的平均值 (前8个元素):\n  ";
        for (size_t i = 0; i < 8 && i < dataA.size(); i++) {
            std::cout << std::fixed << std::setprecision(4) << (dataA[i] + dataB[i] + dataC[i]) / 3.0 << " ";
        }
        std::cout << "...\n\n";

        // 验证误差
        std::cout << "误差 (前8个元素):\n  ";
        double maxError = 0.0;
        for (size_t i = 0; i < 8 && i < plaintextResult->GetRealPackedValue().size(); i++) {
            double expected = (dataA[i] + dataB[i] + dataC[i]) / 3.0;
            double actual = plaintextResult->GetRealPackedValue()[i];
            double err = std::abs(actual - expected);
            if (err > maxError) maxError = err;
            std::cout << std::scientific << std::setprecision(2) << err << " ";
        }
        std::cout << "\n\n";
        std::cout << "最大误差: " << std::scientific << maxError << std::endl;

        if (maxError < 0.01) {
            std::cout << "验证通过! 同态计算的平均值与期望值一致.\n";
        } else {
            std::cout << "注意: 误差较大, 可能需要调整参数.\n";
        }
    } else {
        std::cout << "解密失败!\n";
    }

    ////////////////////////////////////////////////////////////
    // 总结
    ////////////////////////////////////////////////////////////
    PrintSeparator("总结");
    std::cout << "本演示成功展示了三方隐私计算求平均值的完整流程:\n\n";
    std::cout << "  1. 三方分布式密钥生成:\n";
    std::cout << "     Party A 生成初始密钥 -> Party B 加入 -> Party C 加入\n";
    std::cout << "     联合私钥: s = s_a + s_b + s_c\n\n";
    std::cout << "  2. 联合公钥构造:\n";
    std::cout << "     pk_abc = pk_a + pk_b + pk_c\n";
    std::cout << "     使用该公钥加密的数据需要三方协作才能解密\n\n";
    std::cout << "  3. 同态计算平均值:\n";
    std::cout << "     avg = (Enc(A) + Enc(B) + Enc(C)) * (1/3)\n";
    std::cout << "     全程在密文状态下计算, 各方原始数据未泄露\n\n";
    std::cout << "  4. 三方阈值解密:\n";
    std::cout << "     Party A (Lead) + Party B (Main) + Party C (Main)\n";
    std::cout << "     三方各自执行部分解密, 融合后得到最终结果\n\n";
    std::cout << "应用场景: 多机构联合统计分析、隐私保护的数据聚合等.\n";

    return 0;
}
