# 自适应主动降噪(ANC)系统 - Windows版

## 🚀 快速开始 (傻瓜式一键运行)

### 方法1: 一键运行 (推荐)
1. 双击 `run.bat`
2. 等待自动编译和运行
3. 完成！

### 方法2: 手动编译
1. 双击 `build.bat` 编译
2. 双击 `anc_system.exe` 运行

### 清理文件
双击 `clean.bat` 清理所有编译和输出文件

## 📋 系统要求

### 必需软件
- **MinGW-w64** 或 **TDM-GCC** (提供gcc编译器)

### 下载安装GCC (如果还没有)
选择以下任一方式:

#### 选项1: TDM-GCC (推荐，安装简单)
1. 下载: https://jmeubank.github.io/tdm-gcc/
2. 安装时选择 "Add to PATH"
3. 重启命令行

#### 选项2: MinGW-w64
1. 下载: https://sourceforge.net/projects/mingw-w64/
2. 安装后添加到系统PATH
3. 重启命令行

### 验证安装
打开命令行输入:
```cmd
gcc --version
```
如果显示版本号，说明安装成功。

## 📁 文件说明

### 必需文件 (核心代码)
```
main.c              - 主程序
config.h            - 系统配置
coeffs.h            - 参数定义
wav_io.c/.h         - WAV文件读写
fir_filter.c/.h     - FIR滤波器
time_domain_sim.c/.h - 时域仿真
logger.c/.h         - 日志管理
```

### Windows脚本 (一键操作)
```
run.bat             - 一键编译+运行
build.bat           - 编译
clean.bat           - 清理
```

### 可选输入文件
```
input_4ch.wav       - 4通道WAV输入 (可选)
secondary_path.bin  - 次级路径 (可选)
```

## 💡 使用说明

### 运行方式

#### 1. 使用模拟信号 (无需输入文件)
直接双击 `run.bat`

程序会自动生成模拟的参考麦和误差麦信号。

#### 2. 使用真实录音
准备文件并放在程序目录:
- `input_4ch.wav` - 4通道WAV
  - 通道0: 参考麦(FF)
  - 通道1: 误差麦(FB)
  - 采样率: 建议375kHz

- `secondary_path.bin` (可选)
  - 4096个float32值
  - 次级路径冲击响应

放置好后双击 `run.bat`

### 输出文件
运行完成后生成:
- `anc_log.txt` - 完整运行日志
- `output_comparison.wav` - 2通道对比
  - 通道1: 原始参考麦
  - 通道2: 降噪后误差麦

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

## 🐛 常见问题

### Q: 双击run.bat后提示 "gcc不是内部或外部命令"
**A**: 需要安装gcc编译器
- 下载安装 TDM-GCC (推荐)
- 确保安装时勾选 "Add to PATH"
- 重启命令行/电脑

### Q: 编译报错 "undefined reference to..."
**A**: 确保所有.c和.h文件都在同一目录

### Q: 运行后没有输出文件
**A**: 检查anc_log.txt查看错误信息

### Q: WAV文件加载失败
**A**: 
- 确保文件名正确
- 确保是PCM格式 (不是MP3等压缩格式)
- 通道数≥2

## 🎯 算法特性

### 1. DSP友好优化
- 依次优化每个参数 (gain/Q/fc)
- 降低算力需求

### 2. 时序因果性
```
0-100ms:   原始FF+FB → DSP → 参数v1 → 滤波100ms后的所有信号
100-200ms: 原始FF+降噪FB → DSP → 参数v2 → 滤波200ms后的所有信号
...
```

### 3. 稳定性检测
- 平滑度检测
- 尖峰检测
- 界限检测
- 整体偏移检测

### 4. 完整日志
所有过程记录在 `anc_log.txt`

## 📊 性能

- 处理速度: 取决于CPU (非实时，后处理)
- 内存占用: 约150MB (10秒音频)
- 精度: float32

## 🎓 技术文档

详细算法说明请查看:
- `IMPLEMENTATION_SUMMARY.md` - 实现总结
- `README.md` - 完整文档

## 📞 支持

遇到问题请检查:
1. gcc是否正确安装
2. 所有源文件是否完整
3. anc_log.txt中的错误信息

---

**版本**: 1.0 Windows  
**编译器**: GCC (MinGW/TDM-GCC)  
**平台**: Windows 7/8/10/11
