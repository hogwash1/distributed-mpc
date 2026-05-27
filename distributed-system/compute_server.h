//==================================================================================
// Compute Server - 计算服务器
// 
// 真正的门限公钥聚合：使用 OpenFHE 的链式 MultipartyKeyGen 协议
// pk_joint 通过 KeyGen → MultipartyKeyGen → MultipartyKeyGen 链式生成
// 服务器作为协调者，按顺序让参与方依次执行密钥生成
//==================================================================================

#ifndef COMPUTE_SERVER_H
#define COMPUTE_SERVER_H

#include <openfhe.h>
#include <grpcpp/grpcpp.h>
#include <map>
#include <mutex>
#include <memory>
#include <vector>
#include <string>
#include <condition_variable>
#include "distributed_mpc.grpc.pb.h"
#include "grpc_serializer.h"

using namespace lbcrypto;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::Status;

// 参与方信息
struct PartyInfo {
    int32_t party_id;
    std::string party_name;
    std::string p2p_address;
    int32_t p2p_port;
    bool registered = false;
    bool ciphertext_received = false;
    std::vector<uint8_t> ciphertext_data;
};

// 密钥生成轮次数据（服务器存储的中间结果）
struct KeyGenRound {
    int32_t party_id;
    std::vector<uint8_t> public_key;       // 本轮公钥（Party 1 是初始公钥，后续是联合公钥）
    std::vector<uint8_t> eval_mult_key;    // 聚合后的评估乘法密钥
    std::vector<uint8_t> eval_sum_keys;    // 聚合后的评估求和密钥
    bool completed = false;
};

// 计算结果
struct ComputationResult {
    std::string result_id;
    Ciphertext<DCRTPoly> ciphertext;
    bool completed = false;
    std::vector<uint8_t> ciphertext_data;
};

// 全局状态
class ServerState {
public:
    std::map<int32_t, PartyInfo> parties;
    std::map<std::string, ComputationResult> results;
    std::mutex mutex;
    std::string session_id;
    int32_t expected_parties = 3;

    CryptoContext<DCRTPoly> cc;

    // 链式密钥生成状态
    int32_t current_keygen_round = 0;     // 当前完成的轮次 (0=未开始)
    bool keygen_started = false;          // 密钥生成是否已启动
    bool keygen_complete = false;         // 所有轮次是否完成
    std::vector<KeyGenRound> keygen_rounds;  // 每轮的数据

    // 最终联合公钥和评估密钥
    std::vector<uint8_t> final_joint_pk;
    std::vector<uint8_t> final_eval_mult_key;
    std::vector<uint8_t> final_eval_sum_keys;

    // 条件变量：等待下一轮密钥生成
    std::condition_variable keygen_cv;

    // 密文计数
    int32_t ciphertexts_received = 0;
};

// gRPC 服务实现
class ComputeServerGrpcService final : public distributed_mpc::ComputeServer::Service {
public:
    explicit ComputeServerGrpcService(ServerState* state) : state_(state) {}

    Status RegisterParty(ServerContext* context,
                        const distributed_mpc::PartyRegistration* request,
                        distributed_mpc::RegistrationAck* response) override;

    // 链式密钥生成 RPC
    Status InitKeyGen(ServerContext* context,
                     const distributed_mpc::InitKeyGenRequest* request,
                     distributed_mpc::InitKeyGenResponse* response) override;

    Status SubmitKeyGenRound1(ServerContext* context,
                             const distributed_mpc::KeyGenRound1Submission* request,
                             distributed_mpc::KeyGenRoundAck* response) override;

    Status GetPrevPublicKey(ServerContext* context,
                           const distributed_mpc::PrevPublicKeyRequest* request,
                           distributed_mpc::PrevPublicKeyResponse* response) override;

    Status SubmitKeyGenRoundN(ServerContext* context,
                             const distributed_mpc::KeyGenRoundNSubmission* request,
                             distributed_mpc::KeyGenRoundAck* response) override;

    Status GetJointPublicKey(ServerContext* context,
                            const distributed_mpc::JointPublicKeyRequest* request,
                            distributed_mpc::JointPublicKeyResponse* response) override;

    // 密文和计算 RPC
    Status SubmitCiphertext(ServerContext* context,
                           ServerReader<distributed_mpc::CiphertextChunk>* reader,
                           distributed_mpc::SubmissionAck* response) override;

    Status TriggerComputation(ServerContext* context,
                             const distributed_mpc::ComputationRequest* request,
                             distributed_mpc::ComputationStatus* response) override;

    Status GetStatus(ServerContext* context,
                     const distributed_mpc::Empty* request,
                     distributed_mpc::ServerStatus* response) override;

    Status GetResult(ServerContext* context,
                     const distributed_mpc::ResultRequest* request,
                     ServerWriter<distributed_mpc::CiphertextChunk>* writer) override;

private:
    ServerState* state_;
};

#endif // COMPUTE_SERVER_H
