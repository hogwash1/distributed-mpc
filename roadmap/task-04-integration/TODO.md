# Task 04 — TODO 清单

## Day 1 — 理解与重构

- [ ] 阅读 [compute_server.cpp](file:///d:/桌面文件夹/TARE_SOLO工作目录/隐私计算demo/distributed-system/compute_server.cpp) 中 `SubmitCiphertext` 和 `TriggerComputation` 的完整实现
- [ ] 阅读 [party_client.cpp](file:///d:/桌面文件夹/TARE_SOLO工作目录/隐私计算demo/distributed-system/party_client.cpp) 中数据加密上传和部分解密的流程
- [ ] 确认 Task 1-3 的代码已合并到 dev 分支，拉取最新代码
- [ ] 重构 `compute_server.cpp` 的 `TriggerComputation`：
  - [ ] 检查 `has_expression()`，有则使用新流程
  - [ ] 反序列化 AST → 建立 var_map → 调用 HeEvaluator
  - [ ] 保留旧流程作为 else 分支
- [ ] 修改 `party_client.cpp`：
  - [ ] 新增 `--expr` 命令行参数（从 JSON 文件或字符串读取）
  - [ ] 在触发计算时将表达式序列化发送
  - [ ] 上传密文时标注 var_index

## Day 2 — 测试与修复

- [ ] 更新 `CMakeLists.txt`，添加新源文件和 include
- [ ] 编译通过，无警告
- [ ] 手动测试用例 1：两方加法 `data₁ + data₂`
- [ ] 手动测试用例 2：三方平均值 `(A+B+C)/3`
- [ ] 手动测试用例 3：混合运算 `A×B + C`
- [ ] 手动测试用例 4：向下兼容（旧版 `computation_type = "average"`）
- [ ] 错误场景测试：非法 party_id、缺失变量、深度超限
- [ ] 修复所有问题后提交 PR

## 协作接口

- 作为最终集成的负责人，需要在 Task 1-3 完成后合并代码并解决冲突
- 如果 Task 1-3 的接口有变化，需要通知对应负责人调整