//==================================================================================
// Party Client 主程序 - 真正的门限公钥聚合
// 
// 使用 OpenFHE 的链式 MultipartyKeyGen 协议:
// Party 1: KeyGen() → 提交 pk_1 + evalKeys
// Party 2: MultipartyKeyGen(pk_1) → 聚合 evalKeys → 提交 pk_12 + evalKeys
// Party 3: MultipartyKeyGen(pk_12) → 聚合 evalKeys → 提交 pk_123 + evalKeys
// 所有方使用 pk_123 加密，三方协作解密
//==================================================================================

#include "party_client.h"
#include <iostream>
#include <memory>
#include <csignal>
#include <atomic>
#include <thread>

std::atomic<bool> g_shutdown{false};

void signal_handler(int signal) {
    std::cout << "\n[Client] 收到退出信号..." << std::endl;
    g_shutdown = true;
}

int main(int argc, char** argv) {
    std::cout << "===== Party Client (门限公钥聚合模式) =====" << std::endl;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 默认配置
    int32_t party_id = 1;
    std::string party_name = "PartyA";
    std::string server_address = "localhost:60001";
    int32_t p2p_port = 0;
    std::string data_str = "10,20,30,40,50,60,70,80,90,100,110,120,130,140,150,160";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--id" && i + 1 < argc) party_id = std::atoi(argv[++i]);
        else if (arg == "--name" && i + 1 < argc) party_name = argv[++i];
        else if (arg == "--server" && i + 1 < argc) server_address = argv[++i];
        else if (arg == "--p2p-port" && i + 1 < argc) p2p_port = std::atoi(argv[++i]);
        else if (arg == "--data" && i + 1 < argc) data_str = argv[++i];
        else if (arg == "--help") {
            std::cout << "用法: party_client --id <N> --name <名称> --server <地址> "
                      << "--p2p-port <端口> --data <数据>" << std::endl;
            return 0;
        }
    }

    if (p2p_port == 0) p2p_port = 50050 + party_id;

    std::cout << "[Client] ID=" << party_id << ", Name=" << party_name
              << ", Server=" << server_address << ", P2P=" << p2p_port << std::endl;

    // 创建状态
    PartyClientState state;
    state.party_id = party_id;
    state.party_name = party_name;
    state.p2p_port = p2p_port;
    state.expected_parties = 3;

    // 解析数据
    std::stringstream ss(data_str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        state.local_data.push_back(std::stod(item));
    }
    std::cout << "[Client] 本地数据: " << state.local_data.size() << " 个元素" << std::endl;

    // ========== 第一阶段：初始化 CryptoContext ==========
    std::cout << "\n[Client] ===== 第一阶段: 初始化 CryptoContext =====" << std::endl;
    CCParams<CryptoContextCKKSRNS> params;
    params.SetMultiplicativeDepth(3);  // 3 足够做一次乘法 (除以 n)
    params.SetScalingModSize(50);
    params.SetBatchSize(16);

    state.cc = GenCryptoContext(params);
    state.cc->Enable(PKE);
    state.cc->Enable(KEYSWITCH);
    state.cc->Enable(LEVELEDSHE);
    state.cc->Enable(ADVANCEDSHE);
    state.cc->Enable(MULTIPARTY);

    std::cout << "[Client] CKKS 环维度: " << state.cc->GetRingDimension() << std::endl;

    // ========== 第二阶段：注册到服务器 ==========
    std::cout << "\n[Client] ===== 第二阶段: 注册到服务器 =====" << std::endl;
    state.server_channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    state.server_stub = distributed_mpc::ComputeServer::NewStub(state.server_channel);

    {
        distributed_mpc::PartyRegistration reg;
        reg.set_party_id(party_id);
        reg.set_party_name(party_name);
        reg.set_party_p2p_port(p2p_port);

        distributed_mpc::RegistrationAck ack;
        grpc::ClientContext ctx;
        auto status = state.server_stub->RegisterParty(&ctx, reg, &ack);

        if (!status.ok() || !ack.success()) {
            std::cerr << "[Client] 注册失败" << std::endl;
            return 1;
        }

        state.registered = true;
        state.session_id = ack.session_id();
        state.expected_parties = ack.expected_parties();

        for (const auto& other : ack.other_parties()) {
            std::string p2p_addr = "localhost:" + std::to_string(other.p2p_port());
            state.other_party_p2p_addresses[other.party_id()] = p2p_addr;
        }

        std::cout << "[Client] 注册成功, 期望 " << state.expected_parties << " 个参与方" << std::endl;
    }

    // ========== 第三阶段：链式密钥生成 ==========
    std::cout << "\n[Client] ===== 第三阶段: 链式密钥生成 =====" << std::endl;

    bool keygen_success = false;
    if (party_id == 1) {
        // Party 1: 初始 KeyGen
        keygen_success = KeyGenRound1(state);
    } else {
        // Party N > 1: MultipartyKeyGen(prev_pk)
        keygen_success = KeyGenRoundN(state);
    }

    if (!keygen_success) {
        std::cerr << "[Client] 密钥生成失败!" << std::endl;
        return 1;
    }

    // ========== 第四阶段：获取联合公钥 ==========
    std::cout << "\n[Client] ===== 第四阶段: 获取联合公钥 =====" << std::endl;

    if (!FetchJointPublicKey(state)) {
        std::cerr << "[Client] 获取联合公钥失败!" << std::endl;
        return 1;
    }

    // ========== 第五阶段：启动 P2P 服务器 ==========
    std::cout << "\n[Client] ===== 第五阶段: 启动 P2P 服务器 =====" << std::endl;
    std::atomic<bool> p2p_shutdown{false};
    std::thread p2p_thread(RunP2PServer, std::ref(state), std::ref(p2p_shutdown));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // ========== 第六阶段：使用联合公钥加密数据 ==========
    std::cout << "\n[Client] ===== 第六阶段: 加密数据 =====" << std::endl;
    Plaintext pt = state.cc->MakeCKKSPackedPlaintext(state.local_data);
    state.ciphertext = state.cc->Encrypt(state.joint_public_key, pt);
    std::cout << "[Client] 数据已使用联合公钥加密" << std::endl;

    // ========== 第七阶段：发送密文到服务器 ==========
    std::cout << "\n[Client] ===== 第七阶段: 发送密文 =====" << std::endl;
    {
        auto chunks = OpenFHEGrpcSerializer::SerializeCiphertext(state.ciphertext, party_id);

        grpc::ClientContext ctx;
        distributed_mpc::SubmissionAck ack;
        std::unique_ptr<grpc::ClientWriter<distributed_mpc::CiphertextChunk>> writer(
            state.server_stub->SubmitCiphertext(&ctx, &ack));

        for (const auto& chunk : chunks) {
            distributed_mpc::CiphertextChunk grpc_chunk;
            grpc_chunk.set_party_id(chunk.party_id);
            grpc_chunk.set_chunk_index(chunk.chunk_index);
            grpc_chunk.set_total_chunks(chunk.total_chunks);
            grpc_chunk.set_data(chunk.data.data(), chunk.data.size());
            grpc_chunk.set_is_last(chunk.is_last);
            grpc_chunk.set_checksum(chunk.checksum);
            writer->Write(grpc_chunk);
        }
        writer->WritesDone();
        auto st = writer->Finish();

        if (!st.ok() || !ack.success()) {
            std::cerr << "[Client] 密文发送失败" << std::endl;
            p2p_shutdown = true;
            p2p_thread.join();
            return 1;
        }
        std::cout << "[Client] 密文发送成功" << std::endl;
    }

    // ========== 第八阶段：等待计算完成 ==========
    std::cout << "\n[Client] ===== 第八阶段: 等待计算 =====" << std::endl;

    bool computation_done = false;
    for (int i = 0; i < 120 && !computation_done && !g_shutdown; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));

        if (i % 3 == 0 && party_id == 1) {
            distributed_mpc::Empty empty;
            distributed_mpc::ServerStatus status_resp;
            grpc::ClientContext status_ctx;

            auto s = state.server_stub->GetStatus(&status_ctx, empty, &status_resp);
            if (s.ok() && status_resp.joint_key_ready()
                && status_resp.ciphertexts_received() >= status_resp.total_parties()) {

                distributed_mpc::ComputationRequest comp_req;
                comp_req.set_computation_type("average");
                distributed_mpc::ComputationStatus comp_status;
                grpc::ClientContext comp_ctx;

                auto cr = state.server_stub->TriggerComputation(&comp_ctx, comp_req, &comp_status);
                if (cr.ok() && comp_status.completed()) {
                    state.result_id = comp_status.result_id();
                    computation_done = true;
                    std::cout << "[Client] 计算完成: " << state.result_id << std::endl;
                }
            }
        }
    }

    // ========== 第九阶段：P2P 协作解密 ==========
    if (computation_done && !state.result_id.empty()) {
        std::cout << "\n[Client] ===== 第九阶段: P2P 协作解密 =====" << std::endl;

        if (FetchResultFromServer(state) && PerformPartialDecrypt(state)) {

            // 发送部分解密给其他参与方
            for (const auto& [other_id, other_addr] : state.other_party_p2p_addresses) {
                SendPartialDecryptToParty(state, other_id);
            }

            // 等待接收其他参与方的部分解密
            std::cout << "[Client] 等待其他参与方的部分解密..." << std::endl;
            if (state.WaitForAllPartialDecrypts(60)) {
                std::cout << "[Client] 所有部分解密已收到" << std::endl;

                if (FusePartialDecrypts(state) && state.result_ready) {
                    std::cout << "\n[Client] =======================================" << std::endl;
                    std::cout << "[Client]   最终解密结果 (前16个):" << std::endl;
                    std::cout << "[Client]   ";
                    for (size_t i = 0; i < 16 && i < state.decrypted_result.size(); i++) {
                        std::cout << state.decrypted_result[i] << " ";
                    }
                    std::cout << "\n[Client] =======================================" << std::endl;

                    // 验证
                    double local_avg = 0;
                    for (double v : state.local_data) local_avg += v;
                    local_avg /= state.local_data.size();
                    std::cout << "[Client] 本地数据平均值: " << local_avg << std::endl;
                }
            } else {
                std::cerr << "[Client] 等待部分解密超时" << std::endl;
            }
        }
    }

    std::cout << "\n[Client] 按 Ctrl+C 退出" << std::endl;
    while (!g_shutdown) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    p2p_shutdown = true;
    p2p_thread.join();
    return 0;
}
