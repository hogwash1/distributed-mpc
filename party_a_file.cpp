//==================================================================================
// Party A - 协调方 (简化版，使用 CryptoContext 序列化)
//==================================================================================

#include <openfhe.h>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>

using namespace lbcrypto;
namespace fs = std::filesystem;

const std::string EXCHANGE_DIR = "/tmp/mpc_exchange";

bool FileExists(const std::string& filename) {
    return fs::exists(EXCHANGE_DIR + "/" + filename);
}

void CreateSignal(const std::string& name) {
    std::ofstream(EXCHANGE_DIR + "/" + name);
}

bool WaitForSignal(const std::string& name, int timeoutSec = 60) {
    for (int i = 0; i < timeoutSec * 10; i++) {
        if (FileExists(name)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

int main() {
    std::cout << "===== Party A (协调方) =====" << std::endl;
    
    fs::create_directories(EXCHANGE_DIR);
    
    // 初始化
    CCParams<CryptoContextCKKSRNS> params;
    params.SetMultiplicativeDepth(3);
    params.SetScalingModSize(50);
    params.SetBatchSize(16);
    auto cc = GenCryptoContext(params);
    cc->Enable(PKE);
    cc->Enable(KEYSWITCH);
    cc->Enable(LEVELEDSHE);
    cc->Enable(ADVANCEDSHE);
    cc->Enable(MULTIPARTY);
    
    std::cout << "[A] 加密上下文已初始化" << std::endl;
    
    // 生成密钥
    auto kpA = cc->KeyGen();
    cc->KeySwitchGen(kpA.secretKey, kpA.secretKey);
    cc->EvalSumKeyGen(kpA.secretKey);
    
    std::cout << "[A] 密钥对已生成，等待 B 和 C..." << std::endl;
    
    // 发送就绪信号
    CreateSignal("a_ready");
    
    // 等待 B 和 C 就绪
    if (!WaitForSignal("b_ready") || !WaitForSignal("c_ready")) {
        std::cerr << "[A] 等待超时!" << std::endl;
        return 1;
    }
    std::cout << "[A] B 和 C 已就绪" << std::endl;
    
    // Party A 的数据
    std::vector<double> dataA = {10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0,
                                  90.0, 100.0, 110.0, 120.0, 130.0, 140.0, 150.0, 160.0};
    
    // 加密
    Plaintext ptA = cc->MakeCKKSPackedPlaintext(dataA);
    auto ctA = cc->Encrypt(kpA.publicKey, ptA);
    std::cout << "[A] 数据已加密" << std::endl;
    
    // 发送加密完成信号
    CreateSignal("a_encrypted");
    
    // 等待 B 和 C 加密完成
    if (!WaitForSignal("b_encrypted") || !WaitForSignal("c_encrypted")) {
        std::cerr << "[A] 等待加密超时!" << std::endl;
        return 1;
    }
    std::cout << "[A] 所有参与方已加密" << std::endl;
    
    // 同态计算平均值 (简化：假设 B 和 C 的数据已知)
    std::vector<double> dataB = {5.0, 15.0, 25.0, 35.0, 45.0, 55.0, 65.0, 75.0,
                                  85.0, 95.0, 105.0, 115.0, 125.0, 135.0, 145.0, 155.0};
    std::vector<double> dataC = {15.0, 25.0, 35.0, 45.0, 55.0, 65.0, 75.0, 85.0,
                                  95.0, 105.0, 115.0, 125.0, 135.0, 145.0, 155.0, 165.0};
    
    Plaintext ptB = cc->MakeCKKSPackedPlaintext(dataB);
    Plaintext ptC = cc->MakeCKKSPackedPlaintext(dataC);
    auto ctB = cc->Encrypt(kpA.publicKey, ptB);
    auto ctC = cc->Encrypt(kpA.publicKey, ptC);
    
    // 同态求和并除以 3
    auto sum = cc->EvalAdd(cc->EvalAdd(ctA, ctB), ctC);
    auto avg = cc->EvalMult(sum, 1.0/3.0);
    avg = cc->ModReduce(avg);
    
    std::cout << "[A] 同态计算完成" << std::endl;
    
    // 发送计算完成信号
    CreateSignal("computed");
    
    // 执行部分解密 (Lead)
    auto partialA = cc->MultipartyDecryptLead({avg}, kpA.secretKey);
    std::cout << "[A] 部分解密完成" << std::endl;
    
    // 发送解密信号
    CreateSignal("decrypt");
    
    // 等待 B 和 C 的部分解密完成
    if (!WaitForSignal("b_decrypted") || !WaitForSignal("c_decrypted")) {
        std::cerr << "[A] 等待部分解密超时!" << std::endl;
        return 1;
    }
    
    // 模拟融合解密 (实际应该读取 B 和 C 的部分解密结果)
    // 这里简化处理，直接解密
    Plaintext result;
    cc->Decrypt(kpA.secretKey, avg, &result);
    result->SetLength(16);
    
    std::cout << "\n===== 最终结果 =====" << std::endl;
    std::cout << "平均值 (前8个): ";
    for (size_t i = 0; i < 8; i++) {
        std::cout << std::fixed << std::setprecision(4) 
                 << result->GetRealPackedValue()[i] << " ";
    }
    std::cout << "..." << std::endl;
    
    std::cout << "\n期望值 (前8个): ";
    for (size_t i = 0; i < 8; i++) {
        double expected = (dataA[i] + dataB[i] + dataC[i]) / 3.0;
        std::cout << std::fixed << std::setprecision(4) << expected << " ";
    }
    std::cout << "..." << std::endl;
    
    CreateSignal("done");
    std::cout << "\n[A] 演示完成!" << std::endl;
    
    return 0;
}
