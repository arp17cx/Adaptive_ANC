#ifndef COEFFS_H
#define COEFFS_H

// 预制参数集数量
#define NUM_PRESET_SETS 10

// 频点数量（与FFT长度相关，取一半+1）
#define NUM_FREQ_POINTS 1025  // 2048/2 + 1

// Biquad滤波器级联数量
#define NUM_BIQUADS 10

// ============ 梯度下降参数配置 ============
// 学习率/步长
#define LEARNING_RATE_GAIN      0.1f     // Gain步长 (dB)
#define LEARNING_RATE_Q         0.01f    // Q步长
#define LEARNING_RATE_FC        10.0f    // 中心频率步长 (Hz)
#define LEARNING_RATE_TOTAL_GAIN 0.05f   // 总增益步长 (dB)

// 单次更新最大变化量限制
#define MAX_DELTA_GAIN          2.0f     // Gain最大变化量 (dB)
#define MAX_DELTA_Q             0.2f     // Q最大变化量
#define MAX_DELTA_FC            100.0f   // fc最大变化量 (Hz)
#define MAX_DELTA_TOTAL_GAIN    1.0f     // 总增益最大变化量 (dB)

// 参数范围限制
#define MIN_GAIN_DB            -20.0f    // 最小增益
#define MAX_GAIN_DB             20.0f    // 最大增益
#define MIN_Q                   0.3f     // 最小Q值
#define MAX_Q                   10.0f    // 最大Q值
#define MIN_FC                  20.0f    // 最小中心频率
#define MAX_FC                  20000.0f // 最大中心频率
#define MIN_TOTAL_GAIN_DB      -10.0f    // 最小总增益
#define MAX_TOTAL_GAIN_DB       10.0f    // 最大总增益

// Loss判断相关
#define LOSS_IMPROVEMENT_FACTOR  0.95f   // 新loss必须小于 init_loss * 此因子才接受更新

// 次级路径分母保护
#define SP_EPSILON              1e-8f    // 次级路径分母最小值，防止除零

// ============ 稳定性检测参数 ============
// 检测频段
#define STABLE_CHECK_FREQ_LOW   200.0f   // 检测频段下限 (Hz)
#define STABLE_CHECK_FREQ_HIGH  1000.0f  // 检测频段上限 (Hz)

// 平滑度检测
#define SMOOTH_ALPHA            3.0f     // 当前平滑度阈值倍数

// 局部尖峰检测
#define SPIKE_DELTA_DB          6.0f     // 单点变化阈值 (dB)
#define SPIKE_RATIO_THR         0.1f     // 超过阈值的频点比例上限

// 绝对幅度限制
#define RESPONSE_LOW_DB         -40.0f   // 频响下限 (dB)
#define RESPONSE_HIGH_DB        10.0f    // 频响上限 (dB)

// 整体偏移检测
#define MEAN_SHIFT_THR_DB       3.0f     // 平均偏移阈值 (dB)

// 数值微分步长（用于计算梯度）
#define EPSILON_GAIN            0.01f    // Gain数值微分步长 (dB)
#define EPSILON_Q               0.001f   // Q数值微分步长
#define EPSILON_FC              1.0f     // fc数值微分步长 (Hz)
#define EPSILON_TOTAL_GAIN      0.01f    // 总增益数值微分步长 (dB)

// ============ 文件路径配置 ============
#define WAV_INPUT_PATH          "input_4ch.wav"     // 输入4通道WAV文件（项目根目录）
#define SP_IR_PATH              "secondary_path.bin" // 次级路径冲击响应（项目根目录）
#define LOG_OUTPUT_PATH         "result/anc_log.txt"       // 日志文件（result目录）
#define WAV_OUTPUT_PATH         "result/output_comparison.wav" // 输出2通道对比WAV（result目录）

// WAV通道映射
#define WAV_CH_FF               0  // 参考麦通道索引
#define WAV_CH_FB               1  // 误差麦通道索引

// 次级路径参数
#define SP_IR_LENGTH            4096  // 次级路径FIR长度

// Biquad滤波器类型枚举
#ifndef BIQUAD_TYPE_DEFINED
#define BIQUAD_TYPE_DEFINED
typedef enum {
    BIQUAD_LOWSHELF = 0,
    BIQUAD_PEAKING,
    BIQUAD_HIGHSHELF
} BiquadType;
#endif

// Biquad参数结构体
#ifndef BIQUAD_PARAM_DEFINED
#define BIQUAD_PARAM_DEFINED
typedef struct {
    BiquadType type;
    float gain_dB;   // 增益（dB）
    float q;         // Q值
    float fc;        // 中心频率（Hz）
} BiquadParam;
#endif

// EQ参数集结构体
typedef struct {
    BiquadParam biquads[NUM_BIQUADS];
    float total_gain_dB;  // 总增益（dB）
} EQPreset;

// 次级路径频响（复数形式：实部、虚部交替存储）
// secondary_path[set][freq_index * 2 + 0] = 实部
// secondary_path[set][freq_index * 2 + 1] = 虚部
extern const float secondary_path[NUM_PRESET_SETS][NUM_FREQ_POINTS * 2];

// 预制EQ参数（初值前馈参数）
extern const EQPreset eq_presets[NUM_PRESET_SETS];

// 实际数据定义（这里给出示例结构，实际数值需要根据测量填充）
const float secondary_path[NUM_PRESET_SETS][NUM_FREQ_POINTS * 2] = {
    // Set 0: 示例数据（实部、虚部交替）
    {
        1.0f, 0.0f,  // 频点0
        0.99f, 0.1f, // 频点1
        // ... 其余频点
        [NUM_FREQ_POINTS * 2 - 2] = 0.5f,
        [NUM_FREQ_POINTS * 2 - 1] = 0.0f
    },
    // Set 1-9: 其他预制集
    // ... (类似结构)
};

const EQPreset eq_presets[NUM_PRESET_SETS] = {
    // Set 0
    {
        .biquads = {
            {BIQUAD_LOWSHELF, 0.0f, 0.707f, 100.0f},    // Biquad 0: Lowshelf
            {BIQUAD_PEAKING, 0.0f, 1.0f, 250.0f},       // Biquad 1-8: Peaking
            {BIQUAD_PEAKING, 0.0f, 1.0f, 500.0f},
            {BIQUAD_PEAKING, 0.0f, 1.0f, 1000.0f},
            {BIQUAD_PEAKING, 0.0f, 1.0f, 2000.0f},
            {BIQUAD_PEAKING, 0.0f, 1.0f, 4000.0f},
            {BIQUAD_PEAKING, 0.0f, 1.0f, 8000.0f},
            {BIQUAD_PEAKING, 0.0f, 1.0f, 12000.0f},
            {BIQUAD_PEAKING, 0.0f, 1.0f, 14000.0f},
            {BIQUAD_HIGHSHELF, 0.0f, 0.707f, 15000.0f}  // Biquad 9: Highshelf
        },
        .total_gain_dB = 0.0f
    },
    // Set 1-9: 其他预制集（类似结构）
    // ...
};

#endif // COEFFS_H
