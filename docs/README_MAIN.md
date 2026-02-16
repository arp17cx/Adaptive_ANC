# 自适应主动降噪(ANC)系统 - 跨平台版本

## 🎯 选择你的平台

### Windows用户 (推荐看这个)
👉 **请阅读**: `README_WINDOWS.md`  
👉 **安装指南**: `INSTALLATION_GUIDE.md`  
👉 **一键运行**: 双击 `run.bat`

### Linux/Unix用户
👉 **请阅读**: 本文件  
👉 **编译**: `make`  
👉 **运行**: `./anc_system`

---

## 🚀 Windows快速开始 (傻瓜式)

### 第一步: 安装gcc编译器
1. 下载 TDM-GCC: https://jmeubank.github.io/tdm-gcc/
2. 安装时勾选 "Add to PATH"
3. 重启命令行

### 第二步: 运行程序
**双击 `run.bat`** - 就这么简单！

程序会自动:
- 检测编译器
- 编译所有代码
- 运行程序
- 生成输出文件

### 输出文件
- `anc_log.txt` - 完整日志
- `output_comparison.wav` - 2通道对比音频

### 可选输入文件
放在程序目录即可使用:
- `input_4ch.wav` - 4通道WAV文件
- `secondary_path.bin` - 次级路径 (4096个float32)

不提供这些文件时，程序使用模拟信号运行。

---

## 🐧 Linux快速开始

### 编译
```bash
make
```

### 运行
```bash
./anc_system
```

### 清理
```bash
make clean
```

---

## 📁 项目结构

```
ANC_System/
├── Windows脚本 (推荐Windows用户使用)
│   ├── run.bat                 ← 一键编译+运行
│   ├── build.bat               ← 编译
│   └── clean.bat               ← 清理
│
├── Linux/Unix
│   └── Makefile                ← make编译
│
├── 核心源代码
│   ├── main.c                  - 主程序
│   ├── config.h                - 配置
│   ├── coeffs.h                - 参数
│   ├── wav_io.c/.h             - WAV I/O
│   ├── fir_filter.c/.h         - FIR滤波
│   ├── time_domain_sim.c/.h    - 时域仿真
│   └── logger.c/.h             - 日志
│
├── 文档
│   ├── README.md               ← 你正在看的文件
│   ├── README_WINDOWS.md       - Windows详细说明
│   ├── INSTALLATION_GUIDE.md   - Windows安装指南
│   └── IMPLEMENTATION_SUMMARY.md - 实现总结
│
└── 可选输入 (运行时自动检测)
    ├── input_4ch.wav           (可选)
    └── secondary_path.bin      (可选)
```

---

## 🎓 算法特性

### 1. 依次梯度下降 (DSP友好)
每次只优化一个参数 (gain/Q/fc)，降低算力需求

### 2. 时序因果性
```
阶段1 (0-100ms):
  输入: 原始FF + 原始FB
  → 计算参数v1
  → 用v1滤波100ms后的所有剩余信号

阶段2 (100-200ms):
  输入: 原始FF + 降噪后FB
  → 计算参数v2
  → 用v2滤波200ms后的所有剩余信号
  
依此类推...
```

### 3. 稳定性检测
- 平滑度检测
- 尖峰检测
- 界限检测
- 整体偏移检测

### 4. 完整闭环仿真
```
原始FF → Biquad(10级) → 总增益 → 次级路径FIR → 与FB相减 → 新FB
```

---

## ⚙️ 系统要求

### Windows
- Windows 7/8/10/11
- GCC编译器 (TDM-GCC 或 MinGW)
- 内存: 建议2GB以上

### Linux
- GCC 4.8+
- Make
- 标准C库

---

## 📊 配置参数

编辑 `coeffs.h`:

```c
// 文件路径
#define WAV_INPUT_PATH     "input_4ch.wav"
#define SP_IR_PATH         "secondary_path.bin"

// 通道映射
#define WAV_CH_FF          0  // 参考麦通道
#define WAV_CH_FB          1  // 误差麦通道

// 梯度下降参数
#define LEARNING_RATE_GAIN  0.1f
#define LEARNING_RATE_Q     0.01f
#define LEARNING_RATE_FC    10.0f
```

---

## 🐛 常见问题

### Windows: "gcc不是内部或外部命令"
**解决**: 安装TDM-GCC，确保勾选"Add to PATH"，重启电脑

### Linux: "command not found: make"
**解决**: `sudo apt-get install build-essential` (Ubuntu/Debian)

### 编译错误: "No such file"
**解决**: 确保所有.c和.h文件在同一目录

### WAV文件加载失败
**解决**: 
- 确保是PCM格式 (不是MP3)
- 通道数≥2
- 文件名正确

---

## 📚 详细文档

- **Windows用户**: 查看 `README_WINDOWS.md` 和 `INSTALLATION_GUIDE.md`
- **算法实现**: 查看 `IMPLEMENTATION_SUMMARY.md`
- **技术细节**: 查看源代码注释

---

## 🎉 开始使用

### Windows
1. 安装gcc (TDM-GCC)
2. 双击 `run.bat`
3. 完成！

### Linux
1. `make`
2. `./anc_system`
3. 完成！

---

**版本**: 1.0 跨平台  
**支持平台**: Windows 7/8/10/11, Linux, Unix  
**编译器**: GCC  
**语言**: C (标准C99)
