# Task 01 — TODO 清单

## Day 1

- [ ] 阅读 [roadmap/README.md](../README.md) 第四节接口契约
- [ ] 创建 `distributed-system/ast_common.h`，实现 OpType 枚举 + AstNode + 工厂方法 + to_string + clone
- [ ] 修改 `distributed-system/distributed_mpc.proto`：
  - [ ] 新增 OpType 枚举
  - [ ] 新增 AstNodeProto 递归消息
  - [ ] 扩展 ComputationRequest（加 expression + ciphertext_vars 字段）
  - [ ] 扩展 SubmitCiphertextRequest（加 var_index 字段）
- [ ] 运行 `protoc` 重新生成 proto 代码，确认编译通过

## Day 2

- [ ] 创建 `distributed-system/ast_serializer.h`，实现 AstSerializer 类
- [ ] 创建 `distributed-system/test_ast_serializer.cpp`
- [ ] 编写 3 个测试用例，全部通过
- [ ] 更新 `CMakeLists.txt` 确保新文件被编译
- [ ] 提交 PR，在 description 中标注接口契约无变更

## 协作接口

- 将 `ast_common.h` 提交后通知 **乙**（Task 2）和 **丙**（Task 3），他们需要引用此头文件
- proto 消息 `AstNodeProto` 定义后通知 **丁**（Task 4），这是 gRPC 传输格式