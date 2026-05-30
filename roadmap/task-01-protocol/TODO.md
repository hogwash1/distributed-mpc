# Task 01 — TODO 清单

## Phase 1 ✅ 已完成

- [x] 阅读 roadmap/README.md 第四节接口契约
- [x] 创建 `ast_common.h`（OpType + AstNode + to_string + clone）
- [x] 修改 `distributed_mpc.proto`（OpType 枚举 + AstNodeProto + ComputationRequest/CiphertextChunk 扩展）
- [x] protoc 编译通过
- [x] 创建 `ast_serializer.h`（AstSerializer 类）
- [x] 3 个测试全部通过
- [x] 更新 CMakeLists.txt

## Phase 2 待办

- [ ] **P0**: 修复 proto 与 gRPC 版本兼容（`protoc 3.12.4` vs 系统 `libgrpc++`）
- [ ] **P0**: gRPC 端到端测试：Party 1 发送 AstNodeProto → compute_server 反序列化
- [ ] **P1**: 4+ 参与方 proto 扩展（current 硬编码 3 方）
- [ ] **P2**: t-out-of-n 门限配置消息
- [ ] **P2**: Proto 版本号语义化管理

## 协作接口

- **乙 (Task 2)**：`ast_common.h` 已交付 ✅
- **丙 (Task 3)**：`ast_common.h` 已交付 ✅
- **丁 (Task 4)**：`AstNodeProto` + `AstSerializer` 已交付 ✅
