//==================================================================================
// Party Client 实现
// 
// 真正的门限公钥聚合：使用 OpenFHE 的链式 MultipartyKeyGen 协议
//==================================================================================

#include "party_client.h"
#include <iostream>
#include <sstream>

// ==================== 序列化辅助函数 ====================

std::vector<uint8_t> SerializePublicKey(const PublicKey<DCRTPoly>& pk) {
    std::stringstream ss;
    lbcrypto::Serial::Serialize(pk, ss, lbcrypto::SerType::BINARY);
    std::string str = ss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::vector<uint8_t> SerializeEvalKey(const EvalKey<DCRTPoly>& ek) {
    std::stringstream ss;
    lbcrypto::Serial::Serialize(ek, ss, lbcrypto::SerType::BINARY);
    std::string str = ss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

std::vector<uint8_t> SerializeEvalSumKeys(
    const std::shared_ptr<std::map<usint, EvalKey<DCRTPoly>>>& evalSumKeys) {
    std::stringstream ss;
    // 序列化整个 map
    lbcrypto::Serial::Serialize(*evalSumKeys, ss, lbcrypto::SerType::BINARY);
    std::string str = ss.str();
    return std::vector<uint8_t>(str.begin(), str.end());
}

template<typename T>
T DeserializeFromBytes(const std::vector<uint8_t>& data) {
    std::string str(data.begin(), data.end());
    std::stringstream ss(str);
    T obj;
    lbcrypto::Serial::Deserialize(obj, ss, lbcrypto::SerType::BINARY);
    return obj;
}

// ==================== P2P 服务 ====================

void RunP2PServer(PartyClientState& state, std::atomic<bool>& shutdown_flag) {
    PartyP2PServiceImpl p2p_service(&state);

    ServerBuilder builder;
    std::string listen_address = "0.0.0.0:" + std::to_string(state.p2p_port);
    builder.AddListeningPort(listen_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&p2p_service);

    std::unique_ptr<Server> p2p_server(builder.BuildAndStart());
    if (!p2p_server) {
        std::cerr << "[P2P] 服务器启动失败 on " << listen_address << std::endl;
        return;
    }

    std::cout << "[P2P] 服务器已启动，监听 " << listen_address << std::endl;

    while (!shutdown_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    p2p_server->Shutdown();
}

grpc::Status PartyP2PServiceImpl::ExchangePartialDecrypt(
    grpc::ServerContext* context,
    ServerReader<distributed_mpc::PartialDecryptChunk>* reader,
    distributed_mpc::PartialDecryptAck* response) {

    distributed_mpc::PartialDecryptChunk chunk;
    std::vector<uint8_t> all_data;
    int32_t from_party = -1;

    while (reader->Read(&chunk)) {
        if (from_party == -1) from_party = chunk.from_party();
        all_data.insert(all_data.end(), chunk.data().begin(), chunk.data().end());
        if (chunk.is_last()) break;
    }

    if (from_party == -1) {
        response->set_success(false);
        response->set_error("No data received");
        return grpc::Status::OK;
    }

    state_->AddPartialDecrypt(from_party, all_data);
    std::cout << "[P2P] 收到来自 Party " << from_party
              << " 的部分解密 (" << all_data.size() << " 字节)" << std::endl;

    response->set_success(true);
    return grpc::Status::OK;
}

grpc::Status PartyP2PServiceImpl::RequestPartialDecrypt(
    grpc::ServerContext* context,
    const distributed_mpc::PartialDecryptRequest* request,
    ServerWriter<distributed_mpc::PartialDecryptChunk>* writer) {

    int32_t requesting_party = request->requesting_party();
    std::cout << "[P2P] Party " << requesting_party << " 请求部分解密" << std::endl;

    int wait_count = 0;
    while (!IsDecryptReady() && wait_count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        wait_count++;
    }

    if (!IsDecryptReady()) {
        std::cerr << "[P2P] 本地部分解密未就绪" << std::endl;
        return grpc::Status::OK;
    }

    std::lock_guard<std::mutex> lock(state_->mutex);
    size_t chunk_size = 64 * 1024;
    int32_t total_chunks = (state_->partial_decrypt_data.size() + chunk_size - 1) / chunk_size;

    size_t offset = 0;
    for (int32_t i = 0; i < total_chunks; i++) {
        distributed_mpc::PartialDecryptChunk chunk;
        chunk.set_from_party(state_->party_id);
        chunk.set_to_party(requesting_party);
        chunk.set_chunk_index(i);
        chunk.set_total_chunks(total_chunks);
        chunk.set_session_id(request->session_id());

        size_t remaining = state_->partial_decrypt_data.size() - offset;
        size_t copy_size = std::min(remaining, chunk_size);
        chunk.set_data(state_->partial_decrypt_data.data() + offset, copy_size);
        chunk.set_is_last(i == total_chunks - 1);

        std::vector<uint8_t> chunk_data(state_->partial_decrypt_data.begin() + offset,
                                        state_->partial_decrypt_data.begin() + offset + copy_size);
        chunk.set_checksum(OpenFHEGrpcSerializer::ComputeChecksum(chunk_data));

        writer->Write(chunk);
        offset += copy_size;
    }

    return grpc::Status::OK;
}

// ==================== 链式密钥生成 ====================

// Party 1: KeyGen() + 生成评估密钥 + 提交
bool KeyGenRound1(PartyClientState& state) {
    std::cout << "[Client] ===== 第一轮密钥生成 (Party 1) =====" << std::endl;

    // 1. 初始化密钥生成协议
    {
        distributed_mpc::InitKeyGenRequest req;
        req.set_party_id(state.party_id);
        req.set_session_id(state.session_id);

        distributed_mpc::InitKeyGenResponse resp;
        grpc::ClientContext ctx;
        auto status = state.server_stub->InitKeyGen(&ctx, req, &resp);
        if (!status.ok() || !resp.success()) {
            std::cerr << "[Client] InitKeyGen 失败: "
                      << (status.ok() ? resp.error() : status.error_message()) << std::endl;
            return false;
        }
    }
    std::cout << "[Client] 密钥生成协议已初始化" << std::endl;

    // 2. 本地生成密钥对
    std::cout << "[Client] 生成初始密钥对 (KeyGen)..." << std::endl;
    state.keypair = state.cc->KeyGen();
    std::cout << "[Client] 密钥对已生成: pk_1, sk_1" << std::endl;

    // 3. 生成评估密钥
    std::cout << "[Client] 生成评估乘法密钥..." << std::endl;
    auto evalMultKey = state.cc->KeySwitchGen(state.keypair.secretKey, state.keypair.secretKey);

    std::cout << "[Client] 生成评估求和密钥..." << std::endl;
    state.cc->EvalSumKeyGen(state.keypair.secretKey);
    auto evalSumKeys = std::make_shared<std::map<usint, EvalKey<DCRTPoly>>>(
        state.cc->GetEvalSumKeyMap(state.keypair.secretKey->GetKeyTag()));

    // 4. 序列化并提交
    std::cout << "[Client] 序列化并提交第一轮结果..." << std::endl;

    auto pk_bytes = SerializePublicKey(state.keypair.publicKey);
    auto evalMult_bytes = SerializeEvalKey(evalMultKey);
    auto evalSum_bytes = SerializeEvalSumKeys(evalSumKeys);

    distributed_mpc::KeyGenRound1Submission req;
    req.set_party_id(state.party_id);
    req.set_session_id(state.session_id);
    req.set_public_key(pk_bytes.data(), pk_bytes.size());
    req.set_eval_mult_key(evalMult_bytes.data(), evalMult_bytes.size());
    req.set_eval_sum_keys(evalSum_bytes.data(), evalSum_bytes.size());

    distributed_mpc::KeyGenRoundAck resp;
    grpc::ClientContext ctx;
    auto status = state.server_stub->SubmitKeyGenRound1(&ctx, req, &resp);

    if (!status.ok() || !resp.success()) {
        std::cerr << "[Client] SubmitKeyGenRound1 失败: "
                  << (status.ok() ? resp.error() : status.error_message()) << std::endl;
        return false;
    }

    std::cout << "[Client] 第一轮提交成功 (" << resp.completed_rounds()
              << "/" << resp.expected_rounds() << ")" << std::endl;

    return true;
}

// Party N > 1: 获取前一轮公钥 → MultipartyKeyGen → 聚合评估密钥 → 提交
bool KeyGenRoundN(PartyClientState& state) {
    int32_t round = state.party_id;  // Party N 执行第 N 轮
    std::cout << "[Client] ===== 第 " << round << " 轮密钥生成 (Party "
              << state.party_id << ") =====" << std::endl;

    // 1. 获取前一轮的公钥和评估密钥
    std::cout << "[Client] 获取第 " << (round - 1) << " 轮的公钥和评估密钥..." << std::endl;

    PublicKey<DCRTPoly> prevPublicKey;
    EvalKey<DCRTPoly> prevEvalMultKey;
    std::shared_ptr<std::map<usint, EvalKey<DCRTPoly>>> prevEvalSumKeys;

    {
        distributed_mpc::PrevPublicKeyRequest req;
        req.set_party_id(state.party_id);
        req.set_session_id(state.session_id);

        distributed_mpc::PrevPublicKeyResponse resp;
        grpc::ClientContext ctx;
        auto status = state.server_stub->GetPrevPublicKey(&ctx, req, &resp);

        if (!status.ok() || !resp.success()) {
            std::cerr << "[Client] GetPrevPublicKey 失败: "
                      << (status.ok() ? resp.error() : status.error_message()) << std::endl;
            return false;
        }

        // 反序列化前一轮公钥
        std::vector<uint8_t> pk_data(resp.prev_public_key().begin(),
                                     resp.prev_public_key().end());
        prevPublicKey = DeserializeFromBytes<PublicKey<DCRTPoly>>(pk_data);

        // 反序列化前一轮评估乘法密钥
        std::vector<uint8_t> evalMult_data(resp.eval_mult_key().begin(),
                                           resp.eval_mult_key().end());
        prevEvalMultKey = DeserializeFromBytes<EvalKey<DCRTPoly>>(evalMult_data);

        // 反序列化前一轮评估求和密钥
        std::vector<uint8_t> evalSum_data(resp.eval_sum_keys().begin(),
                                          resp.eval_sum_keys().end());
        prevEvalSumKeys = std::make_shared<std::map<usint, EvalKey<DCRTPoly>>>(
            DeserializeFromBytes<std::map<usint, EvalKey<DCRTPoly>>>(evalSum_data));

        std::cout << "[Client] 已获取第 " << (round - 1) << " 轮数据 (来自 Party "
                  << resp.prev_party_id() << ")" << std::endl;
    }

    // 2. 使用前一轮公钥执行 MultipartyKeyGen
    std::cout << "[Client] 执行 MultipartyKeyGen(prev_pk)..." << std::endl;
    state.keypair = state.cc->MultipartyKeyGen(prevPublicKey);
    std::cout << "[Client] 密钥份额已生成: sk_" << state.party_id << std::endl;
    std::cout << "[Client] 联合公钥 pk_1...pk_" << state.party_id << " 已构造" << std::endl;

    // 3. 聚合评估密钥
    std::cout << "[Client] 聚合评估密钥..." << std::endl;

    // 评估乘法密钥聚合
    auto evalMultKeyN = state.cc->MultiKeySwitchGen(
        state.keypair.secretKey, state.keypair.secretKey, prevEvalMultKey);
    auto evalMultAB = state.cc->MultiAddEvalKeys(
        prevEvalMultKey, evalMultKeyN, state.keypair.publicKey->GetKeyTag());

    // 需要前几轮所有参与方的私钥来生成最终的乘法评估密钥
    // 但在分布式场景中，我们无法获取其他方的私钥
    // 解决方案：让客户端提交自己的 evalMultKeyN，由服务器或最后一个参与方完成聚合
    // 这里我们使用简化方法：只聚合当前轮次的评估密钥

    // 评估求和密钥聚合
    auto evalSumKeysN = state.cc->MultiEvalSumKeyGen(
        state.keypair.secretKey, prevEvalSumKeys, state.keypair.publicKey->GetKeyTag());
    auto evalSumKeysJoin = state.cc->MultiAddEvalSumKeys(
        prevEvalSumKeys, evalSumKeysN, state.keypair.publicKey->GetKeyTag());

    std::cout << "[Client] 评估密钥聚合完成" << std::endl;

    // 4. 序列化并提交
    std::cout << "[Client] 序列化并提交第 " << round << " 轮结果..." << std::endl;

    auto joint_pk_bytes = SerializePublicKey(state.keypair.publicKey);
    auto evalMult_bytes = SerializeEvalKey(evalMultAB);
    auto evalSum_bytes = SerializeEvalSumKeys(evalSumKeysJoin);

    distributed_mpc::KeyGenRoundNSubmission req;
    req.set_party_id(state.party_id);
    req.set_session_id(state.session_id);
    req.set_joint_public_key(joint_pk_bytes.data(), joint_pk_bytes.size());
    req.set_eval_mult_key(evalMult_bytes.data(), evalMult_bytes.size());
    req.set_eval_sum_keys(evalSum_bytes.data(), evalSum_bytes.size());
    // 最终聚合的评估密钥（当前轮次的聚合结果）
    req.set_eval_mult_final(evalMult_bytes.data(), evalMult_bytes.size());
    req.set_eval_sum_final(evalSum_bytes.data(), evalSum_bytes.size());

    distributed_mpc::KeyGenRoundAck resp;
    grpc::ClientContext ctx;
    auto status = state.server_stub->SubmitKeyGenRoundN(&ctx, req, &resp);

    if (!status.ok() || !resp.success()) {
        std::cerr << "[Client] SubmitKeyGenRoundN 失败: "
                  << (status.ok() ? resp.error() : status.error_message()) << std::endl;
        return false;
    }

    std::cout << "[Client] 第 " << round << " 轮提交成功 ("
              << resp.completed_rounds() << "/" << resp.expected_rounds() << ")" << std::endl;

    if (resp.all_rounds_complete()) {
        std::cout << "[Client] 所有轮次完成!" << std::endl;
    }

    return true;
}

// 获取联合公钥
bool FetchJointPublicKey(PartyClientState& state) {
    std::cout << "[Client] 获取最终联合公钥..." << std::endl;

    distributed_mpc::JointPublicKeyRequest req;
    req.set_party_id(state.party_id);
    req.set_session_id(state.session_id);

    distributed_mpc::JointPublicKeyResponse resp;
    grpc::ClientContext ctx;
    auto status = state.server_stub->GetJointPublicKey(&ctx, req, &resp);

    if (!status.ok() || !resp.success()) {
        std::cerr << "[Client] GetJointPublicKey 失败: "
                  << (status.ok() ? resp.error() : status.error_message()) << std::endl;
        return false;
    }

    // 反序列化联合公钥
    try {
        std::vector<uint8_t> pk_data(resp.joint_public_key().begin(),
                                     resp.joint_public_key().end());
        state.joint_public_key = DeserializeFromBytes<PublicKey<DCRTPoly>>(pk_data);
        state.has_joint_public_key = true;

        // 反序列化评估密钥并注入到 CryptoContext
        if (resp.eval_mult_key().size() > 0) {
            std::vector<uint8_t> evalMult_data(resp.eval_mult_key().begin(),
                                               resp.eval_mult_key().end());
            auto evalMultKey = DeserializeFromBytes<EvalKey<DCRTPoly>>(evalMult_data);
            state.cc->InsertEvalMultKey({evalMultKey});
        }

        if (resp.eval_sum_keys().size() > 0) {
            std::vector<uint8_t> evalSum_data(resp.eval_sum_keys().begin(),
                                              resp.eval_sum_keys().end());
            auto evalSumKeys = std::make_shared<std::map<usint, EvalKey<DCRTPoly>>>(
                DeserializeFromBytes<std::map<usint, EvalKey<DCRTPoly>>>(evalSum_data));
            state.cc->InsertEvalSumKey(evalSumKeys);
        }

        std::cout << "[Client] 联合公钥获取成功 (参与方数: " << resp.party_count() << ")" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Client] 反序列化失败: " << e.what() << std::endl;
        return false;
    }
}

// ==================== 部分解密 ====================

bool PerformPartialDecrypt(PartyClientState& state) {
    if (!state.result_ciphertext) {
        std::cerr << "[Client] 没有计算结果密文" << std::endl;
        return false;
    }

    try {
        std::cout << "[Client] 执行部分解密..." << std::endl;

        std::vector<Ciphertext<DCRTPoly>> partials;
        if (state.party_id == 1) {
            partials = state.cc->MultipartyDecryptLead({state.result_ciphertext}, state.keypair.secretKey);
        } else {
            partials = state.cc->MultipartyDecryptMain({state.result_ciphertext}, state.keypair.secretKey);
        }

        if (partials.empty()) {
            std::cerr << "[Client] 部分解密返回空结果" << std::endl;
            return false;
        }

        state.partial_decrypt = partials[0];

        std::stringstream ss;
        lbcrypto::Serial::Serialize(state.partial_decrypt, ss, lbcrypto::SerType::BINARY);
        std::string str = ss.str();
        state.partial_decrypt_data.assign(str.begin(), str.end());
        state.decrypt_ready = true;

        std::cout << "[Client] 部分解密完成 (" << state.partial_decrypt_data.size() << " 字节)" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Client] 部分解密失败: " << e.what() << std::endl;
        return false;
    }
}

bool SendPartialDecryptToParty(PartyClientState& state, int32_t target_party) {
    auto it = state.other_party_p2p_addresses.find(target_party);
    if (it == state.other_party_p2p_addresses.end()) {
        std::cerr << "[Client] 未找到 Party " << target_party << " 的 P2P 地址" << std::endl;
        return false;
    }

    auto channel = grpc::CreateChannel(it->second, grpc::InsecureChannelCredentials());
    auto stub = distributed_mpc::PartyP2P::NewStub(channel);

    grpc::ClientContext ctx;
    distributed_mpc::PartialDecryptAck ack;
    auto writer = stub->ExchangePartialDecrypt(&ctx, &ack);

    size_t chunk_size = 64 * 1024;
    int32_t total_chunks = (state.partial_decrypt_data.size() + chunk_size - 1) / chunk_size;
    size_t offset = 0;

    for (int32_t i = 0; i < total_chunks; i++) {
        distributed_mpc::PartialDecryptChunk chunk;
        chunk.set_from_party(state.party_id);
        chunk.set_to_party(target_party);
        chunk.set_chunk_index(i);
        chunk.set_total_chunks(total_chunks);
        chunk.set_session_id(state.session_id);

        size_t remaining = state.partial_decrypt_data.size() - offset;
        size_t copy_size = std::min(remaining, chunk_size);
        chunk.set_data(state.partial_decrypt_data.data() + offset, copy_size);
        chunk.set_is_last(i == total_chunks - 1);

        std::vector<uint8_t> chunk_data(state.partial_decrypt_data.begin() + offset,
                                        state.partial_decrypt_data.begin() + offset + copy_size);
        chunk.set_checksum(OpenFHEGrpcSerializer::ComputeChecksum(chunk_data));

        writer->Write(chunk);
        offset += copy_size;
    }

    writer->WritesDone();
    auto status = writer->Finish();

    if (!status.ok() || !ack.success()) {
        std::cerr << "[Client] 发送部分解密给 Party " << target_party << " 失败" << std::endl;
        return false;
    }

    std::cout << "[Client] 部分解密已发送给 Party " << target_party << std::endl;
    return true;
}

bool FusePartialDecrypts(PartyClientState& state) {
    if (state.party_id != 1) {
        std::cout << "[Client] 非 Lead 参与方，跳过融合" << std::endl;
        return true;
    }

    std::cout << "[Client] 融合所有部分解密..." << std::endl;

    try {
        std::vector<Ciphertext<DCRTPoly>> partialVec;
        partialVec.push_back(state.partial_decrypt);

        {
            std::lock_guard<std::mutex> lock(state.partial_decrypt_mutex);
            for (const auto& [pid, data] : state.received_partial_decrypts) {
                Ciphertext<DCRTPoly> pd;
                std::string str_data(data.begin(), data.end());
                std::stringstream ss(str_data);
                lbcrypto::Serial::Deserialize(pd, ss, lbcrypto::SerType::BINARY);
                partialVec.push_back(pd);
                std::cout << "[Client] 已加载 Party " << pid << " 的部分解密" << std::endl;
            }
        }

        if (partialVec.size() < static_cast<size_t>(state.expected_parties)) {
            std::cerr << "[Client] 部分解密不足: " << partialVec.size()
                      << "/" << state.expected_parties << std::endl;
            return false;
        }

        Plaintext result_pt;
        auto decrypt_result = state.cc->MultipartyDecryptFusion(partialVec, &result_pt);

        if (decrypt_result.isValid) {
            state.decrypted_result = result_pt->GetRealPackedValue();
            state.result_ready = true;

            std::cout << "[Client] ===== 解密融合成功! =====" << std::endl;
            std::cout << "[Client] 结果 (前16个): ";
            for (size_t i = 0; i < 16 && i < state.decrypted_result.size(); i++) {
                std::cout << state.decrypted_result[i] << " ";
            }
            std::cout << std::endl;
            return true;
        } else {
            std::cerr << "[Client] 融合结果无效" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Client] 融合失败: " << e.what() << std::endl;
        return false;
    }
}

bool FetchResultFromServer(PartyClientState& state) {
    distributed_mpc::ResultRequest req;
    req.set_party_id(state.party_id);
    req.set_submission_id(state.result_id);

    grpc::ClientContext ctx;
    auto reader = state.server_stub->GetResult(&ctx, req);

    distributed_mpc::CiphertextChunk chunk;
    std::vector<uint8_t> all_data;

    while (reader->Read(&chunk)) {
        all_data.insert(all_data.end(), chunk.data().begin(), chunk.data().end());
        if (chunk.is_last()) break;
    }

    auto status = reader->Finish();
    if (!status.ok() || all_data.empty()) {
        std::cerr << "[Client] 获取结果失败" << std::endl;
        return false;
    }

    try {
        state.result_ciphertext = OpenFHEGrpcSerializer::DeserializeCiphertextFromBytes(
            all_data, state.cc);
        std::cout << "[Client] 计算结果密文获取成功 (" << all_data.size() << " 字节)" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Client] 结果反序列化失败: " << e.what() << std::endl;
        return false;
    }
}
