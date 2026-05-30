# Task 02 — 表达式解析器

> **负责人：乙 | Phase 1: ✅ 完成 | Phase 2: 待开始**  
> **提交：2026-05-30**

---

## Phase 1 完成的工作

实现 JSON 表达式解析器 `ExprParser`，将用户输入的 JSON 表达式字符串转换为 `AstNode` 抽象语法树，并提供 AST 分析工具。

### 产出文件

| 文件 | 说明 |
|------|------|
| `distributed-system/expr_parser.h` | ExprParser 类声明 + ExprParseError 异常 |
| `distributed-system/expr_parser.cpp` | 递归下降解析器实现（内置极简 JSON Scanner，零外部依赖） |
| `distributed-system/test_expr_parser.cpp` | 6个测试用例 |
| `distributed-system/CMakeLists.txt` | 新增 test_expr_parser 构建目标 |

### 支持的全部 7 种 OpType

| JSON `op` | AST 类型 | 示例 |
|-----------|---------|------|
| `"add"` | ADD | `{"op":"add","lhs":...,"rhs":...}` |
| `"sub"` | SUB | `{"op":"sub","lhs":...,"rhs":...}` |
| `"mul"` | MUL | `{"op":"mul","lhs":...,"rhs":...}` |
| `"negate"` | NEGATE | `{"op":"negate","lhs":...}` |
| `"const"` | CONST | `{"op":"const","value":3.14}` |
| `"var"` | VAR | `{"op":"var","party":1}` |
| `"div_const"` | DIV_CONST | `{"op":"div_const","lhs":...,"value":3.0}` |

同时兼容 `"const_value"` 字段名（roadmap 第四节格式）。

### API 接口

```cpp
// 解析 JSON → AST
auto ast = ExprParser::parse_json(json_string);

// AST → JSON (调试用)
std::string json = ExprParser::to_json(ast);

// 合法性校验 (party_id 范围)
std::string err = ExprParser::validate(ast, num_parties); // 空串=合法

// 乘法深度计算
int depth = ExprParser::compute_depth(ast);

// 变量收集 (去重排序)
auto vars = ExprParser::collect_vars(ast);
```

---

## 测试方法

### 编译

```bash
cd distributed-system
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/usr/local/lib/OpenFHE
make test_expr_parser -j$(nproc)
```

### 运行

```bash
./build/test_expr_parser
```

### 测试用例

| # | 测试 | 结果 |
|---|------|:---:|
| 1 | 简单两方加法 `data1+data2` | ✅ |
| 2 | 嵌套复杂表达式 `(data1*2.5+data2)/3.0` | ✅ |
| 3 | `to_json`→`parse_json` 往返一致性 | ✅ |
| 4 | 非法 party_id 校验 | ✅ |
| 5 | 乘法深度计算 (0/1/2层) | ✅ |
| 6 | 变量收集 (去重排序) | ✅ |

---

## 验收标准

- [x] 支持全部 7 种运算类型的解析
- [x] 非法 JSON 输入抛出 `ExprParseError` 并包含明确错误信息
- [x] `validate()` 捕获 party_id 超范围错误
- [x] `compute_depth()` 准确计算最大乘法嵌套层数
- [x] 所有 6 个测试用例通过

## Phase 2 待办

- [ ] 支持比较操作表达式语法（`gt`/`lt`/`eq`）
- [ ] 支持条件分支表达式（`if`/`then`/`else`）
- [ ] 表达式白名单过滤（防注入）
- [ ] 优化 JSON 解析性能（大表达式基准测试）
- [ ] 支持 YAML 输入格式
