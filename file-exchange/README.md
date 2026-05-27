# File Exchange - 基于文件交换的三方安全计算

基于文件信号同步的三方门限同态加密演示，不依赖 gRPC 网络通信，通过共享文件系统（`/tmp/mpc_exchange/`）进行密钥交换和数据同步。

## 演示内容

1. **三方密钥生成**：A → B → C 链式聚合生成联合公钥
2. **文件同步**：通过信号文件（`.ready` 标记）实现三方协调
3. **加密计算**：各方使用联合公钥加密数据，写入共享目录
4. **三方解密**：读取其他方的部分解密结果，完成最终解密融合

## 文件说明

| 文件 | 说明 |
|------|------|
| `party_a_file.cpp` | Party A - 协调方，负责密钥生成发起和最终解密融合 |
| `party_b_file.cpp` | Party B - 参与方，执行 `MultipartyKeyGen` 和部分解密 |
| `party_c_file.cpp` | Party C - 参与方，执行 `MultipartyKeyGen` 和部分解密 |
| `CMakeLists.txt` | 构建配置（需 OpenFHE） |

## 与 gRPC 版本的对比

| 特性 | 文件交换版 | gRPC 分布式版 |
|------|-----------|-------------|
| 通信方式 | 共享文件系统 | gRPC 网络通信 |
| 同步机制 | 信号文件轮询 | RPC 阻塞等待 |
| 适用场景 | 单机多进程测试 | 真实网络部署 |
| 复杂度 | 低 | 高 |

## 构建运行

### 1. 编译

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 2. 运行（需 3 个终端，按顺序启动）

```bash
# 终端 1: Party A（协调方）
./party_a_file

# 终端 2: Party B
./party_b_file

# 终端 3: Party C
./party_c_file
```

> **注意**：确保 `/tmp/mpc_exchange/` 目录可读写，三方需要在同一台机器上运行。

## 依赖

- CMake >= 3.25
- C++20
- OpenFHE >= 1.4.2
- C++17 `std::filesystem`