#include "../inc/time_domain_sim.h"
#include "../inc/logger.h"
#include <stdlib.h>
#include <string.h>

// 初始化时域仿真器
int time_sim_init(TimeDomainSimulator *sim,
                  const float *ff_signal,
                  const float *fb_signal,
                  int num_samples,
                  const float *sp_ir,
                  int sp_length) {
    
    sim->total_samples = num_samples;
    sim->current_sample = 0;
    
    // 分配内存
    sim->original_ff = (float *)malloc(num_samples * sizeof(float));
    sim->original_fb = (float *)malloc(num_samples * sizeof(float));
    sim->simulated_fb = (float *)malloc(num_samples * sizeof(float));
    
    if (!sim->original_ff || !sim->original_fb || !sim->simulated_fb) {
        log_printf("Error: Failed to allocate memory for time domain simulator\n");
        return -1;
    }
    
    // 复制原始信号
    memcpy(sim->original_ff, ff_signal, num_samples * sizeof(float));
    memcpy(sim->original_fb, fb_signal, num_samples * sizeof(float));
    
    // 初始化模拟信号为原始误差麦(未降噪)
    memcpy(sim->simulated_fb, fb_signal, num_samples * sizeof(float));
    
    // 初始化Biquad状态
    memset(sim->biquad_states, 0, sizeof(sim->biquad_states));
    
    // 初始化次级路径FIR
    fir_init(&sim->secondary_path_fir, sp_ir, sp_length);
    
    sim->enabled = 1;
    
    log_printf("Time domain simulator initialized: %d samples\n", num_samples);
    return 0;
}

// 单样本Biquad滤波
float biquad_process_sample(float input, BiquadCoeffs *coeffs, BiquadTimeDomainState *state) {
    // Direct Form II Transposed
    // y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
    
    float output = coeffs->b0 * input + state->x1;
    
    state->x1 = coeffs->b1 * input - coeffs->a1 * output + state->x2;
    state->x2 = coeffs->b2 * input - coeffs->a2 * output;
    
    return output;
}

// 时域滤波一段信号
void time_sim_process(TimeDomainSimulator *sim,
                      BiquadCoeffs *coeffs,
                      float total_gain,
                      int num_samples) {
    
    if (!sim->enabled) return;
    
    // 从current_sample开始滤波指定数量的样本
    int start_idx = sim->current_sample;
    
    // 检查边界
    if (start_idx + num_samples > sim->total_samples) {
        num_samples = sim->total_samples - start_idx;
    }
    
    if (num_samples <= 0) {
        log_printf("Warning: No samples to process in time domain sim\n");
        return;
    }
    
    log_printf("\n=== Time Domain Simulation ===\n");
    log_printf("Processing samples [%d, %d) (%d samples)\n", 
               start_idx, start_idx + num_samples, num_samples);
    log_printf("Progress: %.1f%% of total signal\n", 
               (float)start_idx / sim->total_samples * 100.0f);
    
    // 注意：这里需要重置滤波器状态，因为每次参数更新后是全新的滤波器
    // 重置Biquad状态
    memset(sim->biquad_states, 0, sizeof(sim->biquad_states));
    
    // 重置FIR状态
    fir_reset(&sim->secondary_path_fir);
    
    // 逐样本处理
    for (int i = 0; i < num_samples; i++) {
        int idx = start_idx + i;
        
        // 1. 读取原始参考麦信号
        float ff_sample = sim->original_ff[idx];
        
        // 2. Biquad级联滤波(10级)
        float filtered = ff_sample;
        for (int stage = 0; stage < NUM_BIQUADS; stage++) {
            filtered = biquad_process_sample(filtered, &coeffs[stage], 
                                             &sim->biquad_states[stage]);
        }
        
        // 3. 应用总增益
        filtered *= total_gain;
        
        // 4. 次级路径FIR滤波(模拟扬声器到误差麦的传递)
        float anti_noise = fir_process(&sim->secondary_path_fir, filtered);
        
        // 5. 与原始误差麦相减(主动降噪)
        float original_error = sim->original_fb[idx];
        float simulated_error = original_error - anti_noise;
        
        // 6. 更新模拟的误差麦信号
        sim->simulated_fb[idx] = simulated_error;
    }
    
    // 注意：这里不更新current_sample，因为下一轮DSP还是从current_sample开始读取
    
    log_printf("Time domain simulation completed: %d samples processed\n", num_samples);
}

// 获取当前时刻的信号
int time_sim_get_signals(TimeDomainSimulator *sim,
                         float *ff_out,
                         float *fb_out,
                         int num_samples) {
    
    if (!sim->enabled) return 0;
    
    // 检查是否有足够的样本
    int available = sim->total_samples - sim->current_sample;
    if (available <= 0) {
        log_printf("Warning: No more samples available\n");
        return 0;
    }
    
    if (num_samples > available) {
        num_samples = available;
    }
    
    // 复制信号
    // FF: 始终使用原始信号
    // FB: 使用模拟降噪后的信号（如果已经滤波过）
    memcpy(ff_out, &sim->original_ff[sim->current_sample], 
           num_samples * sizeof(float));
    memcpy(fb_out, &sim->simulated_fb[sim->current_sample], 
           num_samples * sizeof(float));
    
    // 移动指针（这些样本已经被DSP读取）
    sim->current_sample += num_samples;
    
    return num_samples;
}

// 释放资源
void time_sim_free(TimeDomainSimulator *sim) {
    if (sim->original_ff) {
        free(sim->original_ff);
        sim->original_ff = NULL;
    }
    if (sim->original_fb) {
        free(sim->original_fb);
        sim->original_fb = NULL;
    }
    if (sim->simulated_fb) {
        free(sim->simulated_fb);
        sim->simulated_fb = NULL;
    }
    sim->enabled = 0;
}

// 重置仿真器
void time_sim_reset(TimeDomainSimulator *sim) {
    sim->current_sample = 0;
    
    // 重置Biquad状态
    memset(sim->biquad_states, 0, sizeof(sim->biquad_states));
    
    // 重置FIR状态
    fir_reset(&sim->secondary_path_fir);
    
    // 恢复模拟信号为原始误差麦
    if (sim->simulated_fb && sim->original_fb) {
        memcpy(sim->simulated_fb, sim->original_fb, 
               sim->total_samples * sizeof(float));
    }
    
    log_printf("Time domain simulator reset\n");
}
