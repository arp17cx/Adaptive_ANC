# 自适应ANC算法 - 待实现功能清单

## ✅ 已完成

1. **依次梯度下降优化** (DSP友好)
   - 每个Biquad的gain, Q, fc按顺序逐个优化
   - 每次只优化一个参数,降低算力需求
   - 只有loss降低才接受更新

## ⏳ 待实现

### 2. WAV音频文件导入
**需求:**
- 导入4通道WAV文件
- 第N通道: 主路径参考麦信号 (FF)
- 第M通道: 主路径误差麦信号 (FB)
- 替换现有的模拟生成信号

**实现要点:**
- 添加WAV文件解析器 (读取header, data chunk)
- 支持多通道interleaved格式
- 可配置通道映射 (N, M的值)

### 3. 次级路径冲击响应导入
**需求:**
- 导入4096阶时域冲击响应 (binary文件)
- 用于时域滤波模拟

**实现要点:**
- 读取.bin文件 (4096个float32值)
- 存储为FIR滤波器系数
- 支持FIR卷积运算

### 4. 时域滤波闭环仿真
**需求:**
完整的硬件闭环仿真流程:

```
原始参考麦(FF) 
    ↓
Biquad滤波 (下参后的10级级联)
    ↓
次级路径FIR滤波 (4096阶)
    ↓
与误差麦(FB)相加/相减 (模拟主动降噪)
    ↓
新的误差麦信号
    ↓
送回算法进行下一轮迭代
```

**关键时序:**
- 从"准备开始平均10次FFT"的时刻开始
- 使用剩余的高采样率参考麦信号
- 每次迭代更新滤波器系数后,重新进行时域滤波

### 5. 日志和输出
**需求:**
- 所有printf输出保存到txt文件
- 保存2通道WAV文件:
  - 通道1: 原始参考麦信号
  - 通道2: 对应时刻降噪后的误差麦信号
  - 采样率: 原始高采样率 (375kHz)

## 📋 实现建议

### 方案A: 模块化 (推荐)
创建多个辅助文件:
- `wav_io.h/c`: WAV文件读写
- `fir_filter.h/c`: FIR滤波器
- `time_domain_sim.h/c`: 时域闭环仿真
- `logger.h/c`: 日志管理

### 方案B: 单文件
所有功能添加到main.c中
- 优点: 简单,单文件编译
- 缺点: 文件过大 (可能>3000行)

## 🔧 技术细节

### WAV文件格式
```c
typedef struct {
    char chunk_id[4];        // "RIFF"
    uint32_t chunk_size;
    char format[4];          // "WAVE"
    char subchunk1_id[4];    // "fmt "
    uint32_t subchunk1_size;
    uint16_t audio_format;   // 1 = PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char subchunk2_id[4];    // "data"
    uint32_t subchunk2_size;
} WavHeader;
```

### FIR滤波
```c
// 卷积: y[n] = Σ h[k] * x[n-k]
float fir_filter(float *input, float *h, int h_len, int n);
```

### 时域仿真核心循环
```c
for (iteration = 0; iteration < max_iterations; iteration++) {
    // 1. FFT分析 + 参数优化
    run_adaptive_algorithm();
    
    // 2. 时域滤波仿真
    for (sample = 0; sample < samples_per_iteration; sample++) {
        // Biquad级联滤波
        float filtered = apply_biquad_cascade(ff[sample]);
        
        // 次级路径FIR滤波
        float anti_noise = fir_filter(filtered, sp_ir, 4096);
        
        // 主动降噪
        float new_fb = fb[sample] - anti_noise;
        
        // 更新误差麦信号
        fb[sample] = new_fb;
    }
}
```

## ⚙️ 配置参数建议

```c
// config.h 新增
#define WAV_INPUT_PATH          "input_4ch.wav"
#define SP_IR_PATH              "secondary_path_4096.bin"
#define LOG_OUTPUT_PATH         "anc_log.txt"
#define WAV_OUTPUT_PATH         "output_comparison.wav"

#define WAV_CH_FF               0  // 参考麦通道索引
#define WAV_CH_FB               1  // 误差麦通道索引

#define SP_IR_LENGTH            4096
#define MAX_ITERATIONS          100
```
