# ASTC Block Codec Test - 配置与使用说明

## 目录
- [概述](#概述)
- [默认配置文件功能](#默认配置文件功能)
- [配置文件格式](#配置文件格式)
- [参数说明](#参数说明)
- [用法示例](#用法示例)
- [构建与运行](#构建与运行)
- [常见问题与排查](#常见问题与排查)
- [进阶用法与最佳实践](#进阶用法与最佳实践)

---

## 概述

本项目支持通过外部 INI 配置文件灵活定义 ASTC 压缩参数，便于批量测试、团队协作和参数复用。支持自动加载默认配置文件、命令行覆盖、批量处理等。

---

## 默认配置文件功能

- **无参数启动**：`./test_block_codec` 自动加载 `default_config.ini`
- **CMake 可定制路径**：通过 `DEFAULT_CONFIG_FILE_PATH` 宏或 CMake 变量自定义默认配置
- **找不到配置时**：自动显示帮助信息

**默认配置示例**：
```ini
[block]
width=4
height=4
mode=0
partition=0
[endpoints]
values=255,255,255,255,0,0,0,0
[weights]
count=16
[calculation]
decimation_mode=0
weight_quant=QUANT_4
is_dual_plane=false
```

---

## 配置文件格式

- **INI风格**，支持注释（# 或 ; 开头）
- **区块**：如 [block]、[endpoints]、[weights]、[calculation]
- **键值对**：key=value，自动去除空白

### 支持的区块与参数

#### [block]
- `width`/`height`：块尺寸
- `mode`：0=自动（用decimation_mode/weight_quant/is_dual_plane计算），>0=强制指定block mode
- `partition`：分区索引
- `has_alpha`：是否有alpha通道
- `is_dual_plane`：是否双平面

#### [endpoints]
- `values`：8个端点值（R1,G1,B1,A1,R2,G2,B2,A2）

#### [weights]
- `values`：权重值数组（可选）
- `count`：权重数量（自动补齐，默认128）

#### [calculation]
- `decimation_mode`：权重网格模式
- `weight_quant`：权重量化方式（字符串，如QUANT_4）
- `is_dual_plane`：是否双平面

---

## 参数说明

- `mode=0`：自动模式，需用decimation_mode、weight_quant、is_dual_plane等参数计算block_mode
- `mode>0`：强制指定block_mode
- `weight_quant` 可选值：QUANT_2, QUANT_3, QUANT_4, QUANT_5, QUANT_6, QUANT_8, QUANT_10, QUANT_12, QUANT_16, QUANT_20, QUANT_24, QUANT_32, QUANT_40, QUANT_48, QUANT_64, QUANT_80, QUANT_96, QUANT_128, QUANT_160, QUANT_192, QUANT_256
- 其它参数详见注释

---

## 用法示例

### 加载默认配置
```bash
./test_block_codec
```

### 加载自定义配置
```bash
./test_block_codec -f my_config.ini
```

### 命令行覆盖参数
```bash
./test_block_codec -b 6x6 -w 128,128,128,128,128,128,128,128
```

### 创建示例配置
```bash
./test_block_codec --create-config example.ini
```

---

## 构建与运行

### Windows
```cmd
build_test_only.bat
```

### Linux/macOS
```bash
chmod +x build_test_only.sh
./build_test_only.sh
```

### CMake 高级用法
```bash
cmake .. -DASTCENC_CLI=ON -DASTCENC_UNITTEST=OFF
cmake .. -DASTCENC_CLI=ON -DASTCENC_UNITTEST=OFF -DUSE_CUSTOM_DEFAULT_CONFIG=ON -DDEFAULT_CONFIG_PATH="/path/to/your/custom_config.ini"
```

---

## 常见问题与排查

- **文件找不到**：检查路径和文件名
- **参数无效**：检查区块、键名、数值范围
- **权重数量不符**：确保count与块尺寸和双平面设置一致
- **编码失败**：检查mode、decimation_mode、weight_quant等组合是否合法
- **编码格式问题**：配置文件请用UTF-8编码

---

## 进阶用法与最佳实践

- **批量处理**：
```bash
for config in configs/*.ini; do
    ./test_block_codec -f "$config"
done
```
- **版本管理**：将配置文件纳入git等版本控制
- **参数注释**：在配置文件中详细注释每个参数的用途
- **团队协作**：共享和复用标准配置

---

## 参考与扩展

- 支持HLSL preset参数的配置文件
- 支持命令行与配置文件混合覆盖
- 支持自定义权重/端点/分区等高级用法
- 未来可扩展JSON、变量替换、模板等 