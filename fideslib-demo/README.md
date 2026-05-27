# FIDESlib 同态加密加法演示

基于 FIDESlib（全同态加密硬件加速库）的 CKKS 加密加法演示。

## 演示内容

计算 `EvalAdd(Enc(2), Enc(3))`，验证同态加密加法的正确性。

## 技术要点

- 使用 FIDESlib 封装的 CKKS 方案
- 支持 GPU 加速：`SetDevices({0})` 指定设备
- 与 OpenFHE 原生 API 接口类似但封装更简洁
- `AutoLoad` 模式自动管理密文内存

## 构建运行

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./eval_add_demo
```

预期输出：解密结果为 `5.0`（即 `2 + 3`）。

## 与 OpenFHE 的对比

| 特性 | FIDESlib | OpenFHE |
|------|----------|---------|
| 硬件加速 | GPU 支持 | CPU 为主 |
| API 封装 | 高层封装 | 底层灵活 |
| 适用场景 | 生产部署 | 研究与开发 |

## 依赖

- CMake >= 3.25
- C++20
- FIDESlib（需预先安装）
- 可选：GPU 驱动（CUDA/OpenCL）