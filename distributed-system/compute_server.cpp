//==================================================================================
// Compute Server 实现
// 
// 真正的门限公钥聚合：链式 MultipartyKeyGen 协议
// 服务器作为协调者，不持有任何私钥，只存储和转发序列化的密钥材料
//==================================================================================

#include "compute_server.h"
#include "he_evaluator.h"
#include "ast_serializer.h"
#include <iostream>
#include <sstream>

// ==================== 注册 ====================

Status ComputeServerGrpcService::RegisterParty(
    ServerContext* context,
    const distributed_mpc::PartyRegistration* request,
    distributed_mpc::RegistrationAck* response) {

    std::lock_guard<std::mutex> lock(state_->mutex);

    int32_t party_id = request->party_id();
    std::cout << "[Server] Party " << party_id
              << " (" << request->party_name() << ") 注册" << std::endl;

    PartyInfo info;
    info.party_id = party_id;
    info.party_name = request->party_name();
    info.p2p_address = context->peer();
    info.p2p_port = request->party_p2p_port();
    info.registered = true;
    state_->parties[party_id] = info;

    if (state_->session_id.empty()) {
        std::stringstream ss;
        ss << "session_" << time(nullptr);
        state_->session_id = ss.str();
    }

    response->set_success(true);
    response->set_session_id(state_->session_id);
    response->set_expected_parties(state_->expected_parties);

    for (const auto& [id, party] : state_->parties) {
        if (id != party_id && party.registered) {
            auto* other = response->add_other_parties();
            other->set_party_id(id);
            other->set_party_name(party.party_name);
            other->set_p2p_address(party.p2p_address);
            other->set_p2p_port(party.p2p_port);
        }
    }

    std::cout << "[Server] 已注册: " << state_->parties.size()
              << "/" << state_->expected_parties << std::endl;
    return Status::OK;
}

// ==================== 链式密钥生成协议 ====================

Status ComputeServerGrpcService::InitKeyGen(
    ServerContext* context,
    const distributed_mpc::InitKeyGenRequest* request,
    distributed_mpc::InitKeyGenResponse* response) {

    std::lock_guard<std::mutex> lock(state_->mutex);

    if (state_->keygen_started) {
        response->set_success(false);
        response->set_error("Key generation already started");
        return Status::OK;
    }

    state_->keygen_started = true;
    state_->current_keygen_round = 0;
    state_->keygen_rounds.clear();
    state_->keygen_complete = false;

    std::cout << "[Server] 密钥生成协议已初始化，等待 Party 1 提交第一轮结果" << std::endl;

    response->set_success(true);
    return Status::OK;
}

Status ComputeServerGrpcService::SubmitKeyGenRound1(
    ServerContext* context,
    const distributed_mpc::KeyGenRound1Submission* request,
    distributed_mpc::KeyGenRoundAck* response) {

    std::lock_guard<std::mutex> lock(state_->mutex);

    int32_t party_id = request->party_id();

    std::cout << "[Server] 收到 Party " << party_id << " 的第一轮密钥生成结果" << std::endl;

    if (state_->current_keygen_round != 0) {
        response->set_success(false);
        response->set_error("Round 1 already completed");
        return Status::OK;
    }

    // 存储第一轮数据
    KeyGenRound round;
    round.party_id = party_id;
    round.public_key.assign(request->public_key().begin(), request->public_key().end());
    round.eval_mult_key.assign(request->eval_mult_key().begin(), request->eval_mult_key().end());
    round.eval_sum_keys.assign(request->eval_sum_keys().begin(), request->eval_sum_keys().end());
    round.completed = true;

    state_->keygen_rounds.push_back(round);
    state_->current_keygen_round = 1;

    std::cout << "[Server] 第 1 轮密钥生成完成 (Party " << party_id << ")" << std::endl;

    // 通知等待的线程
    state_->keygen_cv.notify_all();

    response->set_success(true);
    response->set_completed_rounds(1);
    response->set_expected_rounds(state_->expected_parties);
    response->set_all_rounds_complete(1 >= state_->expected_parties);

    if (1 >= state_->expected_parties) {
        // 只有一个参与方的情况
        state_->keygen_complete = true;
        state_->final_joint_pk = round.public_key;
        state_->final_eval_mult_key = round.eval_mult_key;
        state_->final_eval_sum_keys = round.eval_sum_keys;
        state_->keygen_cv.notify_all();
    }

    return Status::OK;
}

Status ComputeServerGrpcService::GetPrevPublicKey(
    ServerContext* context,
    const distributed_mpc::PrevPublicKeyRequest* request,
    distributed_mpc::PrevPublicKeyResponse* response) {

    int32_t party_id = request->party_id();
    int32_t expected_round = party_id - 1;  // Party N 需要第 N-1 轮的结果

    std::cout << "[Server] Party " << party_id
              << " 请求第 " << expected_round << " 轮的公钥" << std::endl;

    // 等待前一轮完成
    {
        std::unique_lock<std::mutex> lock(state_->mutex);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
        bool ready = state_->keygen_cv.wait_until(lock, deadline, [this, expected_round]() {
            return static_cast<int32_t>(state_->keygen_rounds.size()) >= expected_round;
        });

        if (!ready) {
            response->set_success(false);
            response->set_error("Timeout waiting for round " + std::to_string(expected_round));
            return Status::OK;
        }
    }

    std::lock_guard<std::mutex> lock(state_->mutex);

    // 返回前一轮的数据
    const auto& prev_round = state_->keygen_rounds[expected_round - 1];

    response->set_success(true);
    response->set_prev_public_key(prev_round.public_key.data(), prev_round.public_key.size());
    response->set_prev_party_id(prev_round.party_id);
    response->set_eval_mult_key(prev_round.eval_mult_key.data(), prev_round.eval_mult_key.size());
    response->set_eval_sum_keys(prev_round.eval_sum_keys.data(), prev_round.eval_sum_keys.size());

    std::cout << "[Server] 已向 Party " << party_id
              << " 提供第 " << expected_round << " 轮数据 (来自 Party "
              << prev_round.party_id << ")" << std::endl;

    return Status::OK;
}

Status ComputeServerGrpcService::SubmitKeyGenRoundN(
    ServerContext* context,
    const distributed_mpc::KeyGenRoundNSubmission* request,
    distributed_mpc::KeyGenRoundAck* response) {

    std::lock_guard<std::mutex> lock(state_->mutex);

    int32_t party_id = request->party_id();
    int32_t expected_round = party_id;  // Party N 提交第 N 轮

    std::cout << "[Server] 收到 Party " << party_id
              << " 的第 " << expected_round << " 轮密钥生成结果" << std::endl;

    if (static_cast<int32_t>(state_->keygen_rounds.size()) != expected_round - 1) {
        response->set_success(false);
        response->set_error("Expected round " + std::to_string(expected_round - 1)
                            + " to be completed first, but current is "
                            + std::to_string(state_->keygen_rounds.size()));
        return Status::OK;
    }

    // 存储本轮数据
    KeyGenRound round;
    round.party_id = party_id;
    round.public_key.assign(request->joint_public_key().begin(), request->joint_public_key().end());
    round.eval_mult_key.assign(request->eval_mult_final().begin(), request->eval_mult_final().end());
    round.eval_sum_keys.assign(request->eval_sum_final().begin(), request->eval_sum_final().end());
    round.completed = true;

    state_->keygen_rounds.push_back(round);
    state_->current_keygen_round = expected_round;

    std::cout << "[Server] 第 " << expected_round << " 轮密钥生成完成 (Party "
              << party_id << ")" << std::endl;

    // 检查是否所有轮次都完成
    if (expected_round >= state_->expected_parties) {
        state_->keygen_complete = true;
        state_->final_joint_pk = round.public_key;
        state_->final_eval_mult_key = round.eval_mult_key;
        state_->final_eval_sum_keys = round.eval_sum_keys;
        std::cout << "[Server] 所有轮次完成! 联合公钥已就绪" << std::endl;
    }

    state_->keygen_cv.notify_all();

    response->set_success(true);
    response->set_completed_rounds(expected_round);
    response->set_expected_rounds(state_->expected_parties);
    response->set_all_rounds_complete(state_->keygen_complete);

    return Status::OK;
}

Status ComputeServerGrpcService::GetJointPublicKey(
    ServerContext* context,
    const distributed_mpc::JointPublicKeyRequest* request,
    distributed_mpc::JointPublicKeyResponse* response) {

    int32_t party_id = request->party_id();
    std::cout << "[Server] Party " << party_id << " 请求联合公钥" << std::endl;

    // 等待所有轮次完成
    {
        std::unique_lock<std::mutex> lock(state_->mutex);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
        bool ready = state_->keygen_cv.wait_until(lock, deadline, [this]() {
            return state_->keygen_complete;
        });

        if (!ready) {
            response->set_success(false);
            response->set_error("Timeout waiting for key generation to complete");
            return Status::OK;
        }
    }

    std::lock_guard<std::mutex> lock(state_->mutex);

    response->set_success(true);
    response->set_joint_public_key(state_->final_joint_pk.data(), state_->final_joint_pk.size());
    response->set_eval_mult_key(state_->final_eval_mult_key.data(), state_->final_eval_mult_key.size());
    response->set_eval_sum_keys(state_->final_eval_sum_keys.data(), state_->final_eval_sum_keys.size());
    response->set_party_count(state_->keygen_rounds.size());

    std::cout << "[Server] 已向 Party " << party_id << " 提供联合公钥" << std::endl;

    return Status::OK;
}

// ==================== 密文和计算 ====================

Status ComputeServerGrpcService::SubmitCiphertext(
    ServerContext* context,
    ServerReader<distributed_mpc::CiphertextChunk>* reader,
    distributed_mpc::SubmissionAck* response) {

    distributed_mpc::CiphertextChunk chunk;
    std::vector<uint8_t> all_data;
    int32_t party_id = -1;

    std::cout << "[Server] 开始接收密文..." << std::endl;

    while (reader->Read(&chunk)) {
        if (party_id == -1) party_id = chunk.party_id();
        all_data.insert(all_data.end(), chunk.data().begin(), chunk.data().end());
        if (chunk.is_last()) break;
    }

    if (party_id == -1) {
        response->set_success(false);
        response->set_error("No data received");
        return Status::OK;
    }

    std::lock_guard<std::mutex> lock(state_->mutex);

    state_->parties[party_id].ciphertext_data = all_data;
    state_->parties[party_id].ciphertext_received = true;
    state_->ciphertexts_received++;

    std::cout << "[Server] Party " << party_id << " 密文已接收 ("
              << all_data.size() << " 字节), 总计: "
              << state_->ciphertexts_received << "/" << state_->expected_parties << std::endl;

    response->set_success(true);
    response->set_submission_id("sub_" + std::to_string(party_id));
    return Status::OK;
}

Status ComputeServerGrpcService::TriggerComputation(
    ServerContext* context,
    const distributed_mpc::ComputationRequest* request,
    distributed_mpc::ComputationStatus* response) {

    std::lock_guard<std::mutex> lock(state_->mutex);

    std::string comp_type = request->computation_type();
    std::cout << "[Server] 收到计算请求: " << comp_type << std::endl;

    // 检查所有密文是否已收到
    if (state_->ciphertexts_received < state_->expected_parties) {
        response->set_completed(false);
        response->set_has_error(true);
        response->set_error("Waiting for ciphertexts: "
                            + std::to_string(state_->ciphertexts_received) + "/"
                            + std::to_string(state_->expected_parties));
        return Status::OK;
    }

    try {
        Ciphertext<DCRTPoly> result;

        // ============================================================
        // NEW: AST expression-based computation (Task 04 integration)
        // ============================================================
        if (request->has_expression()) {
            std::cout << "[Server] 使用 AST 表达式求值..." << std::endl;

            // 1. Deserialize AST from proto
            auto ast = mpc::AstSerializer::deserialize(request->expression());
            std::cout << "[Server] AST:\n" << ast->to_string() << std::endl;

            // 2. Build var_map: party_id → Ciphertext
            std::map<int32_t, Ciphertext<DCRTPoly>> var_map;
            const auto& var_ids = request->ciphertext_vars();

            // Collect ciphertexts keyed by party_id
            for (size_t i = 0; i < var_ids.size(); i++) {
                int32_t vid = var_ids.Get(i);
                auto it = state_->parties.find(vid);
                if (it == state_->parties.end()) {
                    response->set_completed(false);
                    response->set_has_error(true);
                    response->set_error("Unknown party_id in ciphertext_vars: " + std::to_string(vid));
                    return Status::OK;
                }
                auto ct = OpenFHEGrpcSerializer::DeserializeCiphertextFromBytes(
                    it->second.ciphertext_data, state_->cc);
                var_map[vid] = ct;
                std::cout << "[Server] var_map[" << vid << "] = Party " << vid << " ciphertext" << std::endl;
            }

            // 3. Validate AST against available variables
            auto missing = mpc::ExprParser::validate(ast, var_map.size());
            // Also check that all vars in AST have corresponding ciphertexts
            auto used_vars = mpc::ExprParser::collect_vars(ast);
            for (auto v : used_vars) {
                if (var_map.find(v) == var_map.end()) {
                    response->set_completed(false);
                    response->set_has_error(true);
                    response->set_error("Missing ciphertext for variable party_id=" + std::to_string(v));
                    return Status::OK;
                }
            }

            // 4. Check depth against configured multiplicative depth
            int ast_depth = mpc::ExprParser::compute_depth(ast);
            uint32_t max_depth = state_->cc->GetEncodingParams()->GetMultiplicativeDepth();
            if (ast_depth > static_cast<int>(max_depth)) {
                std::ostringstream oss;
                oss << "AST depth " << ast_depth
                    << " exceeds configured multiplicative depth " << max_depth;
                response->set_completed(false);
                response->set_has_error(true);
                response->set_error(oss.str());
                return Status::OK;
            }
            std::cout << "[Server] AST depth=" << ast_depth
                      << " (max=" << max_depth << ") OK" << std::endl;

            // 5. Evaluate with HeEvaluator
            mpc::HeEvaluator evaluator(state_->cc, state_->joint_public_key, var_map);
            result = evaluator.evaluate(ast);
            result = state_->cc->ModReduce(result);

            std::cout << "[Server] HeEvaluator: ops=" << evaluator.get_op_count()
                      << " depth_consumed=" << evaluator.get_depth_consumed() << std::endl;

        } else {
            // ============================================================
            // OLD: backward-compatible average/sum logic
            // ============================================================
            std::string comp_type = request->computation_type();
            std::cout << "[Server] 使用传统 computation_type: " << comp_type << std::endl;

            // 反序列化所有密文
            std::vector<Ciphertext<DCRTPoly>> ciphertexts;
            for (auto& [id, party] : state_->parties) {
                auto ct = OpenFHEGrpcSerializer::DeserializeCiphertextFromBytes(
                    party.ciphertext_data, state_->cc);
                ciphertexts.push_back(ct);
                std::cout << "[Server] Party " << id << " 密文已加载" << std::endl;
            }

            if (comp_type == "average" || comp_type == "sum") {
                result = ciphertexts[0];
                for (size_t i = 1; i < ciphertexts.size(); i++) {
                    result = state_->cc->EvalAdd(result, ciphertexts[i]);
                }
                if (comp_type == "average") {
                    double divisor = static_cast<double>(ciphertexts.size());
                    result = state_->cc->EvalMult(result, 1.0 / divisor);
                    result = state_->cc->ModReduce(result);
                }
            } else if (comp_type.empty()) {
                response->set_completed(false);
                response->set_has_error(true);
                response->set_error("No expression and no computation_type specified");
                return Status::OK;
            } else {
                response->set_completed(false);
                response->set_has_error(true);
                response->set_error("Unknown computation type: " + comp_type);
                return Status::OK;
            }
        }

        // 存储结果
        std::string result_id = "result_" + std::to_string(time(nullptr));
        ComputationResult comp_result;
        comp_result.result_id = result_id;
        comp_result.ciphertext = result;
        comp_result.completed = true;

        auto chunks = OpenFHEGrpcSerializer::SerializeCiphertext(result, 0);
        for (const auto& chunk : chunks) {
            comp_result.ciphertext_data.insert(comp_result.ciphertext_data.end(),
                                               chunk.data.begin(), chunk.data.end());
        }
        state_->results[result_id] = comp_result;

        std::cout << "[Server] 计算完成: " << comp_type << ", 结果 ID: " << result_id << std::endl;

        response->set_completed(true);
        response->set_result_id(result_id);

    } catch (const std::exception& e) {
        std::cerr << "[Server] 计算失败: " << e.what() << std::endl;
        response->set_completed(false);
        response->set_has_error(true);
        response->set_error(std::string("Computation failed: ") + e.what());
    }

    return Status::OK;
}

Status ComputeServerGrpcService::GetStatus(
    ServerContext* context,
    const distributed_mpc::Empty* request,
    distributed_mpc::ServerStatus* response) {

    std::lock_guard<std::mutex> lock(state_->mutex);

    int connected = 0;
    for (const auto& [id, party] : state_->parties) {
        if (party.registered) connected++;
    }

    response->set_connected_parties(connected);
    response->set_total_parties(state_->expected_parties);
    response->set_status(state_->keygen_complete ? "ready" : "keygen");
    response->set_keygen_round(state_->current_keygen_round);
    response->set_joint_key_ready(state_->keygen_complete);
    response->set_ciphertexts_received(state_->ciphertexts_received);

    return Status::OK;
}

Status ComputeServerGrpcService::GetResult(
    ServerContext* context,
    const distributed_mpc::ResultRequest* request,
    ServerWriter<distributed_mpc::CiphertextChunk>* writer) {

    std::lock_guard<std::mutex> lock(state_->mutex);

    auto it = state_->results.find(request->submission_id());
    if (it == state_->results.end() || !it->second.completed) {
        return Status::OK;
    }

    auto chunks = OpenFHEGrpcSerializer::SerializeCiphertext(it->second.ciphertext, request->party_id());
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

    std::cout << "[Server] 结果已发送给 Party " << request->party_id() << std::endl;
    return Status::OK;
}
