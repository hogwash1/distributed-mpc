# Task 03 — TODO 清单

## Day 1

- [ ] 阅读 [roadmap/README.md](../README.md) 第四节接口契约
- [ ] 从 Task 1（甲）获取 `ast_common.h`，或按接口契约自行创建临时版本
- [ ] 创建 `distributed-system/he_evaluator.h`，定义 `HeEvaluator` 类
- [ ] 实现 `eval_node()` 递归求值函数：
  - [ ] ADD / SUB / MUL / NEGATE 使用对应的 OpenFHE 方法
  - [ ] CONST 使用明文运算优化策略
  - [ ] VAR 直接从 vars_ map 获取
  - [ ] DIV_CONST 使用乘以倒数实现
- [ ] 实现操作日志输出

## Day 2

- [ ] 创建 `distributed-system/test_he_evaluator.cpp`
- [ ] 编写测试用例 1：两方加法
- [ ] 编写测试用例 2：三方平均值
- [ ] 编写测试用例 3：多项式 data₁² + data₁·2 + 1
- [ ] 编写测试用例 4：深度追踪验证
- [ ] 编写测试用例 5：缺失变量异常处理
- [ ] 确保所有测试通过
- [ ] 提交 PR

## 协作接口

- 完成后通知 **丁**（Task 4）：提供 `HeEvaluator` 的完整接口和使用示例
- 特别是 `evaluate()` 方法的签名和 `vars` map 的 key/value 含义