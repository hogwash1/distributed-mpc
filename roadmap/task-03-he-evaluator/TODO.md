# Task 03 — TODO 清单

## Phase 1 ✅ 已完成

- [x] 阅读接口契约 + 创建 `ast_common.h`
- [x] 创建 `he_evaluator.h` + `he_evaluator.cpp`（7 种 OpType，策略 A CONST 优化）
- [x] 5 个测试全部通过（误差 < 1e-4）
- [x] 修复 Plaintext 单 slot bug（`make_scalar_plaintext()`）
- [x] 修复 VAR 深拷贝（EvalMult 共享指针混叠）
- [x] 测试环境适配（非 MULTIPARTY + EvalMultKey）

## Phase 2 待办

- [ ] **P1**: Bootstrap 刷新密文（突破 multDepth 限制）
- [ ] **P1**: EvalCompare / EvalChebyshevSeries 非线性运算
- [ ] **P1**: 多方 (MULTIPARTY) 环境完整测试
- [ ] **P2**: GPU CUDA 加速评估（RTX 4060）
- [ ] **P2**: 性能基准测试（100 节点 AST 延迟）

## 协作接口

- **丁 (Task 4)**：`HeEvaluator` 已集成到 `compute_server` ✅
