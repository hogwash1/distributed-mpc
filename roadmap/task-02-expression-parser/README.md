# Task 02 — 表达式解析器

> **负责人：乙**
> **预计工时：2 天**
> **依赖：接口契约（ast_common.h），由 Task 1 提供**

---

## 任务目标

实现一个 JSON 表达式解析器，将用户输入的 JSON 表达式字符串转换为 `AstNode` 抽象语法树。

## 需要完成的工作

### 2.1 引用公共接口

从 Task 1 获取 `distributed-system/ast_common.h`（或自己先按接口契约创建一个临时版本开始开发）。

### 2.2 实现 JSON 解析器

**文件**：`distributed-system/expr_parser.h`

```cpp
#ifndef EXPR_PARSER_H
#define EXPR_PARSER_H

#include "ast_common.h"
#include <string>
#include <stdexcept>

namespace mpc {

class ExprParser {
public:
    // 从 JSON 字符串解析为 AST
    // 抛出 ExprParseError 当格式错误时
    static std::shared_ptr<AstNode> parse_json(const std::string& json);

    // 从 AST 反序列化为 JSON（调试用）
    static std::string to_json(const std::shared_ptr<AstNode>& node);
};

class ExprParseError : public std::runtime_error {
public:
    explicit ExprParseError(const std::string& msg)
        : std::runtime_error(msg) {}
};

} // namespace mpc
#endif
```

### 2.3 JSON 格式规范

支持以下节点类型：

| JSON op 值 | 对应 OpType | 子节点 | 额外字段 |
|-----------|-------------|--------|---------|
| `"add"` | ADD | lhs, rhs | 无 |
| `"sub"` | SUB | lhs, rhs | 无 |
| `"mul"` | MUL | lhs, rhs | 无 |
| `"negate"` | NEGATE | lhs | 无 |
| `"const"` | CONST | 无 | `"value": 3.14` |
| `"var"` | VAR | 无 | `"party": 1` |
| `"div_const"` | DIV_CONST | lhs | `"value": 3.0` |

**示例输入**：
```json
{
  "op": "add",
  "lhs": {"op": "mul", "lhs": {"op": "var", "party": 1}, "rhs": {"op": "const", "value": 2.5}},
  "rhs": {"op": "var", "party": 2}
}
```
表示：`data₁ × 2.5 + data₂`

### 2.4 实现解析逻辑（expr_parser.cpp）

**文件**：`distributed-system/expr_parser.cpp`

使用递归下降解析。伪代码：
```
parse_json(json_obj):
    op = json_obj["op"]
    switch op:
        case "add"      → make_binary(ADD, parse(lhs), parse(rhs))
        case "sub"      → make_binary(SUB, parse(lhs), parse(rhs))
        case "mul"      → make_binary(MUL, parse(lhs), parse(rhs))
        case "negate"   → make_unary(NEGATE, parse(lhs))
        case "const"    → make_const(json_obj["value"])
        case "var"      → make_var(json_obj["party"])
        case "div_const"→ make_binary(DIV_CONST, parse(lhs), make_const(json_obj["value"]))
        default         → throw ExprParseError
```

**注意**：不依赖任何第三方 JSON 库。只用 C++ 标准库。可以自己实现一个极简的 JSON tokenizer，或者使用 `nlohmann/json` (header-only) 如果项目允许。

> **约束**：本项目当前只依赖 OpenFHE + gRPC + OpenSSL。如需引入 JSON 库，建议使用 `nlohmann/json.hpp`（单头文件，MIT 协议），将文件放入 `distributed-system/` 目录。

### 2.5 实现 AST 验证器

在 `expr_parser.h` 中增加：

```cpp
// 验证 AST 合法性
static bool validate(const std::shared_ptr<AstNode>& node, int num_parties);

// 计算 AST 的乘法深度
static int compute_depth(const std::shared_ptr<AstNode>& node);

// 收集 AST 中用到的所有变量 party_id
static std::vector<int32_t> collect_vars(const std::shared_ptr<AstNode>& node);
```

- `validate`：检查 party_id 在 [1, num_parties] 范围内，结构完整
- `compute_depth`：返回最大乘法嵌套层数（用于检测是否超过 multDepth）
- `collect_vars`：返回去重后的变量列表（用于校验各方数据是否齐全）

### 2.6 编写单元测试

**文件**：`distributed-system/test_expr_parser.cpp`

测试用例：
1. 解析简单的 `data₁ + data₂`，验证 AST 结构
2. 解析 `(data₁ × 2.5 + data₂) / 3.0`，验证嵌套
3. `to_json` 输出的 JSON 可以被 `parse_json` 重新解析
4. `validate` 对非法 party_id 抛出异常
5. `compute_depth` 正确计算（纯加法深度=0，含一次乘法深度=1）
6. `collect_vars` 收集到正确的变量集合

## 产出清单

| 文件 | 状态 |
|------|:----:|
| `distributed-system/expr_parser.h` | 新建 |
| `distributed-system/expr_parser.cpp` | 新建 |
| `distributed-system/test_expr_parser.cpp` | 新建 |
| `distributed-system/nlohmann/json.hpp` | 新增（可选） |

## 验收标准

- [ ] 支持全部 7 种运算类型的解析
- [ ] 输入非法 JSON 时抛出明确的错误信息
- [ ] `validate` 能捕获常见的 AST 结构错误
- [ ] `compute_depth` 准确计算乘法深度
- [ ] 所有测试用例通过