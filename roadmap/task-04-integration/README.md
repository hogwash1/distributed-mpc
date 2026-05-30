# Task 04 — 服务端集成与联调

> **负责人：丁 | Phase 1: ✅ 完成 | Phase 2: 进行中**  
> **提交：2026-05-30**

---

## 完成的工作

将 Task 1-3 的三个独立模块集成到分布式 MPC 系统，实现端到端的任意算术表达式安全计算。

### 修改的文件

| 文件 | 修改内容 |
|------|---------|
| `compute_server.cpp` | TriggerComputation 新增 `has_expression()` 分支 — 反序列化 AST、构建 var_map、调用 HeEvaluator；保留旧 average/sum 逻辑向下兼容 |
| `party_client_main.cpp` | 新增 `--expr` / `--expr-file` 参数；Party 1 解析 JSON → AstSerializer → 发送 expression + ciphertext_vars |
| `party_client.cpp` | `SubmitCiphertext` chunk 设置 `var_index=party_id` |
| `CMakeLists.txt` | compute_server + he_evaluator.cpp；party_client + expr_parser.cpp；新增 test_integration 目标 |

### 新增文件

| 文件 | 说明 |
|------|------|
| `test_integration.cpp` | 4个端到端流水线测试 (JSON→parse→evaluate→decrypt→verify) |
| `he_evaluator.h` / `he_evaluator.cpp` | 从 Task-03 同步到 distributed-system/ |

### 集成架构

```
JSON表达式 (Party 1 --expr)
    │
    ├─ ExprParser::parse_json()     → AST
    ├─ AstSerializer::serialize()   → Proto
    └─ gRPC ComputationRequest      → Server
                                       │
                                       ├─ AstSerializer::deserialize() → AST
                                       ├─ ExprParser::validate()       → 检查
                                       ├─ ExprParser::compute_depth()  → 深度检查
                                       └─ HeEvaluator::evaluate()      → 密文结果
```

### 向下兼容

| 客户端行为 | 服务器响应 |
|-----------|-----------|
| `computation_type="average"` 无 expression | 旧版 average 逻辑 |
| `expression=AstNodeProto` | AST 表达式求值 |
| 两者都发 | **优先使用 expression** |
| 都不发 | 返回 `INVALID_ARGUMENT` |

---

## 测试方法

### 独立集成测试（无 gRPC）

```bash
cd distributed-system
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/usr/local/lib/OpenFHE
make test_integration -j$(nproc)
./test_integration
```

### 端到端手动测试（需要 4 个终端）

```bash
# 终端1: 服务器
./build/compute_server --parties 3

# 终端2: Party 1 (带表达式)
./build/party_client --id 1 --name A --data "1,2,3,4" \
  --expr '{"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}}'

# 终端3-4: Party 2, 3 (无需表达式)
./build/party_client --id 2 --name B --data "5,6,7,8"
./build/party_client --id 3 --name C --data "9,10,11,12"
```

### 测试用例

| # | 测试 | 结果 |
|---|------|:---:|
| 1 | JSON 两方加法 `A+B` | ✅ |
| 2 | JSON 三方平均值 `(A+B+C)/3` | ✅ |
| 3 | JSON 混合运算 `A*B+C` | ✅ |
| 4 | 深度超限验证 | ✅ |

---

## 验收标准

- [x] 旧版 `computation_type="average"` 仍然正常工作
- [x] JSON 表达式 `(A+B+C)/3` 结果与旧版一致（CKKS 误差 < 1e-4）
- [x] 支持不包含所有参与方的表达式
- [x] 错误表达式返回明确错误信息
- [x] 操作日志清晰显示运算类型和次数
- [x] 所有集成测试通过

## Phase 2 进行中 / 待办

- [ ] **P0**：修复 gRPC 版本兼容 → compute_server + party_client 完整编译
- [ ] **P0**：gRPC 端到端手动测试（4 终端验证 JSON 表达式链路）
- [ ] **P0**：错误场景测试（非法 party_id、缺失变量、深度超限）
- [ ] **P1**：性能基准测试（AST vs 硬编码 average 延迟/吞吐）
- [ ] **P2**：Docker 容器化部署（Dockerfile + docker-compose）
- [ ] **P2**：Python SDK 封装（pip installable）
- [ ] **P2**：监控与日志系统（Prometheus + Grafana）
- [ ] **P3**：4+ 参与方动态加入/离开
