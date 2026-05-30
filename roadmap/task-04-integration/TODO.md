# Task 04 — TODO 清单

## Phase 1 ✅ 已完成

- [x] 拷贝 `he_evaluator` 到 `distributed-system/`
- [x] 重构 `compute_server.cpp` TriggerComputation（has_expression 分支 + HeEvaluator）
- [x] 修改 `party_client_main.cpp`（--expr / --expr-file 参数）
- [x] 修改 `party_client.cpp`（var_index 标注）
- [x] 更新 CMakeLists.txt
- [x] 4 个集成测试全部通过
- [x] 向下兼容旧 average/sum

## Phase 2 进行中

- [ ] **P0**：修复 gRPC 版本兼容 — proto 生成代码与系统 libgrpc++ 不匹配
  - 当前：`protoc 3.12.4` 生成的 `grpc.pb.cc` 使用 `CallbackServerContext` 等新版 API
  - 方案 A：用匹配的 protoc/grpc 版本重新生成
  - 方案 B：升级 WSL 中的 libgrpc++ 到 v1.40+
- [ ] **P0**：gRPC 端到端手动测试
  - 4 终端启动 compute_server + 3 个 party_client
  - 测试 JSON 表达式 `(A+B+C)/3`
  - 测试混合运算 `A*B + C`
  - 验证与旧版 average 结果一致（误差 < 1e-4）
- [ ] **P0**：错误场景测试（非法 party_id、缺失变量、深度超限）

## Phase 2 待办

- [ ] **P1**：性能基准测试
- [ ] **P2**：Docker 容器化（Dockerfile + docker-compose.yml）
- [ ] **P2**：Python SDK 封装
- [ ] **P2**：监控与日志（Prometheus metrics + Grafana dashboard）
- [ ] **P3**：4+ 参与方支持

## 已知问题

- `compute_server` 和 `party_client` 因 gRPC 版本不兼容无法编译（WSL libgrpc++ is older than generated code）
- 独立测试（test_ast_serializer / test_expr_parser / test_integration）全部通过
