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
│   ├── threshold/               #   两方门限同态加密演示
│   └── README.md                #   演示程序总览
├── file-exchange/               # 基于文件交换的三方安全计算
├── fideslib-demo/               # FIDESlib 同态加密加法演示
├── roadmap/                     # 🆕 Phase 1 开发路线图
│   ├── README.md                #   总览：架构、分工、接口契约
│   ├── task-01-protocol/        #   Task 1: 协议扩展与序列化
│   ├── task-02-expression-parser/ # Task 2: 表达式解析器
│   ├── task-03-he-evaluator/    #   Task 3: 同态计算引擎
│   └── task-04-integration/     #   Task 4: 服务端集成与联调
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

## 开发路线图

> **目标**：将当前仅支持 `average` 的系统，升级为支持**任意算术表达式**的通用安全计算平台。
>
> 详细分工、接口契约、TODO 清单见 [roadmap/README.md](./roadmap/README.md)

### 团队分工（4人）

| 成员 | 任务 | 说明 | 可立即开始 |
|:----:|------|------|:----:|
| **甲** | [Task 1: 协议扩展](./roadmap/task-01-protocol/) | 扩展 proto + AST 序列化 | ✅ |
| **乙** | [Task 2: 表达式解析器](./roadmap/task-02-expression-parser/) | JSON → AST 解析引擎 | ✅ |
| **丙** | [Task 3: 同态计算引擎](./roadmap/task-03-he-evaluator/) | AST → OpenFHE 运算执行 | ✅ |
| **丁** | [Task 4: 服务端集成](./roadmap/task-04-integration/) | 整合三个模块，端到端联调 | ⚠️ Day 3 |

### Phase 1 日程

```
Day 1-2:  Task 1、2、3 并行开发（三人独立，互不阻塞）
Day 3:    Task 4 集成联调
Day 4:    端到端测试 + 修复 + 合并
```

### Phase 1 验收标准

- 客户端通过 JSON 表达式定义任意算术函数（`+`, `-`, `*`, `/常数`, 取负）
- 服务器解析表达式并执行同态运算
- 通过 `(A+B+C)/3` 和 `A×B+C` 端到端测试
- 计算结果与明文误差 < 1e-4

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
