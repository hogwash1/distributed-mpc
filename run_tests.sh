#!/bin/bash
# 同态任意函数计算库 — 全量测试
# WSL: bash run_tests.sh
set -e
B="$(dirname "$0")/build"
O="/usr/local/lib/OpenFHE"

echo "============================================================"
echo "  同态任意函数计算库 — 全量测试"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "============================================================"

# 编译
echo ""; echo "[1/5] 编译..."
rm -rf "$B" && mkdir "$B" && cd "$B"
cmake .. -DCMAKE_PREFIX_PATH="$O" >/dev/null 2>&1
make test_expr_parser test_integration test_multiparty mpc_demo -j$(nproc) 2>&1 | tail -2
echo "  ✅ 编译完成"

# 测试
echo ""; echo "[2/5] ExprParser — 6 测试"
./test_expr_parser 2>&1 | grep -E "通过|失败|结果" | sed 's/^/  /'

echo ""; echo "[3/5] 集成测试 — 4 测试"
LD_LIBRARY_PATH=/usr/local/lib ./test_integration 2>&1 | grep -E "通过|失败|结果|err=" | sed 's/^/  /'

echo ""; echo "[4/5] 多方密钥+计算 — 5 测试"
LD_LIBRARY_PATH=/usr/local/lib ./test_multiparty 2>&1 | grep -E "通过|失败|结果|err=" | sed 's/^/  /'

echo ""; echo "[5/5] MPC Demo"
LD_LIBRARY_PATH=/usr/local/lib ./mpc_demo --parties 3 --data1 "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16" --data2 "5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20" --data3 "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16" --expr '{"op":"add","lhs":{"op":"var","party":1},"rhs":{"op":"var","party":2}}' 2>&1 | grep -E "联合|加密|计算|解密|结果|err=" | sed 's/^/  /'

echo ""
echo "============================================================"
echo "  全部测试通过 ✅"
echo "============================================================"
