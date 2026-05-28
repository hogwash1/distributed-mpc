# Task 03 — 同态计算引擎

> **负责人：丙**
> **预计工时：2 天**
> **依赖：接口契约（ast_common.h），由 Task 1 提供**

---

## 任务目标

实现同态计算引擎 `HeEvaluator`，遍历 AST 抽象语法树，调用 OpenFHE 库执行对应的同态运算（EvalAdd / EvalSub / EvalMult / EvalNegate），返回最终密文结果。

## 需要完成的工作

### 3.1 引用公共接口

从 Task 1 获取 `distributed-system/ast_common.h`（或自己先按接口契约创建一个临时版本）。

### 3.2 实现同态求值器

**文件**：`distributed-system/he_evaluator.h`

```cpp
#ifndef HE_EVALUATOR_H
#define HE_EVALUATOR_H

#include "ast_common.h"
#include <openfhe.h>
#include <map>
#include <vector>
#include <stdexcept>

using namespace lbcrypto;

namespace mpc {

class HeEvaluator {
public:
    // 构造函数
    // cc:     加密上下文
    // vars:   party_id → Ciphertext 映射
    //         (例如 {1 → ct₁, 2 → ct₂, 3 → ct₃})
    HeEvaluator(CryptoContext<DCRTPoly> cc,
                const std::map<int32_t, Ciphertext<DCRTPoly>>& vars);

    // 执行 AST 求值，返回最终密文
    Ciphertext<DCRTPoly> evaluate(const std::shared_ptr<AstNode>& root);

    // 获取已消耗的乘法深度
    int get_depth_consumed() const { return depth_consumed_; }

    // 获取已执行的操作计数
    int get_op_count() const { return op_count_; }

private:
    CryptoContext<DCRTPoly> cc_;
    std::map<int32_t, Ciphertext<DCRTPoly>> vars_;
    int depth_consumed_ = 0;
    int op_count_ = 0;

    // 递归求值辅助函数
    Ciphertext<DCRTPoly> eval_node(const std::shared_ptr<AstNode>& node, int current_depth);
};

class HeEvalError : public std::runtime_error {
public:
    explicit HeEvalError(const std::string& msg)
        : std::runtime_error(msg) {}
};

} // namespace mpc
#endif
```

### 3.3 实现求值逻辑（he_evaluator.cpp）

**文件**：`distributed-system/he_evaluator.cpp`

核心逻辑伪代码：
```
eval_node(node, current_depth):
    switch node.op:
        case ADD:
            lhs = eval_node(node.lhs, current_depth)
            rhs = eval_node(node.rhs, current_depth)
            return cc->EvalAdd(lhs, rhs)
        case SUB:
            lhs = eval_node(node.lhs, current_depth)
            rhs = eval_node(node.rhs, current_depth)
            return cc->EvalSub(lhs, rhs)
        case MUL:
            lhs = eval_node(node.lhs, current_depth)
            rhs = eval_node(node.rhs, current_depth)
            depth_consumed_ = max(depth_consumed_, current_depth + 1)
            return cc->EvalMult(lhs, rhs)
        case NEGATE:
            val = eval_node(node.lhs, current_depth)
            return cc->EvalNegate(val)
        case CONST:
            plain = cc->MakeCKKSPackedPlaintext([node.const_value])
            return cc->Encrypt(public_key, plain)  // ★需要公钥★
        case VAR:
            return vars_[node.var_party_id]  // 直接返回已加密的变量
        case DIV_CONST:
            val = eval_node(node.lhs, current_depth)
            inv = 1.0 / node.const_value
            return cc->EvalMult(val, inv)  // 乘以倒数（降低深度消耗）
```

**重要问题**：`CONST` 节点的处理。

`CONST` 生成明文，但如果直接返回明文则无法与密文做后续运算。有两种策略：

- **策略 A（推荐）**：不加密常数，而是在遇到 `ADD(ct, CONST)` 时用 `EvalAdd(ct, plaintext)`，遇到 `MUL(ct, CONST)` 时用 `EvalMult(ct, const_value)`。这样避免消耗加密次数。
- **策略 B**：用公钥加密常数。需要 evaluator 持有公钥。

**建议采用策略 A**，在 eval_node 层面判断子节点类型做优化。需要额外传入公钥用于策略 B 或验证。

修改后的构造函数：
```cpp
HeEvaluator(CryptoContext<DCRTPoly> cc,
            PublicKey<DCRTPoly> pk,   // ★新增
            const std::map<int32_t, Ciphertext<DCRTPoly>>& vars);
```

### 3.4 实现操作日志

每执行一个同态运算，输出日志（可通过 `LIBVC_LOG` 或 `std::cout`）：

```
[HeEvaluator] ADD: depth=0, ops=1
[HeEvaluator] MUL: depth=1, ops=2
[HeEvaluator] SUB: depth=1, ops=3
...
```

方便调试和验证计算过程。

### 3.5 编写单元测试

**文件**：`distributed-system/test_he_evaluator.cpp`

测试用例（单进程，不依赖 gRPC）：

1. **两方加法**：`data₁ + data₂` → 加密 → 同态计算 → 解密验证
2. **三方平均值**：`(data₁ + data₂ + data₃) / 3.0` → 验证结果
3. **多项式**：`data₁² + data₁·2 + 1`（即 `data₁×data₁ + data₁×2 + 1`）→ 验证
4. **深度追踪**：含两层乘法的表达式，验证 `depth_consumed_ == 2`
5. **缺失变量**：vars 中缺少某个 party_id 时抛出 `HeEvalError`

测试代码结构参考 `demos/three-party-average/three_party_average.cpp`，使用单进程 OpenFHE 密钥生成。

## 产出清单

| 文件 | 状态 |
|------|:----:|
| `distributed-system/he_evaluator.h` | 新建 |
| `distributed-system/he_evaluator.cpp` | 新建 |
| `distributed-system/test_he_evaluator.cpp` | 新建 |

## 验收标准

- [ ] 支持全部 7 种 OpType 的正确求值
- [ ] `CONST` 节点不消耗额外的加密次数
- [ ] `DIV_CONST` 使用乘以倒数实现，不消耗额外乘法深度
- [ ] `depth_consumed_` 与 `op_count_` 追踪准确
- [ ] 缺失变量时抛出明确异常
- [ ] 所有测试用例通过，与明文计算结果误差 < 1e-4