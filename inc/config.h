#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <complex.h>
#include <math.h>

// 前向声明，避免循环依赖
#ifndef NUM_BIQUADS
#define NUM_BIQUADS 10
#endif

// Biquad滤波器类型枚举（与coeffs.h保持一致）
#ifndef BIQUAD_TYPE_DEFINED
#define BIQUAD_TYPE_DEFINED
typedef enum {
    BIQUAD_LOWSHELF = 0,
    BIQUAD_PEAKING,
    BIQUAD_HIGHSHELF
} BiquadType;
#endif

// Biquad参数结构体（与coeffs.h保持一致）
#ifndef BIQUAD_PARAM_DEFINED
#define BIQUAD_PARAM_DEFINED
typedef struct {
    BiquadType type;
    float gain_dB;
    float q;
    float fc;
} BiquadParam;
#endif

// ============ 系统参数配置 ============
#define DSP_SAMPLE_RATE         32000      // DSP处理采样率（Hz）
#define REALTIME_SAMPLE_RATE    375000     // 实时滤波通路采样率（Hz）
#define FFT_LENGTH              2048       // FFT长度
#define FFT_HALF_LENGTH         1025       // FFT一半长度 + 1 (包含DC和Nyquist)

// 时间相关参数
#define PROCESS_INTERVAL_MS     5          // 处理间隔（ms）
#define ACCUMULATE_TIME_MS      100        // 累积时间（ms）
#define SAMPLES_PER_INTERVAL    ((DSP_SAMPLE_RATE * PROCESS_INTERVAL_MS) / 1000)  // 每次处理样本数：160
#define NUM_ACCUMULATE_FRAMES   (ACCUMULATE_TIME_MS / PROCESS_INTERVAL_MS)        // 累积帧数：20

// FFT参数
#define FFT_OVERLAP_RATIO       0.75f      // 75% overlap
#define FFT_HOP_SIZE            ((int)(FFT_LENGTH * (1.0f - FFT_OVERLAP_RATIO)))  // 512
#define NUM_FFT_AVERAGE         10         // FFT平均次数

// 抗混叠降采样参数
#define DECIMATION_FACTOR       ((REALTIME_SAMPLE_RATE) / (DSP_SAMPLE_RATE))  // 375000/32000 ≈ 11.71875

// 通道数
#define NUM_CHANNELS            3          // FF, FB, SPK三个通道

// ============ 复数结构体 ============
typedef struct {
    float real;
    float imag;
} Complex;

// ============ 时域Buffer结构体 ============
typedef struct {
    float data[FFT_LENGTH];
    int write_index;
    int sample_count;
} TimeBuffer;

// ============ 频域复数向量结构体 ============
typedef struct {
    Complex bins[FFT_HALF_LENGTH];  // 存储FFT结果（仅正频率部分）
} FreqResponse;

// ============ 多通道FFT累积结构体 ============
typedef struct {
    FreqResponse ff_accum;   // FF通道累积
    FreqResponse fb_accum;   // FB通道累积
    FreqResponse spk_accum;  // SPK通道累积
    
    // 主路径传函累积: PP = Sre/Srr (误差麦/参考麦)
    Complex pp_accum[FFT_HALF_LENGTH];  // 主路径传函累积
    
    int accum_count;         // 当前累积次数
} FFTAccumulator;

// ============ Biquad滤波器系数结构体 ============
typedef struct {
    float b0, b1, b2;  // 分子系数
    float a0, a1, a2;  // 分母系数（a0通常归一化为1）
} BiquadCoeffs;

// ============ Biquad滤波器状态结构体 ============
typedef struct {
    float x1, x2;  // 输入历史
    float y1, y2;  // 输出历史
} BiquadState;

// ============ 前馈滤波器组结构体 ============
typedef struct {
    BiquadCoeffs coeffs[10];   // 10个Biquad系数
    BiquadState states[10];     // 10个Biquad状态
    float total_gain;           // 总增益（线性）
} FeedforwardFilter;

// ============ EQ参数梯度结构体 ============
typedef struct {
    float gain_dB;
    float q;
    float fc;
} BiquadGradient;

// ============ EQ参数更新状态 ============
typedef struct {
    BiquadParam params[NUM_BIQUADS];  // 当前EQ参数
    float total_gain_dB;              // 当前总增益
    BiquadGradient gradients[NUM_BIQUADS];  // 各Biquad梯度
    float total_gain_gradient;        // 总增益梯度
    float init_loss;                  // 初始loss（当前FF参数与目标的拟合误差）
    float current_loss;               // 当前损失值
    int update_accepted;              // 更新是否被接受
} EQUpdateState;

// ============ 状态机枚举 ============
typedef enum {
    SIGNAL_PROCESS = 0,     // 信号处理：FFT分析，计算PP_AVERAGE
    CAL_MU,                 // 计算步长μ
    CAL_FF_RESPONSE,        // 计算当前前馈滤波器频响（先于target）
    CAL_TARGET_FF,          // 计算目标前馈响应（基于current_ff）
    STABLE_CHECK,           // 检测目标频响是否稳定/异常
    CAL_FF_INIT_LOSS,       // 计算初始loss作为更新阈值
    UPDATE_EQ_PARAMS,       // 更新EQ参数
    UPDATE_FILTER_COEFFS    // 更新滤波器系数到375kHz
} ProcessState;

// ============ 全局系统状态结构体 ============
typedef struct {
    ProcessState state;             // 当前状态
    
    // 时域缓冲区
    TimeBuffer ff_buffer;
    TimeBuffer fb_buffer;
    TimeBuffer spk_buffer;
    
    // FFT累积器
    FFTAccumulator fft_accum;
    
    // 平均后的频域数据
    FreqResponse ff_avg;
    FreqResponse fb_avg;
    FreqResponse spk_avg;
    
    // 平均后的主路径传函: PP_AVERAGE = Sre/Srr (误差麦/参考麦)
    Complex pp_average[FFT_HALF_LENGTH];
    
    // 次级路径模型（当前使用的预制集）
    Complex secondary_path[FFT_HALF_LENGTH];
    
    // 自适应参数
    float mu[FFT_HALF_LENGTH];              // 各频点步长
    Complex target_ff[FFT_HALF_LENGTH];     // 目标前馈响应
    Complex current_ff[FFT_HALF_LENGTH];    // 当前前馈滤波器响应
    Complex prev_target_ff[FFT_HALF_LENGTH]; // 上一次的目标响应（用于稳定性检测）
    
    // 稳定性检测
    float prev_smoothness;                  // 上一次的平滑度指标
    int target_valid;                       // 目标响应是否有效
    
    // 前馈滤波器
    FeedforwardFilter ff_filter;
    
    // EQ参数更新状态
    EQUpdateState eq_update;
    
    // 当前使用的预制集索引
    int current_preset_index;
    
    // 计数器
    int fft_count;              // FFT执行计数
    int frame_count;            // 帧计数器
    
} SystemState;

// ============ 工具函数声明 ============

// 复数运算
static inline Complex complex_add(Complex a, Complex b) {
    Complex result = {a.real + b.real, a.imag + b.imag};
    return result;
}

static inline Complex complex_sub(Complex a, Complex b) {
    Complex result = {a.real - b.real, a.imag - b.imag};
    return result;
}

static inline Complex complex_mul(Complex a, Complex b) {
    Complex result;
    result.real = a.real * b.real - a.imag * b.imag;
    result.imag = a.real * b.imag + a.imag * b.real;
    return result;
}

static inline Complex complex_div(Complex a, Complex b) {
    Complex result;
    float denom = b.real * b.real + b.imag * b.imag;
    if (denom > 1e-10f) {
        result.real = (a.real * b.real + a.imag * b.imag) / denom;
        result.imag = (a.imag * b.real - a.real * b.imag) / denom;
    } else {
        result.real = 0.0f;
        result.imag = 0.0f;
    }
    return result;
}

static inline float complex_mag(Complex a) {
    return sqrtf(a.real * a.real + a.imag * a.imag);
}

static inline Complex complex_conj(Complex a) {
    Complex result = {a.real, -a.imag};
    return result;
}

static inline Complex complex_scale(Complex a, float scale) {
    Complex result = {a.real * scale, a.imag * scale};
    return result;
}

#endif // CONFIG_H
