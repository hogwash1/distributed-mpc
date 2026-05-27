# 两方门限同态加密演示

基于 OpenFHE CKKS 方案的单进程两方门限同态加密基础演示。

## 演示内容

1. **分布式密钥生成**：2 个参与方各自生成私钥分片
2. **联合公钥构造**：通过 `MultipartyKeyGen` 聚合
3. **加密与解密**：使用联合公钥加密，双方协作阈值解密
4. **同态运算验证**：验证同态加法和乘法的正确性

## 技术要点

- 两方门限：任意一方无法单独解密
- 支持 `EvalAdd` 和 `EvalMult` 同态运算
- 乘法深度 3，适用于多层同态计算

## 构建运行

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./threshold_demo
```

## 依赖

- CMake >= 3.25
- C++20
- OpenFHE >= 1.4.2