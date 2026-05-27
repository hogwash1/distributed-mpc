//==================================================================================
// Compute Server 实现 (完善版 - 支持密文反序列化)
//==================================================================================

#include "compute_server.h"
#include <iostream>
#include <sstream>

Status ComputeServerGrpcService::RegisterParty(
    ServerContext* context,
    const distributed_mpc::PartyRegistration* request,
    distributed_mpc::RegistrationAck* response) {

    std::lock_guard<std::mutex> lock(state_->mutex);

    int32_t party_id = request->party_id();

    // 检查是否已注册
    if (state_->parties.find(party_id) != state_->parties.end()) {
        if (state_->parties[party_id].registered) {
            response->set_success(false);
            response->set_error("Party already registered");
            return Status::OK;
        }
    }

    // 创建参与方信息
    PartyInfo info;
    info.party_id = party_id;
    info.party_name = request->party_name();
    info.p2p_address = context->peer();
    info.p2p_port = request->party_p2p_port();
    info.registered = true;
    info.cc = state_->cc;  // 共享服务器的 CryptoContext

    state_->parties[party_id] = info;

    // 生成 session_id (如果还没有)
    if (state_->session_id.empty()) {
        std::stringstream ss;
        ss << "session_" << time(nullptr);
        state_->session_id = ss.str();
    }

    // 返回确认
    response->set_success(true);
    response->set_session_id(state_->session_id);

    // 列出其他已注册的参与方
    for (const auto& [id, party] : state_->parties) {
        if (id != party_id && party.registered) {
            auto* other = response->add_other_parties();
            other->set_party_id(id);
            other->set_party_name(party.party_name);
            other->set_p2p_address(party.p2p_address);
            other->set_p2p_port(party.p2p_port);
        }
    }

    std::cout << "[Server] Party " << party_id << " (" << request->party_name() << ") 注册成功" << std::endl;
    std::cout << "[Server] 当前已注册: " << state_->parties.size() << "/" << state_->expected_parties << std::endl;

    return Status::OK;
}

Status ComputeServerGrpcService::SubmitCiphertext(
    ServerContext* context,
    ServerReader<distributed_mpc::CiphertextChunk>* reader,
    distributed_mpc::SubmissionAck* response) {

    distributed_mpc::CiphertextChunk chunk;
    std::vector<uint8_t> all_data;
    int32_t party_id = -1;
    int32_t total_chunks = 0;

    std::cout << "[Server] 开始接收密文..." << std::endl;

    // 读取所有分块
    while (reader->Read(&chunk)) {
        if (party_id == -1) {
            party_id = chunk.party_id();
            total_chunks = chunk.total_chunks();
            std::cout << "[Server] 接收 Party " << party_id << " 的密文, 共 " << total_chunks << " 块" << std::endl;
        }

        // 验证校验和
        std::string expected_checksum = chunk.checksum();
        std::string actual_checksum = OpenFHEGrpcSerializer::ComputeChecksum(
            std::vector<uint8_t>(chunk.data().begin(), chunk.data().end()));

        if (expected_checksum != actual_checksum) {
            std::cerr << "[Server] 校验和失败! 块 " << chunk.chunk_index() << std::endl;
            response->set_success(false);
            response->set_error("Checksum mismatch at chunk " + std::to_string(chunk.chunk_index()));
            return Status::OK;
        }

        // 追加数据
        all_data.insert(all_data.end(), chunk.data().begin(), chunk.data().end());

        if (chunk.is_last()) {
            std::cout << "[Server] 收到 Party " << party_id << " 的完整密文, 共 " << all_data.size() << " 字节" << std::endl;
            break;
        }
    }

    if (party_id == -1) {
        response->set_success(false);
        response->set_error("No data received");
        return Status::OK;
    }

    // 反序列化密文
    std::lock_guard<std::mutex> lock(state_->mutex);

    try {
        // 使用服务器的 CryptoContext 反序列化密文
        Ciphertext<DCRTPoly> ct = OpenFHEGrpcSerializer::DeserializeCiphertextFromBytes(
            all_data, state_->cc);

        // 存储到状态
        if (state_->parties.find(party_id) != state_->parties.end()) {
            state_->parties[party_id].ciphertext = ct;
            state_->parties[party_id].ciphertext_received = true;
            std::cout << "[Server] Party " << party_id << " 密文已存储" << std::endl;
        }

        response->set_success(true);
        response->set_submission_id("sub_" + std::to_string(party_id));

    } catch (const std::exception& e) {
        std::cerr << "[Server] 密文反序列化失败: " << e.what() << std::endl;
        response->set_success(false);
        response->set_error(std::string("Deserialization failed: ") + e.what());
    }

    return Status::OK;
}

Status ComputeServerGrpcService::TriggerComputation(
    ServerContext* context,
    const distributed_mpc::ComputationRequest* request,
    distributed_mpc::ComputationStatus* response) {

    std::lock_guard<std::mutex> lock(state_->mutex);

    std::string comp_type = request->computation_type();
    std::cout << "[Server] 收到计算请求: " << comp_type << std::endl;

    // 检查是否所有参与方的密文都已收到
    for (const auto& [id, party] : state_->parties) {
        if (!party.ciphertext_received) {
            std::cout << "[Server] 等待 Party " << id << " 的密文..." << std::endl;
            response->set_completed(false);
            response->set_has_error(true);
            response->set_error("Waiting for Party " + std::to_string(id) + " ciphertext");
            return Status::OK;
        }
    }

    std::cout << "[Server] 所有密文已就绪，开始计算..." << std::endl;

    try {
        // 获取所有密文
        std::vector<Ciphertext<DCRTPoly>> ciphertexts;
        for (const auto& [id, party] : state_->parties) {
            ciphertexts.push_back(party.ciphertext);
        }

        // 执行同态计算 (求平均值)
        if (comp_type == "average" || comp_type.empty()) {
            // 求和
            auto sum = ciphertexts[0];
            for (size_t i = 1; i < ciphertexts.size(); i++) {
                sum = state_->cc->EvalAdd(sum, ciphertexts[i]);
            }

            // 除以数量 (乘以 1/n)
            double inv_n = 1.0 / ciphertexts.size();
            auto avg = state_->cc->EvalMult(sum, inv_n);
            avg = state_->cc->ModReduce(avg);

            // 存储结果
            ComputationResult result;
            result.result_id = "result_avg_" + std::to_string(time(nullptr));
            result.ciphertext = avg;
            result.completed = true;
            result.computation_type = "average";
            state_->results[result.result_id] = result;

            std::cout << "[Server] 平均值计算完成, result_id=" << result.result_id << std::endl;

            response->set_completed(true);
            response->set_result_id(result.result_id);
            response->set_has_error(false);
        } else {
            response->set_completed(false);
            response->set_has_error(true);
            response->set_error("Unknown computation type: " + comp_type);
        }

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
    int ciphertexts_received = 0;
    for (const auto& [id, party] : state_->parties) {
        if (party.registered) connected++;
        if (party.ciphertext_received) ciphertexts_received++;
    }

    response->set_connected_parties(connected);
    response->set_total_parties(state_->expected_parties);

    std::string status_str = state_->session_id.empty() ? "waiting" : "running";
    if (ciphertexts_received == state_->expected_parties && state_->expected_parties > 0) {
        status_str = "ready_for_computation";
    }
    response->set_status(status_str);

    // 添加已完成的计算结果
    for (const auto& [id, result] : state_->results) {
        if (result.completed) {
            response->add_submissions(id);
        }
    }

    return Status::OK;
}

Status ComputeServerGrpcService::GetResult(
    ServerContext* context,
    const distributed_mpc::ResultRequest* request,
    ServerWriter<distributed_mpc::CiphertextChunk>* writer) {

    std::string result_id = request->submission_id();
    std::cout << "[Server] 收到结果请求, result_id=" << result_id << std::endl;

    std::lock_guard<std::mutex> lock(state_->mutex);

    // 查找结果
    auto it = state_->results.find(result_id);
    if (it == state_->results.end() || !it->second.completed) {
        std::cerr << "[Server] 结果未找到或未完成: " << result_id << std::endl;
        return Status::OK;
    }

    // 序列化结果密文并发送
    try {
        auto chunks = OpenFHEGrpcSerializer::SerializeCiphertext(
            it->second.ciphertext, 0);  // party_id=0 表示服务器

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

        std::cout << "[Server] 结果已发送, 共 " << chunks.size() << " 块" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Server] 结果序列化失败: " << e.what() << std::endl;
    }

    return Status::OK;
}
