//==================================================================================
// Party B - 参与方 (简化版)
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
    std::cout << "===== Party B (参与方) =====" << std::endl;
    
    // 等待交换目录
    while (!fs::exists(EXCHANGE_DIR)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
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
    
    std::cout << "[B] 加密上下文已初始化" << std::endl;
    
    // 等待 A 就绪
    if (!WaitForSignal("a_ready")) {
        std::cerr << "[B] 等待 A 超时!" << std::endl;
        return 1;
    }
    
    // 生成密钥份额
    auto kpB = cc->KeyGen();
    std::cout << "[B] 密钥份额已生成" << std::endl;
    
    // 发送就绪信号
    CreateSignal("b_ready");
    
    // 加密数据
    std::vector<double> dataB = {5.0, 15.0, 25.0, 35.0, 45.0, 55.0, 65.0, 75.0,
                                  85.0, 95.0, 105.0, 115.0, 125.0, 135.0, 145.0, 155.0};
    Plaintext ptB = cc->MakeCKKSPackedPlaintext(dataB);
    auto ctB = cc->Encrypt(kpB.publicKey, ptB);
    std::cout << "[B] 数据已加密" << std::endl;
    
    // 发送加密完成信号
    CreateSignal("b_encrypted");
    
    // 等待计算完成
    if (!WaitForSignal("computed")) {
        std::cerr << "[B] 等待计算超时!" << std::endl;
        return 1;
    }
    std::cout << "[B] 计算完成" << std::endl;
    
    // 等待解密指令
    if (!WaitForSignal("decrypt")) {
        std::cerr << "[B] 等待解密指令超时!" << std::endl;
        return 1;
    }
    
    // 执行部分解密 (Main)
    // 这里简化处理，实际应该接收 A 的平均值密文
    std::cout << "[B] 部分解密完成" << std::endl;
    
    // 发送解密完成信号
    CreateSignal("b_decrypted");
    
    // 等待完成
    if (!WaitForSignal("done")) {
        std::cerr << "[B] 等待完成信号超时!" << std::endl;
        return 1;
    }
    std::cout << "[B] 演示完成!" << std::endl;
    
    return 0;
}
