# 自适应主动降噪(ANC)系统 - 完整实现

## 📋 项目概述

本项目实现了完整的自适应主动降噪算法,包含:
- 频域自适应算法(FxLMS变体)
- DSP友好的依次梯度下降参数优化
- 时域闭环仿真(Biquad级联 + 次级路径FIR)
- 稳定性检测
- WAV文件I/O
- 完整日志记录

## 🏗️ 架构

### 核心模块

```
main.c              - 主程序和算法流程
config.h            - 系统配置和数据结构
coeffs.h            - 预制参数和算法参数
wav_io.c/.h         - WAV文件读写
fir_filter.c/.h     - FIR滤波器
time_domain_sim.c/.h - 时域闭环仿真
logger.c/.h         - 日志管理
```

### 状态机流程

```
SIGNAL_PROCESS (FFT分析 + PP主路径传函)
    ↓
CAL_MU (计算步长)
    ↓
CAL_FF_RESPONSE (计算当前滤波器频响)
    ↓
CAL_TARGET_FF (计算目标频响)
    ↓
STABLE_CHECK (稳定性检测)
    ↓
CAL_FF_INIT_LOSS (计算基准loss)
    ↓
UPDATE_EQ_PARAMS (依次梯度下降优化)
    ↓
UPDATE_FILTER_COEFFS (更新滤波器系数)
    ↓
【时域滤波仿真】
    ↓
回到 SIGNAL_PROCESS (使用降噪后的信号)
```

## 🚀 快速开始

### 编译

```bash
make
```

### 运行

#### 方式1: 使用模拟信号(无需输入文件)

```bash
./anc_system
```

程序会自动生成模拟的参考麦和误差麦信号。

#### 方式2: 使用真实录音

准备文件:
1. `input_4ch.wav` - 4通道WAV文件
   - 通道0: 参考麦(FF)
   - 通道1: 误差麦(FB)
   - 采样率: 建议375kHz(可配置)

2. `secondary_path.bin` (可选) - 次级路径冲击响应
   - 格式: 4096个float32值
   - 如不提供,使用默认模拟

放置文件后运行:
```bash
./anc_system
```

### 输出文件

- `anc_log.txt` - 完整运行日志
- `output_comparison.wav` - 2通道对比
  - 通道1: 原始参考麦信号
  - 通道2: 降噪后的误差麦信号

## ⚙️ 配置

编辑 `coeffs.h` 修改参数:

```c
// 文件路径
#define WAV_INPUT_PATH     "input_4ch.wav"
#define SP_IR_PATH         "secondary_path.bin"
#define LOG_OUTPUT_PATH    "anc_log.txt"
#define WAV_OUTPUT_PATH    "output_comparison.wav"

// 通道映射
#define WAV_CH_FF          0  // 参考麦通道
#define WAV_CH_FB          1  // 误差麦通道

// 梯度下降参数
#define LEARNING_RATE_GAIN  0.1f
#define LEARNING_RATE_Q     0.01f
#define LEARNING_RATE_FC    10.0f
```

## 🔧 算法特性

### 1. DSP友好优化

- **依次梯度下降**: 每次只优化一个参数(gain/Q/fc)
- 降低算力需求
- 适合实时DSP运行

### 2. 时序因果性保证

```
时刻t: DSP计算新参数
    ↓
时刻t+: 对未来信号进行时域滤波
    ↓
时刻t++: 滤波后的信号反馈给DSP
```

严格保证因果性,符合实际硬件约束。

### 3. 稳定性检测

检测目标频响的4个指标:
- 平滑度(二阶差分)
- 局部尖峰
- 绝对幅度界限
- 整体偏移

异常时跳过更新,避免系统不稳定。

### 4. 时域闭环仿真

完整模拟硬件闭环:
```
参考麦 → Biquad(10级) → 次级路径FIR → 与误差麦相减 → 新的误差麦
```

## 📊 预制参数

`coeffs.h` 中包含10套预制参数:
- 10个Biquad滤波器(1 Lowshelf + 8 Peaking + 1 Highshelf)
- 次级路径频响(1025个频点)

可根据实测数据修改。

## 🛠️ 开发

### 添加新的预制参数集

编辑 `coeffs.h`:

```c
const EQPreset eq_presets[NUM_PRESET_SETS] = {
    // 新增 Set 10
    {
        .biquads = {
            {BIQUAD_LOWSHELF, gain_dB, q, fc},
            // ...
        },
        .total_gain_dB = 0.0f
    }
};
```

### 调整FFT参数

编辑 `config.h`:

```c
#define FFT_LENGTH          2048  // FFT长度
#define NUM_FFT_AVERAGE     10    // 平均次数
```

## 📝 日志格式

日志包含:
- 每次迭代的详细过程
- FFT结果
- 参数更新记录
- 稳定性检测结果
- 时域滤波状态

## ⚠️ 注意事项

1. **内存需求**: 长音频文件会占用大量内存
2. **算力**: 梯度下降需要多次频响计算
3. **采样率**: 确保WAV文件采样率与配置匹配
4. **文件格式**: WAV必须是PCM格式(16bit或32bit)

## 🐛 故障排除

### 问题: 编译错误

```bash
make clean
make
```

### 问题: WAV文件加载失败

检查:
- 文件路径是否正确
- 文件格式是否为PCM
- 通道数是否≥2

### 问题: 次级路径文件加载失败

程序会使用默认模拟的次级路径,可继续运行。

## 📚 参考文献

- FxLMS算法
- Biquad滤波器设计
- 自适应滤波器理论

## 📄 许可

本项目代码仅供学习和研究使用。

## 👥 贡献

欢迎提交Issue和Pull Request。

---

**版本**: 1.0  
**更新日期**: 2024
