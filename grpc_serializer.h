//==================================================================================
// OpenFHE gRPC 序列化器
// 负责将 OpenFHE 密文对象序列化为字节流，以便通过 gRPC 传输
//==================================================================================

#ifndef GRPC_SERIALIZER_H
#define GRPC_SERIALIZER_H

#include <openfhe.h>
#include <vector>
#include <string>
#include <memory>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

using namespace lbcrypto;

// 序列化后的数据块
struct SerializedChunk {
    int32_t party_id;
    int32_t chunk_index;
    int32_t total_chunks;
    std::vector<uint8_t> data;
    bool is_last;
    std::string checksum;
};

// OpenFHE gRPC 序列化器
class OpenFHEGrpcSerializer {
public:
    // 分块大小 (64KB)
    static constexpr size_t CHUNK_SIZE = 64 * 1024;

    // 将密文序列化并分块
    static std::vector<SerializedChunk> SerializeCiphertext(
        const Ciphertext<DCRTPoly>& ct,
        int32_t party_id) {
        
        std::vector<SerializedChunk> chunks;
        
        // 使用二进制序列化
        std::stringstream ss;
        lbcrypto::Serial::Serialize(ct, ss, lbcrypto::SerType::BINARY);
        std::string data = ss.str();
        
        // 计算总块数
        int32_t total_chunks = (data.size() + CHUNK_SIZE - 1) / CHUNK_SIZE;
        
        // 分块
        size_t offset = 0;
        int32_t chunk_index = 0;
        while (offset < data.size()) {
            SerializedChunk chunk;
            chunk.party_id = party_id;
            chunk.chunk_index = chunk_index;
            chunk.total_chunks = total_chunks;
            
            size_t remaining = data.size() - offset;
            size_t copy_size = std::min(remaining, CHUNK_SIZE);
            chunk.data.assign(data.begin() + offset, data.begin() + offset + copy_size);
            chunk.is_last = (offset + copy_size >= data.size());
            chunk.checksum = ComputeChecksum(chunk.data);
            
            chunks.push_back(std::move(chunk));
            offset += copy_size;
            chunk_index++;
        }
        
        return chunks;
    }

    // 从字节流重组密文
    static Ciphertext<DCRTPoly> DeserializeCiphertext(
        const std::vector<SerializedChunk>& chunks,
        CryptoContext<DCRTPoly> cc) {
        
        // 重组数据
        std::stringstream ss;
        for (const auto& chunk : chunks) {
            ss.write(reinterpret_cast<const char*>(chunk.data.data()), chunk.data.size());
        }
        
        // 反序列化
        Ciphertext<DCRTPoly> ct;
        lbcrypto::Serial::Deserialize(ct, ss, lbcrypto::SerType::BINARY);
        
        return ct;
    }

    // 从原始字节数据重组密文
    static Ciphertext<DCRTPoly> DeserializeCiphertextFromBytes(
        const std::vector<uint8_t>& data,
        CryptoContext<DCRTPoly> cc) {
        
        std::string str_data(data.begin(), data.end());
        std::stringstream ss(str_data);
        
        Ciphertext<DCRTPoly> ct;
        lbcrypto::Serial::Deserialize(ct, ss, lbcrypto::SerType::BINARY);
        
        return ct;
    }

    // 计算 SHA256 校验和
    static std::string ComputeChecksum(const std::vector<uint8_t>& data) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(data.data(), data.size(), hash);
        
        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        }
        return ss.str();
    }

    // 验证校验和
    static bool VerifyChecksum(const std::vector<uint8_t>& data, const std::string& expected) {
        std::string actual = ComputeChecksum(data);
        return actual == expected;
    }

    // 将字节数据转为 base64 (用于日志输出)
    static std::string BytesToBase64(const std::vector<uint8_t>& data) {
        static const char* base64_chars = 
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
        
        std::string ret;
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];
        
        for (size_t k = 0; k < data.size(); k++) {
            char_array_3[i++] = data[k];
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;
                
                for(i = 0; i < 4; i++) {
                    ret += base64_chars[char_array_4[i]];
                }
                i = 0;
            }
        }
        
        if (i > 0) {
            for(j = i; j < 3; j++) {
                char_array_3[j] = '\0';
            }
            
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            
            for (j = 0; j < i + 1; j++) {
                ret += base64_chars[char_array_4[j]];
            }
            
            while((i++ < 3)) {
                ret += '=';
            }
        }
        
        return ret;
    }
};

#endif // GRPC_SERIALIZER_H
