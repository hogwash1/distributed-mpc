# Distributed System - gRPC 分布式多方安全计算系统

基于 OpenFHE 全同态加密库 + gRPC 框架构建的分布式多方安全计算（MPC）系统，支持三方门限同态加密。

## 功能概述

- **分布式密钥生成**：使用 OpenFHE 链式 `MultipartyKeyGen` 协议，三方协作生成联合公钥 `pk₁₂₃`，联合私钥 `s = s₁ + s₂ + s₃`，任意单方无法解密
- **同态计算**：服务器对加密数据进行同态平均值计算，全程无需解密
- **P2P 协作解密**：三方通过 gRPC P2P 直连交换部分解密结果，任意一方均可完成最终解密融合
- **流式传输**：大密文和评估密钥通过 64KB 分块 + SHA256 校验和可靠传输

## 架构

```
Party 1 ←→ Compute Server ←→ Party 2
  ↕                              ↕
Party 3 ←─────────────────────────┘
  (P2P 部分解密交换)
```

- **Compute Server**：密钥协调中心 + 同态计算引擎，不持有任何私钥
- **Party Client**：参与方客户端，内置 P2P gRPC 服务用于部分解密交换

## 文件说明

| 文件 | 说明 |
|------|------|
| `distributed_mpc.proto` | gRPC 协议定义（ComputeServer 服务 + PartyP2P 服务） |
| `compute_server.h` | 服务器头文件，定义服务端状态与接口 |
| `compute_server.cpp` | 服务器实现：密钥协调、同态计算、状态管理 |
| `compute_server_final.cpp` | 服务器完善版，修复密文反序列化问题 |
| `compute_server_main.cpp` | 服务器主程序入口 |
| `party_client.h` | 客户端头文件，定义客户端逻辑 |
| `party_client.cpp` | 客户端实现：链式密钥生成、加密上传、P2P 解密 |
| `party_client_main.cpp` | 客户端主程序入口 |
| `grpc_serializer.h` | OpenFHE 对象 gRPC 序列化工具（基础版） |
| `grpc_serializer_fixed.h` | 序列化修正版 v3，使用 ciphertext-ser.h 等头文件 |
| `CMakeLists.txt` | 构建配置（需 OpenFHE + gRPC + OpenSSL） |
| `ast_common.h` | 定义了抽象语法树（AST）的节点结构 AstNode 和支持的操作类型 OpType（加、减、乘、负号、常量、变量）。 |
| `expr_parser.h & expr_parser.cpp （核心逻辑解析器）` | 翻译（序列化与反序列化）质检（验证与属性分析） |
| `test_expr_parser.cpp （自动化测试）` | 针对上述所有功能编写了 6 个自动化测试用例。 |

## 构建运行

### 1. 生成 proto 代码

```bash
mkdir -p proto-generated
protoc --cpp_out=./proto-generated \
       --grpc_out=./proto-generated \
       --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
       distributed_mpc.proto
```

### 2. 编译

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 3. 运行（需 4 个终端）

```bash
# 终端 1: 启动服务器
./build/compute_server --parties 3

# 终端 2: Party 1 (Lead)
./build/party_client --id 1 --name PartyA \
  --data "10,20,30,40,50,60,70,80,90,100,110,120,130,140,150,160"

# 终端 3: Party 2
./build/party_client --id 2 --name PartyB \
  --data "20,30,40,50,60,70,80,90,100,110,120,130,140,150,160,170"

# 终端 4: Party 3
./build/party_client --id 3 --name PartyC \
  --data "30,40,50,60,70,80,90,100,110,120,130,140,150,160,170,180"
```

## 实现变体说明

- `compute_server.cpp` / `grpc_serializer.h`：基础实现
- `compute_server_final.cpp` / `grpc_serializer_fixed.h`：完善版本，修复了序列化问题
- 实际使用时选择一组版本，在 CMakeLists.txt 中切换源文件即可

## 依赖

- CMake >= 3.25
- C++20
- OpenFHE >= 1.4.2
- gRPC + Protocol Buffers
- OpenSSL（SHA256 校验）
