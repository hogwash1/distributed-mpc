# Task 03 — 同态计算引擎 HE Evaluator

> **负责人：丙 | Phase 1: ✅ 完成 | Phase 2: 待开始**  
> **提交：2026-05-29 | 测试：2026-05-30**

---

## Phase 1 完成的工作

实现了同态计算引擎 `HeEvaluator`，支持遍历 AST 抽象语法树并调用 OpenFHE 执行对应的同态运算。

### 产出文件

| 文件 | 说明 |
|------|------|
| `ast_common.h` | AST 接口契约 — OpType 枚举(7种)、AstNode 结构体、工厂方法 |
| `he_evaluator.h` | HeEvaluator 类声明 + HeEvalError 异常类 |
| `he_evaluator.cpp` | 递归求值实现：策略A CONST 明文优化 + 全7种 OpType |
| `test_he_evaluator.cpp` | 5个测试用例，单进程 CKKS |
| `CMakeLists.txt` | 构建配置（仅依赖 OpenFHE） |

### 核心设计

- **策略 A (CONST 优化)**：二元运算中遇到 CONST 子节点时，直接用 `EvalAdd(ct, plaintext)` / `EvalMult(ct, scalar)`，避免加密常数浪费 slots
- **DIV_CONST**：用 `EvalMult(val, 1.0/const_value)` 实现，不消耗额外乘法深度
- **操作日志**：每次同态运算输出 `[HeEvaluator] OPNAME: depth=X, ops=Y`
- **错误处理**：缺失变量抛出 `HeEvalError`，零除抛出异常

---

## 测试方法

### 构建

```bash
cd task-03-he-evaluator
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 运行测试

```bash
./build/test_he_evaluator
```

### 测试用例覆盖

| # | 测试 | 输入 | 验证方式 |
|---|------|------|---------|
| 1 | 两方加法 | `data1 + data2` | 解密 vs 明文，误差 < 1e-4 |
| 2 | 三方平均值 | `(data1+data2+data3)/3` | 解密 vs 明文，误差 < 1e-4 |
| 3 | 多项式 | `data1^2 + data1*2 + 1` | 解密 vs 明文，误差 < 1e-4 |
| 4 | 深度追踪 | `(data1*data2)*data3` | 断言 `depth_consumed_ == 2` |
| 5 | 缺失变量 | `data1 + data9` (party 9 缺失) | 断言抛出 `HeEvalError` |

---

## 验收标准

- [x] 支持全部 7 种 OpType 的正确求值
- [x] `CONST` 节点不消耗额外的加密次数（策略A）
- [x] `DIV_CONST` 使用乘以倒数实现，不消耗额外乘法深度
- [x] `depth_consumed_` 与 `op_count_` 追踪准确
- [x] 缺失变量时抛出明确异常
- [x] 所有测试用例通过，与明文计算结果误差 < 1e-4

## Phase 2 待办

- [ ] 集成 Bootstrap 支持（深度受限时刷新密文）
- [ ] EvalCompare 支持（比较操作）
- [ ] EvalChebyshevSeries 支持（sign/tanh 等非线性）
- [ ] GPU 加速评估（CUDA 环境已配置 RTX 4060）
- [ ] 性能基准：AST 表达式 vs 硬编码 average 延迟对比
- [ ] 多方环境 (MULTIPARTY) 下的 HeEvaluator 测试
