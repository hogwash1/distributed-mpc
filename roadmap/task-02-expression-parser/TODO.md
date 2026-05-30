# Task 02 — TODO 清单

## Phase 1 ✅ 已完成

- [x] 阅读接口契约，引用 `ast_common.h`
- [x] 创建 `expr_parser.h` + `expr_parser.cpp`（7 种 OpType 递归解析）
- [x] `parse_json()` / `to_json()` — JSON ↔ AST 双向转换
- [x] `validate()` / `compute_depth()` / `collect_vars()` — AST 分析工具
- [x] 6 个测试全部通过
- [x] 内置极简 JSON Scanner（零外部依赖）

## Phase 2 待办

- [ ] **P1**: 比较操作表达式（`gt`/`lt`/`eq` for EvalCompare）
- [ ] **P1**: 条件分支语法（`{"op":"if","cond":...,"then":...,"else":...}`）
- [ ] **P1**: 表达式白名单/黑名单过滤
- [ ] **P2**: 性能基准测试（100 节点 AST 解析延迟）
- [ ] **P3**: YAML 格式输入支持

## 协作接口

- **丙 (Task 3)**：`parse_json()` 输出可直传 `HeEvaluator` ✅
- **丁 (Task 4)**：`--expr` 参数已集成 ✅
