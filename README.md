# Distributed MPC - 分布式多方隐私计算

基于 OpenFHE + gRPC 的三方门限同态加密系统，实现联合公钥生成、同态计算平均值、P2P 协作解密。

## 项目结构

```
distributed-mpc/
├── distributed-system/          # gRPC 分布式多方安全计算系统（主项目）
│   ├── distributed_mpc.proto    #   gRPC 协议定义
│   ├── compute_server.h         #   服务器头文件
│   ├── compute_server.cpp       #   服务器实现
│   ├── compute_server_final.cpp #   服务器完善版
│   ├── compute_server_main.cpp  #   服务器主程序
│   ├── party_client.h           #   客户端头文件
│   ├── party_client.cpp         #   客户端实现
│   ├── party_client_main.cpp    #   客户端主程序
│   ├── grpc_serializer.h        #   序列化工具（基础版）
│   ├── grpc_serializer_fixed.h  #   序列化工具（修正版）
│   ├── CMakeLists.txt           #   构建配置
│   └── README.md                #   详细说明
├── demos/                       # 单进程演示程序（无需网络）
│   ├── three-party-average/     #   三方门限同态加密求平均值
│   │   ├── three_party_average.cpp
│   │   ├── CMakeLists.txt
│   │   └── README.md
│   ├── threshold/               #   两方门限同态加密演示
│   │   ├── threshold_demo.cpp
│   │   ├── CMakeLists.txt
│   │   └── README.md
│   └── README.md                #   演示程序总览
├── file-exchange/               # 基于文件交换的三方安全计算
│   ├── party_a_file.cpp         #   Party A - 协调方
│   ├── party_b_file.cpp         #   Party B - 参与方
│   ├── party_c_file.cpp         #   Party C - 参与方
│   ├── CMakeLists.txt           #   构建配置
│   └── README.md                #   详细说明
├── fideslib-demo/               # FIDESlib 同态加密加法演示
│   ├── eval_add_demo.cpp        #   加法演示程序
│   ├── CMakeLists.txt           #   构建配置
│   └── README.md                #   详细说明
├── .gitignore                   # Git 忽略规则
└── README.md                    # 本文件
```

## 目录导航

| 模块 | 说明 | 通信方式 | 运行方式 |
|------|------|---------|---------|
| [distributed-system/](./distributed-system/) | 完整 gRPC 分布式 MPC 系统 | gRPC 网络通信 | 4 个终端分别启动 |
| [demos/three-party-average/](./demos/three-party-average/) | 三方平均值计算演示 | 单进程（无网络） | 直接运行 |
| [demos/threshold/](./demos/threshold/) | 两方门限加密演示 | 单进程（无网络） | 直接运行 |
| [file-exchange/](./file-exchange/) | 文件交换三方 MPC | 共享文件系统 | 3 个终端按序启动 |
| [fideslib-demo/](./fideslib-demo/) | FIDESlib 加法演示 | 单进程（无网络） | 直接运行 |

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
| gRPC | 最新 | RPC 框架（仅 distributed-system） |
| Protocol Buffers | 最新 | 序列化（仅 distributed-system） |
| OpenSSL | 任意 | SHA256 校验（仅 distributed-system） |

## 快速开始

### 安装 OpenFHE

```bash
git clone https://github.com/openfheorg/openfhe-development.git
cd openfhe-development && mkdir build && cd build
cmake .. && make -j$(nproc) && sudo make install
```

### 安装 gRPC

```bash
sudo apt install -y libgrpc++-dev protobuf-compiler
```

### 构建与运行

参见各子目录的 README 文件获取详细说明：

- [distributed-system/README.md](./distributed-system/README.md) — 完整分布式系统
- [demos/README.md](./demos/README.md) — 单进程演示
- [file-exchange/README.md](./file-exchange/README.md) — 文件交换版
- [fideslib-demo/README.md](./fideslib-demo/README.md) — FIDESlib 演示

## 已知问题

| 优先级 | 问题 | 说明 |
|--------|------|------|
| 🔴 高 | 评估乘法密钥聚合不完整 | 分布式场景下 `MultiMultEvalKey` 需要多方私钥交互 |
| 🟡 中 | 无 TLS 加密 | gRPC 使用 insecure 模式 |
| 🟡 中 | 硬编码参与方数量 | 当前固定为 3 方 |
| 🟢 低 | 错误重试缺失 | 网络故障无自动重试 |

## 开发指南

### 分支策略

- `main` - 稳定版本
- `dev` - 开发分支
- `feature/*` - 功能分支

### Commit 规范

```
feat: 新功能
fix: 修复 bug
docs: 文档更新
refactor: 代码重构
test: 测试相关
chore: 构建/工具链
```

## 许可证

Private - 仅供团队内部使用