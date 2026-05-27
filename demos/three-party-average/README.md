# 三方门限同态加密求平均值演示

基于 OpenFHE CKKS 方案的单进程三方门限同态加密演示。

## 演示内容

1. **三方分布式密钥生成**：Party A / B / C 各自生成私钥分片
2. **联合公钥构造**：通过 `MultipartyKeyGen` 链式聚合生成联合公钥
3. **三方各自加密数据**：使用联合公钥分别加密本地数据
4. **同态计算**：计算 `平均值 = (Enc(A) + Enc(B) + Enc(C)) / 3`
5. **三方阈值解密**：三方各自执行 `MultipartyDecryptLead/Lag` 完成协作解密

## 技术要点

- 使用 CKKS 近似数方案，支持浮点数同态运算
- 联合私钥 `s = s₁ + s₂ + s₃`，任意单方无法独立解密
- 同态计算由任意一方执行，无需可信第三方

## 构建运行

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./three_party_average
```

## 依赖

- CMake >= 3.25
- C++20
- OpenFHE >= 1.4.2