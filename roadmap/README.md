# 多方任意函数安全计算平台 — 开发路线图

> **总协调人请先阅读本文**，了解整体架构、任务依赖、接口契约和协作流程。

---

## 一、项目目标

将当前仅支持 `average` 的分布式 MPC 系统，升级为支持**任意算术表达式**的通用安全计算平台。

## 二、整体架构

```
用户输入 (JSON表达式)
       │
       ▼
┌──────────────────────┐
│ Task 2: 表达式解析器  │  JSON → AST (抽象语法树)
│ expr_parser.h/.cpp   │
└─────────┬────────────┘
          │ AST 数据结构
          ▼
┌──────────────────────┐
│ Task 3: 同态计算引擎  │  AST → OpenFHE 同态运算序列
│ he_evaluator.h/.cpp  │  EvalAdd / EvalSub / EvalMult ...
└─────────┬────────────┘
          │ 密文结果
          ▼
┌──────────────────────┐
│ Task 1: 协议扩展      │  proto 消息类型 → gRPC 传输
│ distributed_mpc.proto │  gRPC 序列化/反序列化
└─────────┬────────────┘
          │
          ▼
┌──────────────────────┐
│ Task 4: 服务端集成    │  compute_server 调用 evaluator
│ compute_server.cpp   │  party_client 发送表达式
└──────────────────────┘
```

## 三、四人分工与并行策略

| 成员 | 任务 | 代号 | 产出文件 | 可并行开始 |
|------|------|------|---------|:----:|
| **甲** | 协议扩展 + 序列化 | Task 1 | `distributed_mpc.proto`, `ast_serializer.h` | ✅ 立即 |
| **乙** | 表达式解析器 | Task 2 | `expr_parser.h`, `expr_parser.cpp` | ✅ 立即 |
| **丙** | 同态计算引擎 | Task 3 | `he_evaluator.h`, `he_evaluator.cpp` | ✅ 立即 |
| **丁** | 服务端集成 + 联调 | Task 4 | `compute_server.cpp`, `party_client.cpp` | ⚠️ 等 Task 1-3 完成 |

**并行关键**：Task 1、2、3 共享**统一的 AST 接口契约**（见下文），三方可按契约独立开发，互不阻塞。

---

## 四、核心接口契约（所有人必须遵守）

三个任务通过以下 AST 数据结构通信。此结构定义在 `ast_common.h` 中，由 Task 1 负责创建，Task 2/3 引用。

```cpp
// ast_common.h — 所有模块共享的 AST 定义

#ifndef AST_COMMON_H
#define AST_COMMON_H

#include <cstdint>
#include <vector>
#include <memory>
#include <string>

namespace mpc {

enum class OpType : int32_t {
    ADD       = 0,   // lhs + rhs
    SUB       = 1,   // lhs - rhs
    MUL       = 2,   // lhs * rhs
    NEGATE    = 3,   // -operand (一元运算，只用 lhs)
    CONST     = 4,   // 明文常数 (const_value)
    VAR       = 5,   // 参与方变量 (var_party_id)
    DIV_CONST = 6,   // lhs / 常数 (lhs * (1/const_value))
};

struct AstNode {
    OpType op;
    double  const_value   = 0.0;   // CONST / DIV_CONST 的值
    int32_t var_party_id  = 0;     // VAR 的参与方 ID

    std::shared_ptr<AstNode> lhs;   // 左子节点
    std::shared_ptr<AstNode> rhs;   // 右子节点 (NEGATE 时忽略)

    // 便捷工厂方法
    static std::shared_ptr<AstNode> make_const(double v);
    static std::shared_ptr<AstNode> make_var(int32_t party);
    static std::shared_ptr<AstNode> make_binary(
        OpType op, std::shared_ptr<AstNode> l, std::shared_ptr<AstNode> r);
    static std::shared_ptr<AstNode> make_unary(
        OpType op, std::shared_ptr<AstNode> operand);
};

} // namespace mpc
#endif
```

**JSON 表达式格式**（Task 2 输入，Task 4 通过 gRPC 传输）：
```json
{
  "op": "div_const",
  "const_value": 3.0,
  "lhs": {
    "op": "add",
    "lhs": {"op": "add", "lhs": {"op": "var", "party": 1}, "rhs": {"op": "var", "party": 2}},
    "rhs": {"op": "var", "party": 3}
  }
}
```
等价于 `(data₁ + data₂ + data₃) / 3`（三方平均值）。

---

## 五、任务依赖关系

```
Task 1 (协议) ──┐
                ├──► Task 4 (集成)
Task 2 (解析器)─┤
                │
Task 3 (求值器)─┘

第一阶段 (Day 1-2):  Task 1、2、3 并行开发
第二阶段 (Day 3):    Task 4 集成联调
第三阶段 (Day 4):    端到端测试 + 修复
```

## 六、各任务入口

| 任务 | 目录 | 详细说明 |
|------|------|---------|
| Task 1 | [task-01-protocol/](./task-01-protocol/) | 协议扩展与序列化 |
| Task 2 | [task-02-expression-parser/](./task-02-expression-parser/) | JSON 表达式解析器 |
| Task 3 | [task-03-he-evaluator/](./task-03-he-evaluator/) | 同态计算执行引擎 |
| Task 4 | [task-04-integration/](./task-04-integration/) | 服务端集成与联调 |

---

## 七、开发流程

```bash
# 1. 每人从 dev 分支创建自己的 feature 分支
git checkout dev
git checkout -b feature/task-01-protocol    # 甲
git checkout -b feature/task-02-parser      # 乙
git checkout -b feature/task-03-evaluator   # 丙
git checkout -b feature/task-04-integration # 丁

# 2. 开发完成后提交 PR 到 dev
git add .
git commit -m "feat(task-01): 扩展 proto 支持计算图"
git push origin feature/task-01-protocol

# 3. 丁负责合并 Task 1-3 后进行集成
```

## 八、Phase 1 验收标准

1. 客户端可以通过 JSON 表达式定义任意算术函数
2. 服务器能解析表达式并执行对应的同态运算
3. 正确支持 `+`, `-`, `*`, `/常数`, 取负
4. 至少通过 `(A+B+C)/3` 和 `A*B + C` 两个端到端测试
5. 计算结果与明文计算误差 < 1e-4（CKKS 精度范围）