# Task 01 — 协议扩展与序列化

> **负责人：甲 | Phase 1: ✅ 完成 | Phase 2: 待开始**  
> **提交：2026-05-30**

---

## Phase 1 完成的工作

扩展分布式多方安全计算系统的 gRPC 协议，新增 AST 表达式支持，实现 AST 与 Proto 消息的双向转换。

### 产出文件

| 文件 | 说明 |
|------|------|
| `distributed-system/ast_common.h` | AST 接口契约 — OpType枚举(7种)、AstNode结构体、工厂方法、to_string()递归打印、clone()深拷贝 |
| `distributed-system/distributed_mpc.proto` | 协议扩展 — 新增 OpType 枚举、AstNodeProto 递归消息、ComputationRequest+expression/ciphertext_vars、CiphertextChunk+var_index |
| `distributed-system/ast_serializer.h` | AST↔Proto 双向转换 — AstSerializer 类，header-only 实现 |
| `distributed-system/test_ast_serializer.cpp` | 3个往返测试用例 |
| `distributed-system/CMakeLists.txt` | 新增 test_ast_serializer 构建目标 |
| `distributed-system/proto-generated/` | protoc 生成的 C++ 代码 |

### Proto 协议变更

```protobuf
// 新增 OpType 枚举 (与 C++ AstNode 对应)
enum OpType { OP_ADD=0; OP_SUB=1; OP_MUL=2; OP_NEGATE=3; OP_CONST=4; OP_VAR=5; OP_DIV_CONST=6; }

// 新增 AstNodeProto 递归消息
message AstNodeProto { OpType op=1; double const_value=2; int32 var_party_id=3; AstNodeProto lhs=4; AstNodeProto rhs=5; }

// ComputationRequest 扩展 (保留向下兼容)
message ComputationRequest { ..., AstNodeProto expression=10; repeated int32 ciphertext_vars=11; }

// CiphertextChunk 扩展
message CiphertextChunk { ..., int32 var_index=10; }
```

---

## 测试方法

### 编译

```bash
cd distributed-system
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/usr/local/lib/OpenFHE
make test_ast_serializer -j$(nproc)
```

### 运行测试

```bash
./build/test_ast_serializer
```

### 测试用例

| # | 内容 | 结果 |
|---|------|:---:|
| 1 | `(A+B)*C` 序列化往返 | ✅ |
| 2 | 5层嵌套 `((-A-B+5)*C)/2` 往返 | ✅ |
| 3 | 叶子节点 CONST、VAR、DIV_CONST | ✅ |

---

## 验收标准

- [x] `ast_common.h` 所有工厂方法可正常构造 AST
- [x] Proto 文件通过 `protoc` 编译，无报错
- [x] `AstSerializer::serialize()` / `deserialize()` 往返后 AST 完全一致
- [x] 三个测试用例全部通过
- [x] 接口契约无变更

## Phase 2 待办

- [ ] 修复 proto 与 WSL gRPC 版本兼容性（`protoc 3.12.4` → 需升级或重新生成）
- [ ] gRPC 端到端测试：验证 AstNodeProto 通过 gRPC 传输
- [ ] 支持 4+ 参与方的 proto 扩展
- [ ] 门限可配置（t-out-of-n）的消息类型
- [ ] Proto 消息版本号管理
