# 自适应主动降噪(ANC)系统 - 规范化项目结构

## 📁 项目结构

```
ANC_Project/
├── src/                    源代码
│   ├── main.c              - 主程序
│   ├── wav_io.c            - WAV文件I/O
│   ├── fir_filter.c        - FIR滤波器
│   ├── time_domain_sim.c   - 时域仿真
│   └── logger.c            - 日志管理
│
├── inc/                    头文件
│   ├── config.h            - 系统配置
│   ├── coeffs.h            - 参数定义
│   ├── wav_io.h
│   ├── fir_filter.h
│   ├── time_domain_sim.h
│   └── logger.h
│
├── result/                 输出目录（自动创建）
│   ├── anc_log.txt         - 运行日志
│   └── output_comparison.wav - 对比音频
│
├── docs/                   文档
│   └── (说明文档)
│
├── build.bat               编译脚本
├── run.bat                 一键运行
├── clean.bat               清理
└── README.md               本文件
```

## 🚀 快速开始

### Windows用户

1. **安装gcc** (只需一次)
   - 下载 TDM-GCC: https://jmeubank.github.io/tdm-gcc/
   - 安装时勾选 "Add to PATH"

2. **运行程序**
   ```
   双击 run.bat
   ```

3. **查看结果**
   - 日志: `result/anc_log.txt`
   - 音频: `result/output_comparison.wav`

### 手动编译

```batch
build.bat      # 编译
anc_system.exe # 运行
```

## ⏱️ 时序说明 (重要!)

### 正确的迭代时序

```
Iteration 0: 0-325ms
  [Phase 1] DSP Processing
    - 0-100ms: 累积数据
    - 100-325ms: 10次FFT (75%滑窗)
    - 计算新参数
  
  [Phase 2] Time Domain Filtering
    - 用新参数滤波 325ms-end 的所有信号
    - 更新 simulated_fb[325ms-end]

Iteration 1: 325-650ms
  [Phase 1] DSP Processing
    - 325-425ms: 累积
    - 425-650ms: 10次FFT
    - 计算新参数
  
  [Phase 2] Time Domain Filtering
    - 用新参数滤波 650ms-end
    - 更新 simulated_fb[650ms-end]

依此类推...
```

### 关键点

- ✅ 每轮处理固定325ms数据
- ✅ DSP使用: 原始FF + 降噪后FB (上一轮结果)
- ✅ 滤波范围: 当前边界后的所有剩余信号
- ✅ 下一轮从当前边界开始

## 📊 输出说明

### 日志文件 (result/anc_log.txt)

包含完整的运行过程:
- 每次迭代的时间范围
- DSP处理详情
- 参数更新记录
- 时域滤波状态

### 音频文件 (result/output_comparison.wav)

2通道对比:
- 通道1: 原始参考麦信号
- 通道2: 降噪后的误差麦信号

## ⚙️ 可选输入文件

放在项目根目录:
- `input_4ch.wav` - 4通道WAV (可选)
- `secondary_path.bin` - 次级路径 (可选)

不提供时自动使用模拟信号。

## 🎯 算法特性

1. **DSP友好优化** - 依次优化参数
2. **严格时序** - 325ms一轮迭代
3. **完整仿真** - Biquad + FIR闭环
4. **稳定性检测** - 防止发散

## 🛠️ 修改配置

编辑 `inc/coeffs.h`:

```c
// 修改迭代时间
// (在main.c中: float iteration_time_ms = 325.0f)

// 修改输出路径
#define LOG_OUTPUT_PATH "result/anc_log.txt"
#define WAV_OUTPUT_PATH "result/output_comparison.wav"
```

## 📝 开发说明

### 编译选项

```batch
gcc -c src/xxx.c -o xxx.o -Iinc -Wall -O2
```

- `-Iinc`: 指定头文件目录
- `-Wall`: 显示所有警告
- `-O2`: 优化级别2

### 添加新模块

1. 源文件放在 `src/`
2. 头文件放在 `inc/`
3. 修改 `build.bat` 添加编译命令

## ❓ 常见问题

### Q: 为什么是325ms一轮?

A: 
- 每5msDMA的数据送给DSP累积100ms第一次进行FFT的buffer长度
- 10次FFT覆盖约0-325ms
- 暂不考虑处理时间，325ms认为DSP立刻可以算完所有步骤且完成参数ramp，开始时域滤波

### Q: result文件夹不存在?

A: build.bat会自动创建

### Q: 编译错误 "No such file"?

A: 确保所有源文件都在 src/ 和 inc/ 目录

---

**版本**: 2.0 规范化结构  
**日期**: 2026.02  
**平台**: Windows (GCC)
