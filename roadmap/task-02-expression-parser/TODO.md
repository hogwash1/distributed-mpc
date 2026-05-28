# Task 02 — TODO 清单

## Day 1

- [ ] 阅读 [roadmap/README.md](../README.md) 第四节接口契约
- [ ] 从 Task 1（甲）获取 `ast_common.h`，或按接口契约自行创建临时版本
- [ ] 决定 JSON 解析方案：
  - [ ] 方案 A：引入 `nlohmann/json.hpp`（推荐，单头文件）
  - [ ] 方案 B：手写极简 JSON 解析器（如果不想引入外部依赖）
- [ ] 创建 `distributed-system/expr_parser.h`，定义 `ExprParser` 类接口
- [ ] 实现 `parse_json()` 递归下降解析
- [ ] 实现 `to_json()` 反序列化

## Day 2

- [ ] 实现 `validate()` — AST 合法性校验
- [ ] 实现 `compute_depth()` — 乘法深度计算
- [ ] 实现 `collect_vars()` — 变量收集
- [ ] 创建 `distributed-system/test_expr_parser.cpp`
- [ ] 编写全部 6 个测试用例并通过
- [ ] 提交 PR

## 协作接口

- 通知 **丙**（Task 3）：`AstNode` 结构可正常工作，他可以开始开发 evaluator
- 通知 **丁**（Task 4）：JSON 格式规范可参考本文 README 中的表格