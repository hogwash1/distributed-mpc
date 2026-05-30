# Phase 2 开发计划

> 日期: 2026-05-30 | Phase 1 (Task 1-4) ✅ 已完成  
> 最后更新: roadmap 四个任务文档 + 主 README 已同步

---

## 文档更新状态

| 文件 | 状态 |
|------|:---:|
| `roadmap/README.md` | ✅ 已更新（Phase 1 验收达成 + Phase 2 分级） |
| `roadmap/task-01-protocol/README.md` + TODO.md | ✅ 已更新 |
| `roadmap/task-02-expression-parser/README.md` + TODO.md | ✅ 已更新 |
| `roadmap/task-03-he-evaluator/README.md` + TODO.md | ✅ 已更新 |
| `roadmap/task-04-integration/README.md` + TODO.md | ✅ 已更新 |

---

## 一、短期 — 端到端验证与修复 (1-2天)

### 1.1 gRPC 端到端手动测试
- [ ] 在 WSL 中启动 compute_server + 3个 party_client
- [ ] 测试 JSON 表达式 `(A+B+C)/3`（验证与旧版 average 一致）
- [ ] 测试混合运算 `A*B + C`
- [ ] 测试错误场景：非法 party_id、缺失变量、深度超限

### 1.2 修复 gRPC 编译兼容
- [ ] `proto-generated/distributed_mpc.grpc.pb.cc` 与当前 gRPC 版本不兼容
- [ ] 选项 A: 重新生成 proto（匹配 gRPC 版本）
- [ ] 选项 B: 升级/降级 gRPC 库版本

### 1.3 编译 compute_server + party_client
- [ ] 确认 CMakeLists.txt 覆盖所有新源文件
- [ ] 解决 grpc_serializer 与 he_evaluator 的 OpenFHE 头文件路径冲突
- [ ] 确保 WSL 中能编译通过完整系统

---

## 二、中期 — 系统加固 (3-5天)

### 2.1 性能优化
- [ ] HeEvaluator CONST 优化正确性基准测试
- [ ] 对比 AST 表达式 vs 硬编码 average 的延迟与吞吐量
- [ ] GPU 加速评估（利用 CUDA 环境配置）

### 2.2 错误处理增强
- [ ] 表达式解析失败时的客户端重试机制
- [ ] 服务器端 AST 深度预检（在调用 evaluate 前）
- [ ] 部分解密失败的回滚与重试

### 2.3 安全增强
- [ ] 表达式白名单/黑名单过滤
- [ ] 密文大小/复杂度限制（防 DoS）
- [ ] 参与方认证 + 会话管理

---

## 三、长期 — 功能扩展 (1-2周)

### 3.1 表达式能力扩展
- [ ] 支持比较操作 (EvalCompare)
- [ ] 支持条件分支 (EvalChebyshevSeries for sign/tanh)
- [ ] Bootstrap 支持（深度受限时刷新密文）

### 3.2 多方扩展
- [ ] 支持 4+ 参与方（当前硬编码 3 方）
- [ ] 动态加入/离开参与方
- [ ] 门限可配置（t-out-of-n）

### 3.3 系统工程
- [ ] 配置文件替代命令行参数（YAML/JSON）
- [ ] Docker 容器化部署
- [ ] 监控与日志系统
- [ ] Python SDK 封装

---

## 四、当前工作目录结构

```
隐私计算demo/
├── README.md
├── distributed-system/       # ★ 主代码库
│   ├── ast_common.h          # Task-01 产出
│   ├── ast_serializer.h      # Task-01 产出
│   ├── expr_parser.h/.cpp    # Task-02 产出
│   ├── he_evaluator.h/.cpp   # Task-03 产出
│   ├── compute_server.cpp    # Task-04 修改
│   ├── party_client_main.cpp # Task-04 修改
│   ├── party_client.cpp      # Task-04 修改
│   ├── distributed_mpc.proto # Task-01 修改
│   ├── CMakeLists.txt        # 全部更新
│   ├── test_*.cpp            # 4个测试文件 (18用例)
│   └── proto-generated/      # ✅ 已生成
├── roadmap/
│   ├── README.md
│   ├── task-01-protocol/     # ✅ 完成
│   ├── task-02-expression-parser/ # ✅ 完成
│   ├── task-03-he-evaluator/ # ✅ 完成
│   └── task-04-integration/  # ✅ 完成
├── demos/
├── fideslib-demo/
└── file-exchange/
```

---

## 五、优先级矩阵

| 优先级 | 任务 | 预计工时 | 阻塞项 |
|:---:|------|:---:|------|
| 🔴 P0 | gRPC 端到端测试 | 4h | grpc.pb.cc 兼容 |
| 🔴 P0 | compute_server 编译通过 | 2h | grpc_serializer 头文件 |
| 🟡 P1 | 错误场景测试 | 3h | P0 完成 |
| 🟡 P1 | 性能基准测试 | 4h | P0 完成 |
| 🟢 P2 | Docker 部署 | 8h | — |
| 🟢 P2 | Python SDK | 12h | — |
| 🔵 P3 | 比较/条件运算 | 16h | — |
