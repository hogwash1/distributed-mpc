//==================================================================================
// Party Client - 参与方客户端
// 
// 真正的门限公钥聚合：使用 OpenFHE 的链式 MultipartyKeyGen 协议
// Party 1: KeyGen() → 提交公钥+评估密钥
// Party N: MultipartyKeyGen(prev_pk) → 聚合评估密钥 → 提交联合公钥+评估密钥
//==================================================================================

#ifndef PARTY_CLIENT_H
#define PARTY_CLIENT_H

#include <openfhe.h>
#include <grpcpp/grpcpp.h>
#include <map>
#include <mutex>
#include <memory>
#include <vector>
#include <string>
#include <thread>
#include <condition_variable>
#include "distributed_mpc.grpc.pb.h"
#include "grpc_serializer.h"

using namespace lbcrypto;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;

// 参与方客户端状态
class PartyClientState {
public:
    int32_t party_id;
    std::string party_name;

    // gRPC
    std::shared_ptr<grpc::Channel> server_channel;
    std::unique_ptr<distributed_mpc::ComputeServer::Stub> server_stub;
    int32_t p2p_port;

    // 加密上下文和密钥
    CryptoContext<DCRTPoly> cc;
    KeyPair<DCRTPoly> keypair;              // 本参与方的密钥对
    PublicKey<DCRTPoly> joint_public_key;   // 最终联合公钥
    bool has_joint_public_key = false;

    // 本地数据
    std::vector<double> local_data;
    Ciphertext<DCRTPoly> ciphertext;

    // 部分解密
    Ciphertext<DCRTPoly> partial_decrypt;
    std::vector<uint8_t> partial_decrypt_data;
    std::map<int32_t, std::vector<uint8_t>> received_partial_decrypts;
    std::mutex partial_decrypt_mutex;
    std::condition_variable partial_decrypt_cv;
    int expected_parties = 3;

    // P2P
    std::map<int32_t, std::string> other_party_p2p_addresses;

    // 计算结果
    Ciphertext<DCRTPoly> result_ciphertext;
    std::vector<double> decrypted_result;
    bool result_ready = false;

    // 同步
    std::mutex mutex;
    bool registered = false;
    bool ciphertext_sent = false;
    bool decrypt_ready = false;
    std::string session_id;
    std::string result_id;

    void AddPartialDecrypt(int32_t from_party, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(partial_decrypt_mutex);
        received_partial_decrypts[from_party] = data;
        partial_decrypt_cv.notify_all();
    }

    bool WaitForAllPartialDecrypts(int timeout_seconds = 60) {
        std::unique_lock<std::mutex> lock(partial_decrypt_mutex);
        auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
        return partial_decrypt_cv.wait_until(lock, timeout, [this]() {
            return received_partial_decrypts.size() >= static_cast<size_t>(expected_parties - 1);
        });
    }
};

// P2P 服务实现
class PartyP2PServiceImpl final : public distributed_mpc::PartyP2P::Service {
public:
    explicit PartyP2PServiceImpl(PartyClientState* state) : state_(state) {}

    grpc::Status ExchangePartialDecrypt(
        grpc::ServerContext* context,
        ServerReader<distributed_mpc::PartialDecryptChunk>* reader,
        distributed_mpc::PartialDecryptAck* response) override;

    grpc::Status RequestPartialDecrypt(
        grpc::ServerContext* context,
        const distributed_mpc::PartialDecryptRequest* request,
        ServerWriter<distributed_mpc::PartialDecryptChunk>* writer) override;

    void SetPartialDecrypt(const Ciphertext<DCRTPoly>& pd) {
        std::lock_guard<std::mutex> lock(state_->mutex);
        state_->partial_decrypt = pd;
        std::stringstream ss;
        lbcrypto::Serial::Serialize(pd, ss, lbcrypto::SerType::BINARY);
        std::string str = ss.str();
        state_->partial_decrypt_data.assign(str.begin(), str.end());
        state_->decrypt_ready = true;
    }

    bool IsDecryptReady() {
        std::lock_guard<std::mutex> lock(state_->mutex);
        return state_->decrypt_ready;
    }

private:
    PartyClientState* state_;
};

// ==================== 函数声明 ====================

// P2P 服务器线程
void RunP2PServer(PartyClientState& state, std::atomic<bool>& shutdown_flag);

// 链式密钥生成：Party 1 执行第一轮
bool KeyGenRound1(PartyClientState& state);

// 链式密钥生成：Party N > 1 执行第 N 轮
bool KeyGenRoundN(PartyClientState& state);

// 获取联合公钥
bool FetchJointPublicKey(PartyClientState& state);

// 执行部分解密
bool PerformPartialDecrypt(PartyClientState& state);

// 发送部分解密给其他参与方
bool SendPartialDecryptToParty(PartyClientState& state, int32_t target_party);

// 融合所有部分解密
bool FusePartialDecrypts(PartyClientState& state);

// 从服务器获取计算结果
bool FetchResultFromServer(PartyClientState& state);

// 序列化辅助函数
std::vector<uint8_t> SerializePublicKey(const PublicKey<DCRTPoly>& pk);
std::vector<uint8_t> SerializeEvalKey(const EvalKey<DCRTPoly>& ek);
std::vector<uint8_t> SerializeEvalSumKeys(
    const std::shared_ptr<std::map<usint, EvalKey<DCRTPoly>>>& evalSumKeys);

#endif // PARTY_CLIENT_H
