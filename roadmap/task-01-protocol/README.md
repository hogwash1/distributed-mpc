# Task 01 — 协议扩展与序列化

> **负责人：甲**
> **预计工时：2 天**
> **依赖：无（可立即开始）**

---

## 任务目标

扩展 `distributed_mpc.proto` 协议和 `grpc_serializer`，使系统能够通过网络传输"计算表达式"，而不仅仅是"单一密文"。

## 当前状态

- `distributed_mpc.proto` 中 `ComputationRequest` 只有一个 `computation_type` 字符串字段（值固定为 `"average"`）
- `compute_server.cpp` 中 `TriggerComputation` 只能硬编码执行 `EvalAdd` 循环
- 多方数据各自独立上传，没有标识哪个密文对应哪个变量

## 需要完成的工作

### 1.1 创建 AST 公共头文件

**文件**：`distributed-system/ast_common.h`

包含 `OpType` 枚举和 `AstNode` 结构体定义（见 [roadmap/README.md](../README.md) 第四节接口契约）。

要求：
- 实现全部 4 个工厂方法
- 添加 `to_string()` 方法用于调试输出
- 添加 `clone()` 深拷贝方法

### 1.2 扩展 Proto 协议

**文件**：`distributed-system/distributed_mpc.proto`

新增以下消息类型：

```protobuf
// 操作类型枚举
enum OpType {
  OP_ADD = 0;
  OP_SUB = 1;
  OP_MUL = 2;
  OP_NEGATE = 3;
  OP_CONST = 4;
  OP_VAR = 5;
  OP_DIV_CONST = 6;
}

// AST 节点（递归结构）
message AstNodeProto {
  OpType op = 1;
  double const_value = 2;
  int32 var_party_id = 3;
  AstNodeProto lhs = 4;
  AstNodeProto rhs = 5;
}

// 扩展 ComputationRequest
// 在已有的 ComputationRequest 中新增字段：
//   AstNodeProto expression = 10;
//   repeated int32 ciphertext_vars = 11;  // 标识每个密文对应的变量 ID

// 扩展 SubmitCiphertextRequest
// 新增字段：
//   int32 var_index = 10;  // 此密文对应的变量索引
```

修改 `ComputationRequest`，保留 `computation_type` 字段向下兼容，新增 `expression` 字段。

### 1.3 实现 AST ↔ Proto 序列化

**文件**：`distributed-system/ast_serializer.h`

```cpp
namespace mpc {

class AstSerializer {
public:
    // C++ AST → Proto 消息
    static distributed_mpc::AstNodeProto serialize(
        const std::shared_ptr<AstNode>& node);
    
    // Proto 消息 → C++ AST
    static std::shared_ptr<AstNode> deserialize(
        const distributed_mpc::AstNodeProto& proto);
};

} // namespace mpc
```

要求：正确处理递归嵌套结构，不做扁平化。

### 1.4 更新 CMakeLists.txt

在 `distributed-system/CMakeLists.txt` 中：
- 确保 `ast_common.h` 和 `ast_serializer.h` 被 include
- 确认 proto 生成代码路径正确

### 1.5 编写单元测试

**文件**：`distributed-system/test_ast_serializer.cpp`

测试用例：
1. 构造 `(A+B)*C` → 序列化 → 反序列化 → 验证结构一致
2. 构造嵌套 5 层的表达式 → 序列化往返
3. 边界测试：单节点（CONST / VAR）

## 产出清单

| 文件 | 状态 |
|------|:----:|
| `distributed-system/ast_common.h` | 新建 |
| `distributed-system/ast_serializer.h` | 新建 |
| `distributed-system/distributed_mpc.proto` | 修改 |
| `distributed-system/test_ast_serializer.cpp` | 新建 |

## 验收标准

- [ ] `ast_common.h` 所有工厂方法可正常构造 AST
- [ ] proto 编译通过（`protoc` 无报错）
- [ ] `AstSerializer::serialize/deserialize` 往返后 AST 完全一致
- [ ] 测试用例全部通过