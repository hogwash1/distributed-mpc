//==================================================================================
// Compute Server 主程序
// 
// 关键改进：联合公钥由所有参与方共同生成
// 服务器不再单独生成密钥对，只负责收集和聚合公钥
//==================================================================================

#include "compute_server.h"
#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>

std::atomic<bool> g_shutdown{false};

void signal_handler(int signal) {
    std::cout << "\n[Server] 收到退出信号..." << std::endl;
    g_shutdown = true;
}

int main(int argc, char** argv) {
    std::cout << "===== Compute Server (分布式同态计算服务器) =====" << std::endl;
    std::cout << "[Server] 联合公钥由所有参与方共同生成" << std::endl;
    
    // 设置信号处理
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // 服务器配置
    std::string server_address = "0.0.0.0:60001";
    int32_t expected_parties = 3;
    
    // 从命令行参数读取配置
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            server_address = "0.0.0.0:" + std::string(argv[++i]);
        } else if (arg == "--parties" && i + 1 < argc) {
            expected_parties = std::atoi(argv[++i]);
        } else if (arg == "--help") {
            std::cout << "用法: compute_server [选项]\n"
                      << "选项:\n"
                      << "  --port <端口>     服务器监听端口 (默认: 60001)\n"
                      << "  --parties <数量>  期望的参与方数量 (默认: 3)\n"
                      << "  --help            显示帮助信息\n";
            return 0;
        }
    }
    
    std::cout << "[Server] 配置: 端口=" << server_address 
              << ", 参与方数=" << expected_parties << std::endl;
    
    // 创建服务器状态
    ServerState state;
    state.expected_parties = expected_parties;
    
    // 初始化加密上下文（服务器需要 CryptoContext 来反序列化密文）
    std::cout << "[Server] 初始化加密上下文..." << std::endl;
    CCParams<CryptoContextCKKSRNS> params;
    params.SetMultiplicativeDepth(4);
    params.SetScalingModSize(50);
    params.SetBatchSize(16);
    
    state.cc = GenCryptoContext(params);
    state.cc->Enable(PKE);
    state.cc->Enable(KEYSWITCH);
    state.cc->Enable(LEVELEDSHE);
    state.cc->Enable(ADVANCEDSHE);
    state.cc->Enable(MULTIPARTY);
    
    std::cout << "[Server] CKKS 环维度: " << state.cc->GetRingDimension() << std::endl;
    
    // 注意：服务器不再生成密钥对
    // 联合公钥将由所有参与方共同生成
    std::cout << "[Server] 等待参与方提交公钥..." << std::endl;
    
    // 创建并启动服务器
    ComputeServerGrpcService service(&state);
    
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<Server> server(builder.BuildAndStart());
    
    if (!server) {
        std::cerr << "[Server] 服务器启动失败!" << std::endl;
        return 1;
    }
    
    std::cout << "[Server] 服务器已启动，监听 " << server_address << std::endl;
    std::cout << "[Server] 等待参与方连接..." << std::endl;
    std::cout << "[Server] 按 Ctrl+C 退出" << std::endl;
    
    // 等待退出信号
    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // 定期打印状态
        std::lock_guard<std::mutex> lock(state.mutex);
        int connected = 0;
        int ciphertexts = 0;
        for (const auto& [id, party] : state.parties) {
            if (party.registered) connected++;
            if (party.ciphertext_received) ciphertexts++;
        }
        
        // 只在状态变化时打印
        static int last_connected = 0;
        static int last_ciphertexts = 0;
        static int last_pk_received = 0;
        
        if (connected != last_connected || ciphertexts != last_ciphertexts || 
            state.public_keys_received != last_pk_received) {
            std::cout << "[Server] 状态: 已连接=" << connected 
                      << "/" << expected_parties 
                      << ", 公钥=" << state.public_keys_received
                      << ", 密文=" << ciphertexts
                      << ", 联合公钥=" << (state.joint_key_ready ? "就绪" : "等待") << std::endl;
            last_connected = connected;
            last_ciphertexts = ciphertexts;
            last_pk_received = state.public_keys_received;
        }
    }
    
    std::cout << "[Server] 正在关闭..." << std::endl;
    server->Shutdown();
    
    std::cout << "[Server] 服务器已关闭" << std::endl;
    return 0;
}
