# Task 04 — 服务端集成与联调

> **负责人：丁（技术负责人）**
> **预计工时：2 天**
> **依赖：Task 1（协议）、Task 2（解析器）、Task 3（求值器）全部完成**

---

## 任务目标

将 Task 1-3 的三个模块集成到 `compute_server.cpp` 和 `party_client.cpp` 中，实现端到端的"任意表达式安全计算"流程。

## 当前状态（需要理解后再动手）

- [compute_server.cpp](file:///d:/桌面文件夹/TARE_SOLO工作目录/隐私计算demo/distributed-system/compute_server.cpp) 中 `TriggerComputation` 硬编码了平均值计算
- 客户端上传数据时没有标识密文对应哪个变量
- 没有表达式传输机制
- 解密流程不变（P2P 部分解密 + 融合），可以复用

## 需要完成的工作

### 4.1 理解现状

首先阅读并理解以下关键代码：

| 文件 | 关键函数 | 行号附近 |
|------|---------|---------|
| `compute_server.cpp` | `SubmitCiphertext` | 约 L180 |
| `compute_server.cpp` | `TriggerComputation` | 约 L340 |
| `party_client.cpp` | `RunProtocol` / 数据加密上传 | 约 L200 |
| `party_client.cpp` | `PerformPartialDecrypt` | 约 L378 |

### 4.2 重构服务端 TriggerComputation

**修改文件**：`distributed-system/compute_server.cpp`

将硬编码的 `EvalAdd` 循环替换为：

```cpp
// 原代码:
// for (...) { result = cc->EvalAdd(result, ct); }

// 新代码:
#include "he_evaluator.h"
#include "ast_serializer.h"

Status ComputeServerGrpcService::TriggerComputation(...) {
    // 1. 检查 expression 字段是否存在
    //    - 有: 使用新流程（AST 求值）
    //    - 无: 回退到旧流程（向下兼容 average/sum）
    
    if (request->has_expression()) {
        // 2. 反序列化表达式
        auto ast = AstSerializer::deserialize(request->expression());
        
        // 3. 按 ciphertext_vars 建立 party_id → Ciphertext 映射
        std::map<int32_t, Ciphertext<DCRTPoly>> var_map;
        for (int i = 0; i < request->ciphertext_vars_size(); i++) {
            int32_t var_id = request->ciphertext_vars(i);
            var_map[var_id] = state_->ciphertexts[i];  // 按索引对应
        }
        
        // 4. 执行同态求值
        HeEvaluator evaluator(state_->cc, state_->pk, var_map);
        state_->result = evaluator.evaluate(ast);
        
        LOG("Computation done. Ops: " << evaluator.get_op_count()
            << ", Depth: " << evaluator.get_depth_consumed());
    } else {
        // 旧的 average/sum 逻辑（保留兼容）
        // ... 原有代码 ...
    }
    
    state_->computation_done = true;
    state_->cv.notify_all();
    return Status::OK;
}
```

### 4.3 修改客户端数据上传

**修改文件**：`distributed-system/party_client.cpp`

增加以下能力：

1. **支持命令行传入 JSON 表达式**（新增 `--expr` 参数或从文件读取）
2. **发送表达式到服务器**：在注册或触发计算时通过 `ComputationRequest.expression` 发送
3. **标注密文对应的变量 ID**：通过 `SubmitCiphertextRequest.var_index` 告诉服务器此密文对应 AST 中的哪个 VAR

命令行示例：
```bash
./party_client --id 1 --name PartyA \
  --data "10,20,30" \
  --expr '{"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}}'
```

### 4.4 端到端联调测试

**文件**：`distributed-system/test_e2e_expression.cpp`（手动测试脚本）

手动测试流程（需 4 个终端）：

```bash
# 终端 1: 服务器
./build/compute_server --parties 3

# 终端 2: Party 1
./build/party_client --id 1 --name A --data "1,2,3" \
  --expr '{"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"mul","lhs":{"op":"var","party":2},"rhs":{"op":"const","value":2.0}}}'

# 终端 3: Party 2
./build/party_client --id 2 --name B --data "4,5,6"

# 终端 4: Party 3
./build/party_client --id 3 --name C --data "7,8,9"
```

测试用例：
1. `data₁ + data₂` — 基础两方加法
2. `(data₁ + data₂ + data₃) / 3` — 三方平均值（验证与旧系统结果一致）
3. `data₁ × data₂ + data₃` — 混合运算
4. 只发旧版 `computation_type = "average"` — 验证向下兼容

### 4.5 更新 CMakeLists.txt

添加新的源文件和头文件到编译目标：

```cmake
add_executable(compute_server
    compute_server.h
    compute_server.cpp
    compute_server_main.cpp
    ast_common.h           # ★新增
    ast_serializer.h       # ★新增
    he_evaluator.h         # ★新增
    he_evaluator.cpp       # ★新增
    ${PROTO_SRCS}
)
```

### 4.6 错误处理

确保以下场景有合理的错误响应：
- 表达式中的 party_id 超出范围 → 返回 `INVALID_ARGUMENT`
- 某个变量对应的密文未上传 → 返回 `FAILED_PRECONDITION`
- AST 深度超过 multDepth → 在计算前返回错误，不要在计算中途崩溃

## 产出清单

| 文件 | 状态 |
|------|:----:|
| `distributed-system/compute_server.cpp` | 修改 |
| `distributed-system/party_client.cpp` | 修改 |
| `distributed-system/party_client_main.cpp` | 修改（新增 --expr 参数） |
| `distributed-system/CMakeLists.txt` | 修改 |

## 验收标准

- [ ] 旧版 `computation_type = "average"` 仍然正常工作（向下兼容）
- [ ] 新版 JSON 表达式 `(A+B+C)/3` 计算结果与旧版一致
- [ ] 支持不包含所有参与方的表达式（如只用两方）
- [ ] 错误表达式返回明确的 gRPC 错误码
- [ ] 操作日志清晰显示运算类型和次数
- [ ] 端到端手动测试全部通过