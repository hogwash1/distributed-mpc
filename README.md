# 基于同态加密的多方任意函数安全计算平台

> 最小原型 | 14/15 测试通过 | 零外部依赖 (除 OpenFHE)

## 最小原型思想

不是"先搭框架再填功能"，而是：

1. **先跑通最小闭环** — 两个向量在同态下加法并验证
2. **每次只加一个能力** — AST、JSON 解析、多方密钥，逐层叠加
3. **每层独立可测** — 每个 Task 都有自己的测试，不依赖后续模块
4. **删掉不是核心的** — gRPC 分布式、Proto 序列化，确认不是当前必需就删除

核心只有三个问题要回答：

- **表达式**是什么？ → `ast_common.h`（7 种 OpType 覆盖所有算术运算）
- **怎么算**？ → `he_evaluator.cpp`（递归遍历 AST，调用 OpenFHE）
- **谁来解**？ → `threshold_decrypt.h`（多方部分解密融合）

其余一切（JSON 输入、CLI、测试框架）都是这三个核心的外围。

---

## 功能

- **7 种同态运算**: `+`, `-`, `×`, 取负, 常数, 变量, 除以常数
- **JSON 表达式输入**: 零外部依赖的递归下降解析器
- **CKKS 策略 A 优化**: 常数节点直接明文运算，不消耗乘法深度
- **多方联合密钥生成**: 2/3/5 方链式 MultipartyKeyGen
- **阈值解密**: Lead + Main + Fusion 的完整解密流程
- **单进程 MPC Demo**: 一键模拟 3 方完整计算流程

---

## 项目结构

```
├── task-01-ast/              # AST 抽象语法树定义
│   └── ast_common.h
│
├── task-02-parser/           # JSON 表达式解析器
│   ├── expr_parser.h
│   ├── expr_parser.cpp
│   └── test_expr_parser.cpp       → 6/6 ✅
│
├── task-03-evaluator/        # 同态计算引擎
│   ├── he_evaluator.h
│   └── he_evaluator.cpp
│
├── task-04-integration/      # 端到端集成验证
│   └── test_integration.cpp       → 4/4 ✅
│
├── task-05-multiparty/       # 多方密钥 + Demo
│   ├── multiparty_keygen.h       # 联合密钥生成
│   ├── threshold_decrypt.h       # 阈值解密
│   ├── test_multiparty.cpp       → 4/5 ✅
│   └── mpc_demo.cpp              # 单进程 3 方 Demo
│
├── CMakeLists.txt
├── run_tests.sh
└── README.md
```

---

## 快速开始

### 前置条件

- Ubuntu 20.04+ / WSL
- OpenFHE 1.4.2+ (`/usr/local/lib/OpenFHE`)
- CMake 3.25+, g++ 11+

### 一键测试

```bash
bash run_tests.sh
```

预期输出：

```
[2/5] ExprParser — 6 测试
    [✓ 通过] 简单加法 / 嵌套复杂 / 往返 / 非法参与方 / 深度 / 收集变量
    结果: 6 / 6 个测试通过

[3/5] 集成测试 — 4 测试
    [✓ 通过] 两方加法  err=1.4e-13
    [✓ 通过] 三方平均  err=2.0e-13
    [✓ 通过] A×B+C     err=1.5e-12
    [✓ 通过] 深度校验
    结果: 4 / 4 个测试通过

[4/5] 多方密钥+计算 — 5 测试
    [✓ 通过] 2/3/5 方密钥生成
    [✓ 通过] 3 方加密计算解密  err=2.3e-13

[5/5] MPC Demo — 3 方联合计算演示
```

### 运行 MPC Demo

```bash
cd build
./mpc_demo --parties 3 \
  --data1 "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16" \
  --data2 "5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20" \
  --data3 "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16" \
  --expr '{"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}}'
```

输出 6 个阶段：

```
[阶段1] ✓ 3 方联合密钥已生成
[阶段2] ✓ 各方数据已加密
[阶段3] ✓ 表达式解析完成
[阶段4] ✓ 同态求值完成 (ops=3, depth=0)
[阶段5] ✓ 阈值解密融合成功
[阶段6] 结果: 6.0, 8.0, 10.0, ...  (data1 + data2)
```

---

## 支持的表达式

```json
// 两方加法
{"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}}

// 三方平均值
{"op":"div_const","value":3.0,"lhs":{"op":"add",...}}

// 混合 A×B + C
{"op":"add","lhs":{"op":"mul","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}},"rhs":{"op":"var","party":3}}

// 含常数
{"op":"add","lhs":{"op":"mul","lhs":{"op":"var","party":1},"rhs":{"op":"const","value":2.5}},"rhs":{"op":"var","party":2}}

// 取负、减法
{"op":"negate","lhs":{"op":"var","party":1}}
{"op":"sub","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}}
```

---

## 测试覆盖

| 测试             | 数量 | 内容                                        |
| ---------------- | :--: | ------------------------------------------- |
| test_expr_parser |  6   | JSON 解析、嵌套、往返、校验、深度、变量收集 |
| test_integration |  4   | 两方加、三方平均、混合运算、深度超限        |
| test_multiparty  |  5   | 2/3/5 方密钥、加密计算解密、混合 `A×B+C`    |

**14/15 通过**。test_multiparty 第 5 个 `A×B+C` 因 OpenFHE 1.4.2 multiparty CKKS 乘法解密噪声问题已知失败，纯加法/平均等无乘法表达式完全可用。

---

## 数据流

```
      Party 1                 Party 2                 Party 3
        │                       │                       │
        ├─ KeyGen() ──────────►│ MultipartyKeyGen() ──►│ MultipartyKeyGen()
        │                       │                       │
        │                  pk_joint (联合公钥) ←────────┘
        │                       │                       │
   Encrypt(d1)            Encrypt(d2)            Encrypt(d3)
        │                       │                       │
        └───────────────┬───────┴───────────────┬───────┘
                        │                       │
                   HeEvaluator(ast, {ct1, ct2, ct3})
                        │
                    result_ct
                        │
          PartialDecrypt(sk1) + PartialDecrypt(sk2) + PartialDecrypt(sk3)
                        │
                    明文结果
```

---

## 下一步

- 修复 multiparty CKKS EvalMult 解密
- Bootstrap 支持（突破乘法深度限制）
- GPU 加速 NTT/FFT (RTX 4060)
- EvalCompare / EvalChebyshevSeries 非线性运算

## 注：任务的readme和todo文件由负责人自己完成并更新
