#ifndef TIME_DOMAIN_SIM_H
#define TIME_DOMAIN_SIM_H

#include "config.h"
#include "fir_filter.h"

// Biquad时域滤波器状态
typedef struct {
    float x1, x2;  // 输入延迟
    float y1, y2;  // 输出延迟
} BiquadTimeDomainState;

// 时域仿真器结构体
typedef struct {
    // Biquad级联滤波器(10级)
    BiquadTimeDomainState biquad_states[NUM_BIQUADS];
    
    // 次级路径FIR滤波器
    FIRFilter secondary_path_fir;
    
    // 原始信号存储
    float *original_ff;      // 原始参考麦信号
    float *original_fb;      // 原始误差麦信号
    float *simulated_fb;     // 模拟降噪后的误差麦信号
    
    int total_samples;       // 总样本数
    int current_sample;      // 当前处理到的样本索引
    
    // 使能标志
    int enabled;
    
} TimeDomainSimulator;

/**
 * 初始化时域仿真器
 * @param sim 仿真器结构体
 * @param ff_signal 参考麦原始信号
 * @param fb_signal 误差麦原始信号
 * @param num_samples 样本数
 * @param sp_ir 次级路径冲击响应
 * @param sp_length 次级路径长度
 * @return 0=成功, -1=失败
 */
int time_sim_init(TimeDomainSimulator *sim, 
                  const float *ff_signal,
                  const float *fb_signal,
                  int num_samples,
                  const float *sp_ir,
                  int sp_length);

/**
 * 时域滤波一段信号(保证因果性)
 * 处理从current_sample开始的一段信号
 * @param sim 仿真器结构体
 * @param coeffs Biquad系数数组
 * @param total_gain 总增益(线性)
 * @param num_samples 要处理的样本数
 */
void time_sim_process(TimeDomainSimulator *sim,
                      BiquadCoeffs *coeffs,
                      float total_gain,
                      int num_samples);

/**
 * 获取当前时刻的参考麦和误差麦信号
 * 用于送回DSP进行下一轮FFT
 * @param sim 仿真器结构体
 * @param ff_out 输出参考麦信号缓冲区
 * @param fb_out 输出误差麦信号缓冲区  
 * @param num_samples 需要获取的样本数
 * @return 实际获取的样本数
 */
int time_sim_get_signals(TimeDomainSimulator *sim,
                         float *ff_out,
                         float *fb_out,
                         int num_samples);

/**
 * 单样本Biquad滤波
 * @param input 输入样本
 * @param coeffs Biquad系数
 * @param state Biquad状态
 * @return 输出样本
 */
float biquad_process_sample(float input, BiquadCoeffs *coeffs, BiquadTimeDomainState *state);

/**
 * 释放仿真器资源
 * @param sim 仿真器结构体
 */
void time_sim_free(TimeDomainSimulator *sim);

/**
 * 重置仿真器到初始状态
 * @param sim 仿真器结构体
 */
void time_sim_reset(TimeDomainSimulator *sim);

#endif // TIME_DOMAIN_SIM_H
