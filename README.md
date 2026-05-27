# Distributed MPC - 分布式多方隐私计算

基于 OpenFHE + gRPC 的三方门限同态加密系统，实现联合公钥生成、同态计算平均值、P2P 协作解密。

## 架构概览

```
Party 1                Party 2                Party 3
  │                      │                      │
  │ KeyGen()             │                      │
  │──pk₁──►Server───────►│                      │
  │                      │ MultipartyKeyGen(pk₁)│
  │                      │──pk₁₂──►Server──────►│
  │                      │                      │ MultipartyKeyGen(pk₁₂)
  │                      │                      │
  │◄────────── pk₁₂₃ (联合公钥) ──────────────│
  │                      │                      │
  │ Enc(pk₁₂₃, data)     │ Enc(pk₁₂₃, data)     │ Enc(pk₁₂₃, data)
  │──────────────────────►│──────────────────────►│
  │              Server: 同态计算 avg = (A+B+C)/3
  │                      │                      │
  │ PartialDecrypt       │ PartialDecrypt       │ PartialDecrypt
  │◄────P2P交换──────────►│◄────P2P交换──────────►│
  │                      │                      │
  │ Fusion (Party 1)     │                      │
  │═══ 最终明文结果 ══════│                      │
```

## 核心特性

- **真正的门限加密**：使用 OpenFHE 链式 `MultipartyKeyGen` 协议，联合私钥 `s = s₁ + s₂ + s₃`
- **服务器零知识**：服务器不持有任何私钥，仅做密文存储和同态计算
- **P2P 协作解密**：部分解密通过 gRPC P2P 直连交换，无需经过服务器
- **流式传输**：大密文和评估密钥通过 64KB 分块 + SHA256 校验和传输

## 环境要求

| 依赖 | 版本 | 说明 |
|------|------|------|
| OS | Ubuntu 20.04+ / WSL2 | 推荐 Linux 环境 |
| CMake | >= 3.25 | 构建系统 |
| C++ | C++20 | 编译器标准 |
| OpenFHE | >= 1.4.2 | 全同态加密库 |
| gRPC | 最新 | RPC 框架 |
| Protocol Buffers | 最新 | 序列化 |
| OpenSSL | 任意 | SHA256 校验 |

## 快速开始

### 1. 安装 OpenFHE

```bash
git clone https://github.com/openfheorg/openfhe-development.git
cd openfhe-development && mkdir build && cd build
cmake .. && make -j$(nproc) && sudo make install
```

### 2. 安装 gRPC

```bash
sudo apt install -y libgrpc++-dev protobuf-compiler
```

### 3. 构建项目

```bash
cd distributed-mpc

# 生成 proto 代码
mkdir -p proto-generated
protoc --cpp_out=./proto-generated \
       --grpc_out=./proto-generated \
       --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
       distributed_mpc.proto

# 编译
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 4. 运行测试

需要 4 个终端：

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

预期输出（Party 1 终端）：
```
[Client] ===== 解密融合成功! =====
[Client] 结果 (前16个): 20.0000 30.0000 40.0000 ...
```

## 项目结构

```
distributed-mpc/
├── distributed_mpc.proto      # gRPC 协议定义
├── compute_server.h            # 服务器头文件
├── compute_server.cpp          # 服务器实现（密钥协调 + 同态计算）
├── compute_server_main.cpp     # 服务器主程序
├── party_client.h              # 客户端头文件
├── party_client.cpp            # 客户端实现（链式密钥生成 + P2P 解密）
├── party_client_main.cpp       # 客户端主程序
├── grpc_serializer.h           # OpenFHE 对象序列化工具
├── CMakeLists.txt              # 构建配置
├── three_party_average.cpp     # OpenFHE 官方参考实现（单进程）
├── threshold_demo.cpp          # OpenFHE 两方参考实现（单进程）
└── proto-generated/            # proto 生成代码（需手动生成）
```

## gRPC 协议

### ComputeServer 服务（客户端 ↔ 服务器）

| RPC | 说明 |
|-----|------|
| `RegisterParty` | 参与方注册 |
| `InitKeyGen` | 初始化密钥生成协议（Party 1 调用） |
| `SubmitKeyGenRound1` | 提交第一轮密钥（Party 1） |
| `GetPrevPublicKey` | 获取前一轮公钥（阻塞等待） |
| `SubmitKeyGenRoundN` | 提交第 N 轮密钥（Party N） |
| `GetJointPublicKey` | 获取最终联合公钥（阻塞等待） |
| `SubmitCiphertext` | 流式提交密文 |
| `TriggerComputation` | 触发同态计算 |
| `GetResult` | 流式获取计算结果 |
| `GetStatus` | 查询服务器状态 |

### PartyP2P 服务（参与方 ↔ 参与方）

| RPC | 说明 |
|-----|------|
| `ExchangePartialDecrypt` | 流式发送部分解密 |
| `RequestPartialDecrypt` | 流式请求部分解密 |

## 密钥生成协议

```
Round 1: Party 1 → KeyGen() → 提交 pk₁ + evalMultKey + evalSumKeys
Round 2: Party 2 → GetPrevPublicKey(pk₁) → MultipartyKeyGen(pk₁) → 聚合 evalKeys → 提交 pk₁₂
Round 3: Party 3 → GetPrevPublicKey(pk₁₂) → MultipartyKeyGen(pk₁₂) → 聚合 evalKeys → 提交 pk₁₂₃

最终: pk₁₂₃ = 联合公钥，所有方使用 pk₁₂₃ 加密
解密: 三方各自 PartialDecrypt → P2P 交换 → Party 1 Fusion
```

## 命令行参数

### compute_server

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--port` | 60001 | 服务器监听端口 |
| `--parties` | 3 | 期望的参与方数量 |

### party_client

| 参数 | 默认值 | 说明 |
|------|--------|------|
| `--id` | 1 | 参与方 ID（必须从 1 开始） |
| `--name` | PartyA | 参与方名称 |
| `--server` | localhost:60001 | 服务器地址 |
| `--p2p-port` | 50050+id | P2P 监听端口 |
| `--data` | 10,20,... | 逗号分隔的本地数据 |

## 协同开发指南

### 分支策略

- `main` - 稳定版本
- `dev` - 开发分支
- `feature/*` - 功能分支

### 开发流程

```bash
git checkout dev
git pull origin dev
git checkout -b feature/your-feature

# 开发完成后
git add .
git commit -m "feat: 描述你的改动"
git push origin feature/your-feature
# 然后创建 Pull Request 到 dev
```

### Commit 规范

```
feat: 新功能
fix: 修复 bug
docs: 文档更新
refactor: 代码重构
test: 测试相关
chore: 构建/工具链
```

## 已知问题

| 优先级 | 问题 | 说明 |
|--------|------|------|
| 🔴 高 | 评估乘法密钥聚合不完整 | 分布式场景下 `MultiMultEvalKey` 需要多方私钥交互 |
| 🟡 中 | 无 TLS 加密 | gRPC 使用 insecure 模式 |
| 🟡 中 | 硬编码参与方数量 | 当前固定为 3 方 |
| 🟢 低 | 错误重试缺失 | 网络故障无自动重试 |

## 许可证

Private - 仅供团队内部使用
