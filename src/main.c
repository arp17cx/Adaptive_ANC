#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../inc/config.h"
#include "../inc/coeffs.h"
#include "../inc/wav_io.h"
#include "../inc/fir_filter.h"
#include "../inc/time_domain_sim.h"
#include "../inc/logger.h"

// 定义M_PI（某些编译器可能没有）
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============ 全局变量 ============
SystemState g_system_state;
TimeDomainSimulator g_time_sim;

// Blackman窗函数
float blackman_window[FFT_LENGTH];

// ============ 函数声明 ============
void system_init(void);
void init_blackman_window(void);
void anti_alias_decimate(float *input, int input_len, float *output, int output_len);
void apply_window(float *buffer, float *windowed, int length);
void perform_fft(float *input, Complex *output, int length);
void accumulate_fft_results(Complex *fft_result, FreqResponse *accum);
void average_fft_results(FFTAccumulator *accum, FreqResponse *ff_avg, FreqResponse *fb_avg, 
                         FreqResponse *spk_avg, Complex *pp_average);
void calculate_mu(SystemState *state);
void calculate_target_ff(SystemState *state);
void calculate_ff_response(SystemState *state);
float calculate_smoothness(float *H_db, int length);
int check_target_stability(SystemState *state);
void calculate_ff_init_loss(SystemState *state);
float calculate_loss(SystemState *state);
int update_single_param(SystemState *state, int biquad_idx, int param_type);
void update_eq_params(SystemState *state);
void eq_to_biquad_coeffs(BiquadParam *eq_param, float sample_rate, BiquadCoeffs *coeffs);
void update_filter_coeffs(SystemState *state);
void process_audio_frame(float *ff_in, float *fb_in, float *spk_in, int frame_len);

// ============ 主函数 ============
/*
 * 自适应ANC算法状态机流程：
 * 
 * 1. SIGNAL_PROCESS: 
 *    - 每5ms接收音频帧
 *    - 累积100ms数据后开始FFT
 *    - 执行10次75% overlap FFT
 *    - 同时计算主路径传函 PP = 误差麦FFT / 参考麦FFT
 *    - 平均10次FFT结果和PP_AVERAGE
 * 
 * 2. CAL_MU:
 *    - 计算各频点自适应步长 μ(ω)
 * 
 * 3. CAL_FF_RESPONSE:
 *    - 计算当前生效的FF滤波器参数的频率响应 W_current(ω)
 *    - 必须先于CAL_TARGET_FF执行
 * 
 * 4. CAL_TARGET_FF:
 *    - 计算目标频响: W_target = W_current + μ * PP_AVERAGE / (SP + ε)
 * 
 * 5. STABLE_CHECK:
 *    - 检测目标频响是否稳定（平滑度、尖峰、界限、整体偏移）
 *    - 若异常则跳过本次更新，重新采集数据
 * 
 * 6. CAL_FF_INIT_LOSS:
 *    - 计算初始loss = |W_target - W_current|²
 *    - 作为梯度下降更新的基准阈值
 * 
 * 7. UPDATE_EQ_PARAMS:
 *    - 梯度下降优化10个Biquad参数(gain, Q, fc)和总增益
 *    - 只有新loss < init_loss才接受更新
 * 
 * 8. UPDATE_FILTER_COEFFS:
 *    - 将优化后的EQ参数转换为375kHz下的Biquad系数
 *    - 应用到实时滤波通路
 *    - 【关键时序】用新参数对当前位置之后的所有剩余原始信号进行滤波
 * 
 * 时序示例:
 *   0-100ms:    原始FF + 原始FB → DSP处理 → 得到参数v1
 *   滤波:       用v1对100ms后的所有剩余原始FF信号滤波，更新FB
 *   100-200ms:  原始FF + 降噪后FB → DSP处理 → 得到参数v2  
 *   滤波:       用v2对200ms后的所有剩余原始FF信号滤波，更新FB
 *   ...依此类推
 */
int main(void) {
    log_printf("==============================================\n");
    log_printf("  Adaptive ANC System with Time Domain Sim\n");
    log_printf("==============================================\n\n");
    
    // 初始化日志系统
    logger_init(LOG_OUTPUT_PATH, 1);  // 同时输出到文件和控制台
    
    log_printf("Configuration:\n");
    log_printf("  DSP Sample Rate: %d Hz\n", DSP_SAMPLE_RATE);
    log_printf("  Realtime Sample Rate: %d Hz\n", REALTIME_SAMPLE_RATE);
    log_printf("  FFT Length: %d\n", FFT_LENGTH);
    log_printf("  Process Interval: %d ms\n", PROCESS_INTERVAL_MS);
    log_printf("\n");
    
    // ========== 1. 加载WAV文件（如果存在） ==========
    WavData wav_data;
    int use_wav_input = 0;
    float *ff_signal = NULL;
    float *fb_signal = NULL;
    int total_samples = 0;
    int sample_rate_actual = REALTIME_SAMPLE_RATE;
    
    if (wav_file_exists(WAV_INPUT_PATH)) {
        log_printf("Loading WAV file: %s\n", WAV_INPUT_PATH);
        if (wav_read(WAV_INPUT_PATH, &wav_data) == 0) {
            if (wav_data.num_channels >= 2) {
                ff_signal = wav_data.channels[WAV_CH_FF];
                fb_signal = wav_data.channels[WAV_CH_FB];
                total_samples = wav_data.num_samples;
                sample_rate_actual = wav_data.sample_rate;
                use_wav_input = 1;
                log_printf("WAV file loaded successfully\n");
                log_printf("  Using channel %d as FF (reference mic)\n", WAV_CH_FF);
                log_printf("  Using channel %d as FB (error mic)\n", WAV_CH_FB);
            } else {
                log_printf("Warning: WAV file has < 2 channels, using generated signal\n");
            }
        }
    } else {
        log_printf("WAV file not found: %s\n", WAV_INPUT_PATH);
        log_printf("Using generated signal instead\n");
    }
    
    // 如果没有WAV文件,生成模拟信号
    if (!use_wav_input) {
        total_samples = sample_rate_actual * 10;  // 10秒
        ff_signal = (float *)malloc(total_samples * sizeof(float));
        fb_signal = (float *)malloc(total_samples * sizeof(float));
        
        for (int i = 0; i < total_samples; i++) {
            ff_signal[i] = 0.001f * sinf(2.0f * M_PI * 1000.0f * i / sample_rate_actual);
            fb_signal[i] = 0.0005f * sinf(2.0f * M_PI * 2000.0f * i / sample_rate_actual);
        }
        log_printf("Generated %d samples at %d Hz\n", total_samples, sample_rate_actual);
    }
    
    log_printf("\n");
    
    // ========== 2. 加载次级路径冲击响应 ==========
    float sp_ir[SP_IR_LENGTH];
    int sp_length = SP_IR_LENGTH;
    
    int sp_loaded = fir_load_coeffs(SP_IR_PATH, sp_ir, SP_IR_LENGTH);
    if (sp_loaded > 0) {
        sp_length = sp_loaded;
    } else {
        // 使用默认冲击响应（简化的模拟）
        log_printf("Using default secondary path IR\n");
        for (int i = 0; i < SP_IR_LENGTH; i++) {
            sp_ir[i] = 0.5f * expf(-i / 100.0f);  // 指数衰减
        }
    }
    
    log_printf("\n");
    
    // ========== 3. 初始化时域仿真器 ==========
    if (time_sim_init(&g_time_sim, ff_signal, fb_signal, total_samples, 
                      sp_ir, sp_length) != 0) {
        log_printf("Error: Failed to initialize time domain simulator\n");
        return -1;
    }
    
    log_printf("\n");
    
    // ========== 4. 系统初始化 ==========
    system_init();
    
    log_printf("\n");
    log_printf("==============================================\n");
    log_printf("  Starting Iterative Adaptation Loop\n");
    log_printf("==============================================\n\n");
    
    // ========== 5. 主迭代循环 ==========
    // 
    // 【正确的FFT时序】（按用户说明）:
    // - 累积0-100ms @ 32kHz = 3200样本
    // - 第1次FFT: 使用0-100ms数据（取前2048样本 = 0-64ms）
    // - 第2次FFT: 使用25-125ms数据（hop=25ms=800样本@32kHz）
    // - 第3次FFT: 使用50-150ms数据
    // - ...
    // - 第10次FFT: 使用225-325ms数据
    // 
    // 总结：
    // - 需要0-325ms @ 32kHz的数据（10400样本）
    // - 对应实时采样率: 325ms @ 375kHz = 121875样本
    // - 完成10次FFT后进行参数更新
    // - 然后用新参数滤波325ms之后的所有剩余信号
    //
    // 注意：当前config.h中FFT_HOP_SIZE=512(16ms)需要改为800(25ms)
    //      或者在这里直接按325ms一轮计算
    
    int iteration = 0;
    int max_iterations = 100;
    
    // 每轮迭代时间：325ms (用户指定)
    float iteration_time_ms = 325.0f;
    int iteration_samples = (int)(iteration_time_ms * sample_rate_actual / 1000.0f);
    
    log_printf("Iteration Timing:\n");
    log_printf("  Each iteration processes: %.1f ms (%d samples @ %d Hz)\n", 
               iteration_time_ms, iteration_samples, sample_rate_actual);
    log_printf("  Sequence: 0-%.1fms DSP -> Filter %.1fms-end -> Next from %.1fms\n\n",
               iteration_time_ms, iteration_time_ms, iteration_time_ms);
    
    while (g_time_sim.current_sample < total_samples && iteration < max_iterations) {
        int iteration_start_sample = g_time_sim.current_sample;
        float iteration_start_time_ms = (float)iteration_start_sample * 1000.0f / sample_rate_actual;
        float iteration_end_time_ms = iteration_start_time_ms + iteration_time_ms;
        
        log_printf("\n");
        log_printf("╔══════════════════════════════════════════════════════════════╗\n");
        log_printf("║  Iteration %d: %.1f - %.1f ms                              \n", 
                   iteration, iteration_start_time_ms, iteration_end_time_ms);
        log_printf("╚══════════════════════════════════════════════════════════════╝\n");
        log_printf("\n");
        
        // 5.1 处理这一轮的所有帧 (0-325ms的数据)
        log_printf("[Phase 1] DSP Processing (%.1f-%.1f ms)\n", 
                   iteration_start_time_ms, iteration_end_time_ms);
        log_printf("  - Accumulating 100ms\n");
        log_printf("  - Then 10x FFT with 75%% overlap\n");
        log_printf("  - Calculate parameters\n\n");
        
        int samples_processed = 0;
        int target_samples = iteration_samples;
        if (iteration_start_sample + target_samples > total_samples) {
            target_samples = total_samples - iteration_start_sample;
        }
        
        int frame_count_this_iteration = 0;
        
        // 逐帧处理
        while (samples_processed < target_samples) {
            int samples_per_frame = (sample_rate_actual * PROCESS_INTERVAL_MS) / 1000;
            int remaining = target_samples - samples_processed;
            if (samples_per_frame > remaining) {
                samples_per_frame = remaining;
            }
            
            float *ff_frame = (float *)malloc(samples_per_frame * sizeof(float));
            float *fb_frame = (float *)malloc(samples_per_frame * sizeof(float));
            float *spk_frame = (float *)calloc(samples_per_frame, sizeof(float));
            
            int got_samples = time_sim_get_signals(&g_time_sim, ff_frame, fb_frame, samples_per_frame);
            
            if (got_samples <= 0) {
                free(ff_frame);
                free(fb_frame);
                free(spk_frame);
                break;
            }
            
            // 处理音频帧 (DSP算法：降采样、FFT、参数计算)
            process_audio_frame(ff_frame, fb_frame, spk_frame, got_samples);
            
            samples_processed += got_samples;
            frame_count_this_iteration++;
            
            free(ff_frame);
            free(fb_frame);
            free(spk_frame);
        }
        
        if (samples_processed == 0) {
            log_printf("  No more samples, ending\n");
            break;
        }
        
        float actual_processed_time = (float)samples_processed * 1000.0f / sample_rate_actual;
        log_printf("  ✓ Processed: %.1f ms (%d samples, %d frames)\n", 
                   actual_processed_time, samples_processed, frame_count_this_iteration);
        log_printf("  ✓ DSP State: %s\n", 
                   g_system_state.state == SIGNAL_PROCESS ? "Parameters Updated" : "Processing");
        
        // 5.2 如果完成了参数计算，进行时域滤波
        if (g_system_state.state == SIGNAL_PROCESS && 
            g_system_state.eq_update.update_accepted) {
            
            log_printf("\n[Phase 2] Time Domain Filtering\n");
            
            // 计算剩余样本数 (从当前位置到结束)
            int filter_start_sample = g_time_sim.current_sample;
            int remaining_samples = total_samples - filter_start_sample;
            
            if (remaining_samples > 0) {
                float filter_start_time = (float)filter_start_sample * 1000.0f / sample_rate_actual;
                float filter_duration = (float)remaining_samples * 1000.0f / sample_rate_actual;
                
                log_printf("  Range: %.1f ms - end\n", filter_start_time);
                log_printf("  Duration: %.2f sec (%d samples)\n", 
                           filter_duration / 1000.0f, remaining_samples);
                log_printf("  Applying new Biquad parameters...\n");
                
                // 对所有剩余信号进行时域滤波
                time_sim_process(&g_time_sim,
                                g_system_state.ff_filter.coeffs,
                                g_system_state.ff_filter.total_gain,
                                remaining_samples);
                
                log_printf("  ✓ Filtering complete\n");
                log_printf("\n");
                log_printf("  Next iteration will use:\n");
                log_printf("    FF: Original signal from %.1f ms\n", filter_start_time);
                log_printf("    FB: Filtered signal from %.1f ms\n", filter_start_time);
            } else {
                log_printf("  No remaining samples to filter\n");
            }
            
            g_system_state.eq_update.update_accepted = 0;
        } else {
            log_printf("\n[Phase 2] Skipped (parameters not updated)\n");
        }
        
        iteration++;
        
        // 每5次迭代刷新日志
        if (iteration % 5 == 0) {
            logger_flush();
        }
    }
    
    log_printf("\n==============================================\n");
    log_printf("  Adaptation Loop Completed\n");
    log_printf("  Total iterations: %d\n", iteration);
    log_printf("==============================================\n\n");
    
    // ========== 6. 保存输出WAV文件 ==========
    log_printf("Saving output WAV file...\n");
    
    float *output_channels[2];
    output_channels[0] = g_time_sim.original_ff;  // 原始参考麦
    output_channels[1] = g_time_sim.simulated_fb; // 降噪后的误差麦
    
    wav_write(WAV_OUTPUT_PATH, output_channels, 2, total_samples, sample_rate_actual);
    
    log_printf("\n");
    
    // ========== 7. 清理资源 ==========
    time_sim_free(&g_time_sim);
    
    if (use_wav_input) {
        wav_free(&wav_data);
    } else {
        free(ff_signal);
        free(fb_signal);
    }
    
    logger_close();
    
    log_printf("\n==============================================\n");
    log_printf("  System finished successfully\n");
    log_printf("  Log file: %s\n", LOG_OUTPUT_PATH);
    log_printf("  Output WAV: %s\n", WAV_OUTPUT_PATH);
    log_printf("==============================================\n");
    
    return 0;
}

// ============ 系统初始化 ============
void system_init(void) {
    memset(&g_system_state, 0, sizeof(SystemState));
    
    // 初始化状态
    g_system_state.state = SIGNAL_PROCESS;
    g_system_state.current_preset_index = 0;  // 使用第一套预制参数
    
    // 加载预制次级路径
    for (int i = 0; i < FFT_HALF_LENGTH; i++) {
        g_system_state.secondary_path[i].real = secondary_path[g_system_state.current_preset_index][i * 2];
        g_system_state.secondary_path[i].imag = secondary_path[g_system_state.current_preset_index][i * 2 + 1];
    }
    
    // 加载预制EQ参数并转换为滤波器系数
    EQPreset *preset = (EQPreset *)&eq_presets[g_system_state.current_preset_index];
    
    // 初始化EQ更新状态
    for (int i = 0; i < NUM_BIQUADS; i++) {
        g_system_state.eq_update.params[i] = preset->biquads[i];
        eq_to_biquad_coeffs(&g_system_state.eq_update.params[i], REALTIME_SAMPLE_RATE, 
                            &g_system_state.ff_filter.coeffs[i]);
    }
    g_system_state.eq_update.total_gain_dB = preset->total_gain_dB;
    g_system_state.ff_filter.total_gain = powf(10.0f, preset->total_gain_dB / 20.0f);
    g_system_state.eq_update.init_loss = 0.0f;
    g_system_state.eq_update.current_loss = 0.0f;
    g_system_state.eq_update.update_accepted = 0;
    
    // 初始化稳定性检测
    memset(g_system_state.prev_target_ff, 0, sizeof(g_system_state.prev_target_ff));
    g_system_state.prev_smoothness = 1.0f;  // 初始值设为较小的数
    g_system_state.target_valid = 1;
    
    // 初始化Blackman窗
    init_blackman_window();
    
    log_printf("System initialized with preset %d\n", g_system_state.current_preset_index);
}

// ============ 初始化Blackman窗 ============
void init_blackman_window(void) {
    const float a0 = 0.42f;
    const float a1 = 0.5f;
    const float a2 = 0.08f;
    
    for (int i = 0; i < FFT_LENGTH; i++) {
        blackman_window[i] = a0 - a1 * cosf(2.0f * M_PI * i / (FFT_LENGTH - 1)) 
                                + a2 * cosf(4.0f * M_PI * i / (FFT_LENGTH - 1));
    }
}

// ============ 抗混叠降采样 ============
void anti_alias_decimate(float *input, int input_len, float *output, int output_len) {
    // 简化版：直接降采样（实际应用需要先低通滤波）
    // 降采样比例约为 375000/32000 ≈ 11.71875
    float ratio = (float)input_len / (float)output_len;
    
    for (int i = 0; i < output_len; i++) {
        int src_idx = (int)(i * ratio);
        if (src_idx < input_len) {
            output[i] = input[src_idx];
        }
    }
}

// ============ 应用窗函数 ============
void apply_window(float *buffer, float *windowed, int length) {
    for (int i = 0; i < length; i++) {
        windowed[i] = buffer[i] * blackman_window[i];
    }
}

// ============ 执行FFT（简化版，实际使用FFT库如FFTW或CMSIS-DSP） ============
void perform_fft(float *input, Complex *output, int length) {
    // 这里仅为示意，实际需要调用优化的FFT库
    // 例如: arm_rfft_fast_f32() 或 fftwf_execute()
    
    // 伪代码示意:
    // 1. 将实数输入转换为复数
    // 2. 执行FFT
    // 3. 提取正频率部分
    
    for (int k = 0; k < FFT_HALF_LENGTH; k++) {
        output[k].real = 0.0f;
        output[k].imag = 0.0f;
        
        for (int n = 0; n < length; n++) {
            float angle = -2.0f * M_PI * k * n / length;
            output[k].real += input[n] * cosf(angle);
            output[k].imag += input[n] * sinf(angle);
        }
    }
}

// ============ 累积FFT结果 ============
void accumulate_fft_results(Complex *fft_result, FreqResponse *accum) {
    for (int i = 0; i < FFT_HALF_LENGTH; i++) {
        accum->bins[i].real += fft_result[i].real;
        accum->bins[i].imag += fft_result[i].imag;
    }
}

// ============ 平均FFT结果 ============
void average_fft_results(FFTAccumulator *accum, FreqResponse *ff_avg, FreqResponse *fb_avg, 
                         FreqResponse *spk_avg, Complex *pp_average) {
    float scale = 1.0f / accum->accum_count;
    
    for (int i = 0; i < FFT_HALF_LENGTH; i++) {
        ff_avg->bins[i] = complex_scale(accum->ff_accum.bins[i], scale);
        fb_avg->bins[i] = complex_scale(accum->fb_accum.bins[i], scale);
        spk_avg->bins[i] = complex_scale(accum->spk_accum.bins[i], scale);
        
        // 平均主路径传函
        pp_average[i] = complex_scale(accum->pp_accum[i], scale);
    }
}

// ============ 计算步长μ ============
void calculate_mu(SystemState *state) {
    const float mu_min = 0.0001f;
    const float mu_max = 0.1f;
    const float regularization = 1e-6f;
    
    for (int i = 0; i < FFT_HALF_LENGTH; i++) {
        // 基于功率归一化的步长计算
        float ff_power = state->ff_avg.bins[i].real * state->ff_avg.bins[i].real + 
                         state->ff_avg.bins[i].imag * state->ff_avg.bins[i].imag;
        
        float sec_path_mag = complex_mag(state->secondary_path[i]);
        
        // μ = μ_base / (|S(ω)|² * P_ff(ω) + ε)
        state->mu[i] = mu_max / (sec_path_mag * sec_path_mag * ff_power + regularization);
        
        // 限制步长范围
        if (state->mu[i] < mu_min) state->mu[i] = mu_min;
        if (state->mu[i] > mu_max) state->mu[i] = mu_max;
    }
}

// ============ 计算目标前馈响应 ============
void calculate_target_ff(SystemState *state) {
    // 新算法：目标频响 = 当前生效的滤波器系数频响 + 步长 * PP_AVERAGE / (SP + ε)
    // 其中:
    //   - 当前FF频响: 已经在 calculate_ff_response() 中计算并存储在 state->current_ff
    //   - 步长: state->mu[i]
    //   - PP_AVERAGE: state->pp_average[i] (主路径传函 = 误差麦FFT / 参考麦FFT)
    //   - SP: state->secondary_path[i] (预制次级路径频响)
    //   - ε: SP_EPSILON 防止分母为0
    
    log_printf("=== Calculating Target FF Response ===\n");
    log_printf("Formula: W_target = W_current + mu * PP_AVERAGE / (SP + epsilon)\n");
    
    // 打印几个示例频点的值
    int sample_bins[] = {10, 100, 500};  // 示例频点
    for (int j = 0; j < 3; j++) {
        int i = sample_bins[j];
        if (i < FFT_HALF_LENGTH) {
            float freq = (float)i * DSP_SAMPLE_RATE / FFT_LENGTH;
            log_printf("  Bin %d (%.1f Hz): PP_mag=%.4f, SP_mag=%.4f, mu=%.6f\n",
                   i, freq,
                   complex_mag(state->pp_average[i]),
                   complex_mag(state->secondary_path[i]),
                   state->mu[i]);
        }
    }
    
    for (int i = 0; i < FFT_HALF_LENGTH; i++) {
        // 当前FF滤波器频响
        Complex current_ff = state->current_ff[i];
        
        // 主路径传函
        Complex pp = state->pp_average[i];
        
        // 次级路径（添加小量防止分母为0）
        Complex sp = state->secondary_path[i];
        float sp_mag = complex_mag(sp);
        
        // 如果SP幅度太小，添加保护
        if (sp_mag < SP_EPSILON) {
            // 按原角度添加最小幅度
            float phase = atan2f(sp.imag, sp.real);
            sp.real = SP_EPSILON * cosf(phase);
            sp.imag = SP_EPSILON * sinf(phase);
        }
        
        // 计算 PP_AVERAGE / (SP + ε)
        Complex pp_over_sp = complex_div(pp, sp);
        
        // 应用步长: mu * (PP_AVERAGE / SP)
        Complex update = complex_scale(pp_over_sp, state->mu[i]);
        
        // 目标频响 = 当前频响 + 步长 * PP_AVERAGE / SP
        state->target_ff[i] = complex_add(current_ff, update);
    }
    
    log_printf("Target FF response calculated successfully\n");
}

// ============ 计算平滑度指标（二阶差分的均方） ============
float calculate_smoothness(float *H_db, int length) {
    if (length < 3) return 0.0f;
    
    float sum = 0.0f;
    int count = 0;
    
    // 计算二阶差分: d²H/df² ≈ H[i+2] - 2*H[i+1] + H[i]
    for (int i = 0; i < length - 2; i++) {
        float d2 = H_db[i + 2] - 2.0f * H_db[i + 1] + H_db[i];
        sum += d2 * d2;
        count++;
    }
    
    return (count > 0) ? (sum / count) : 0.0f;
}

// ============ 稳定性检测：检查目标频响是否异常 ============
int check_target_stability(SystemState *state) {
    const float eps = 1e-8f;
    
    log_printf("\n=== Target Response Stability Check ===\n");
    
    // 确定检测频段的频点索引范围
    int bin_low = (int)(STABLE_CHECK_FREQ_LOW * FFT_LENGTH / DSP_SAMPLE_RATE);
    int bin_high = (int)(STABLE_CHECK_FREQ_HIGH * FFT_LENGTH / DSP_SAMPLE_RATE);
    
    if (bin_low < 0) bin_low = 0;
    if (bin_high >= FFT_HALF_LENGTH) bin_high = FFT_HALF_LENGTH - 1;
    
    int band_len = bin_high - bin_low + 1;
    
    if (band_len < 3) {
        log_printf("Band too narrow for stability check\n");
        return 1;  // 默认通过
    }
    
    // 分配临时数组存储dB值
    float *H_curr_db = (float *)malloc(band_len * sizeof(float));
    float *H_prev_db = (float *)malloc(band_len * sizeof(float));
    
    // 转换为dB
    for (int i = 0; i < band_len; i++) {
        int bin = bin_low + i;
        float mag_curr = complex_mag(state->target_ff[bin]);
        float mag_prev = complex_mag(state->prev_target_ff[bin]);
        
        H_curr_db[i] = 20.0f * log10f(mag_curr + eps);
        H_prev_db[i] = 20.0f * log10f(mag_prev + eps);
    }
    
    // ========== 检测1: 平滑度 ==========
    float smooth_curr = calculate_smoothness(H_curr_db, band_len);
    
    log_printf("Check 1 - Smoothness:\n");
    log_printf("  Current smoothness: %.4f\n", smooth_curr);
    log_printf("  Previous smoothness: %.4f\n", state->prev_smoothness);
    log_printf("  Threshold (%.1fx prev): %.4f\n", SMOOTH_ALPHA, SMOOTH_ALPHA * state->prev_smoothness);
    
    if (smooth_curr > SMOOTH_ALPHA * state->prev_smoothness && state->prev_smoothness > eps) {
        log_printf("  Result: FAIL (too rough)\n");
        free(H_curr_db);
        free(H_prev_db);
        return 0;
    }
    log_printf("  Result: PASS\n");
    
    // ========== 检测2: 局部尖峰 ==========
    int spike_count = 0;
    for (int i = 0; i < band_len; i++) {
        float delta = fabsf(H_curr_db[i] - H_prev_db[i]);
        if (delta > SPIKE_DELTA_DB) {
            spike_count++;
        }
    }
    
    float spike_ratio = (float)spike_count / band_len;
    
    log_printf("Check 2 - Local Spikes:\n");
    log_printf("  Points with >%.1f dB change: %d/%d (%.1f%%)\n", 
           SPIKE_DELTA_DB, spike_count, band_len, spike_ratio * 100.0f);
    log_printf("  Threshold: %.1f%%\n", SPIKE_RATIO_THR * 100.0f);
    
    if (spike_ratio > SPIKE_RATIO_THR) {
        log_printf("  Result: FAIL (too many spikes)\n");
        free(H_curr_db);
        free(H_prev_db);
        return 0;
    }
    log_printf("  Result: PASS\n");
    
    // ========== 检测3: 绝对幅度界限 ==========
    float min_db = H_curr_db[0];
    float max_db = H_curr_db[0];
    
    for (int i = 1; i < band_len; i++) {
        if (H_curr_db[i] < min_db) min_db = H_curr_db[i];
        if (H_curr_db[i] > max_db) max_db = H_curr_db[i];
    }
    
    log_printf("Check 3 - Absolute Bounds:\n");
    log_printf("  Response range: [%.2f, %.2f] dB\n", min_db, max_db);
    log_printf("  Allowed range: [%.2f, %.2f] dB\n", RESPONSE_LOW_DB, RESPONSE_HIGH_DB);
    
    if (min_db < RESPONSE_LOW_DB || max_db > RESPONSE_HIGH_DB) {
        log_printf("  Result: FAIL (out of bounds)\n");
        free(H_curr_db);
        free(H_prev_db);
        return 0;
    }
    log_printf("  Result: PASS\n");
    
    // ========== 检测4: 整体偏移 ==========
    float mean_delta = 0.0f;
    for (int i = 0; i < band_len; i++) {
        mean_delta += (H_curr_db[i] - H_prev_db[i]);
    }
    mean_delta /= band_len;
    
    log_printf("Check 4 - Global Shift:\n");
    log_printf("  Mean shift: %.2f dB\n", mean_delta);
    log_printf("  Threshold: %.2f dB\n", MEAN_SHIFT_THR_DB);
    
    if (fabsf(mean_delta) > MEAN_SHIFT_THR_DB) {
        log_printf("  Result: FAIL (too much shift)\n");
        free(H_curr_db);
        free(H_prev_db);
        return 0;
    }
    log_printf("  Result: PASS\n");
    
    // 所有检测通过，更新平滑度基准
    state->prev_smoothness = smooth_curr;
    
    log_printf("\n=== Stability Check: PASSED ===\n");
    
    free(H_curr_db);
    free(H_prev_db);
    return 1;
}

// ============ 旧算法（已废弃） ============
/*
void calculate_target_ff_old(SystemState *state) {
    // 旧目标：W_target(ω) = -E(ω) / (S(ω) * X_ff(ω))
    // 其中 E(ω) 是误差信号（FB），X_ff(ω) 是前馈参考
    
    for (int i = 0; i < FFT_HALF_LENGTH; i++) {
        Complex error = state->fb_avg.bins[i];
        Complex ff_ref = state->ff_avg.bins[i];
        Complex sec_path = state->secondary_path[i];
        
        // 计算分母: S(ω) * X_ff(ω)
        Complex denom = complex_mul(sec_path, ff_ref);
        
        // 计算目标响应: -E(ω) / denom
        Complex neg_error = {-error.real, -error.imag};
        state->target_ff[i] = complex_div(neg_error, denom);
    }
}
*/

// ============ 计算前馈滤波器频响 ============
void calculate_ff_response(SystemState *state) {
    // 从当前Biquad系数计算频率响应
    // H(z) = (b0 + b1*z^-1 + b2*z^-2) / (a0 + a1*z^-1 + a2*z^-2)
    
    for (int k = 0; k < FFT_HALF_LENGTH; k++) {
        float omega = 2.0f * M_PI * k / FFT_LENGTH;
        
        Complex H = {1.0f, 0.0f};  // 初始化为1
        
        // 级联所有Biquad
        for (int stage = 0; stage < NUM_BIQUADS; stage++) {
            BiquadCoeffs *c = &state->ff_filter.coeffs[stage];
            
            // z^-1 = e^(-jω)
            Complex z_inv = {cosf(omega), -sinf(omega)};
            Complex z_inv2 = complex_mul(z_inv, z_inv);
            
            // 分子: b0 + b1*z^-1 + b2*z^-2
            Complex num = {c->b0, 0.0f};
            num = complex_add(num, complex_scale(z_inv, c->b1));
            num = complex_add(num, complex_scale(z_inv2, c->b2));
            
            // 分母: 1 + a1*z^-1 + a2*z^-2 (a0已归一化为1)
            Complex den = {1.0f, 0.0f};
            den = complex_add(den, complex_scale(z_inv, c->a1));
            den = complex_add(den, complex_scale(z_inv2, c->a2));
            
            // H_stage = num / den
            Complex H_stage = complex_div(num, den);
            
            // 级联
            H = complex_mul(H, H_stage);
        }
        
        // 应用总增益
        state->current_ff[k] = complex_scale(H, state->ff_filter.total_gain);
    }
}

// ============ 计算初始FF响应与目标的loss ============
void calculate_ff_init_loss(SystemState *state) {
    // 计算当前FF滤波器参数产生的频响
    calculate_ff_response(state);
    
    // 计算当前FF响应与目标响应的拟合误差
    state->eq_update.init_loss = calculate_loss(state);
    
    log_printf("=== Initial FF Loss Calculation ===\n");
    log_printf("Initial Loss (current FF vs target): %.6f\n", state->eq_update.init_loss);
    log_printf("This will be used as baseline for parameter updates\n");
    
    // 同时初始化current_loss
    state->eq_update.current_loss = state->eq_update.init_loss;
}

// ============ 计算损失函数 ============
float calculate_loss(SystemState *state) {
    // Loss = Σ |W_target(ω) - W_current(ω)|²
    float loss = 0.0f;
    
    for (int i = 0; i < FFT_HALF_LENGTH; i++) {
        Complex diff = complex_sub(state->target_ff[i], state->current_ff[i]);
        float mag_squared = diff.real * diff.real + diff.imag * diff.imag;
        loss += mag_squared;
    }
    
    // 归一化
    loss /= FFT_HALF_LENGTH;
    
    return loss;
}

// ============ 参数限幅函数 ============
float clamp_value(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

// ============ 更新单个Biquad的单个参数（梯度下降） ============
int update_single_param(SystemState *state, int biquad_idx, int param_type) {
    // param_type: 0=gain, 1=Q, 2=fc
    BiquadParam *param = &state->eq_update.params[biquad_idx];
    float original_loss = state->eq_update.current_loss;
    
    // 保存原始参数
    float original_value;
    float epsilon, learning_rate, max_delta, min_val, max_val;
    const char *param_name;
    
    switch (param_type) {
        case 0: // Gain
            original_value = param->gain_dB;
            epsilon = EPSILON_GAIN;
            learning_rate = LEARNING_RATE_GAIN;
            max_delta = MAX_DELTA_GAIN;
            min_val = MIN_GAIN_DB;
            max_val = MAX_GAIN_DB;
            param_name = "Gain";
            break;
        case 1: // Q
            original_value = param->q;
            epsilon = EPSILON_Q;
            learning_rate = LEARNING_RATE_Q;
            max_delta = MAX_DELTA_Q;
            min_val = MIN_Q;
            max_val = MAX_Q;
            param_name = "Q";
            break;
        case 2: // fc
            original_value = param->fc;
            epsilon = EPSILON_FC;
            learning_rate = LEARNING_RATE_FC;
            max_delta = MAX_DELTA_FC;
            min_val = MIN_FC;
            max_val = MAX_FC;
            param_name = "fc";
            break;
        default:
            return 0;
    }
    
    // 计算梯度（数值微分）
    float *param_ptr = (param_type == 0) ? &param->gain_dB : 
                       (param_type == 1) ? &param->q : &param->fc;
    
    *param_ptr += epsilon;
    eq_to_biquad_coeffs(param, REALTIME_SAMPLE_RATE, &state->ff_filter.coeffs[biquad_idx]);
    calculate_ff_response(state);
    float loss_plus = calculate_loss(state);
    
    float gradient = (loss_plus - original_loss) / epsilon;
    
    // 恢复原值
    *param_ptr = original_value;
    
    // 计算更新量
    float delta = -learning_rate * gradient;
    delta = clamp_value(delta, -max_delta, max_delta);
    
    // 尝试更新
    float new_value = original_value + delta;
    new_value = clamp_value(new_value, min_val, max_val);
    *param_ptr = new_value;
    
    // 重新计算loss
    eq_to_biquad_coeffs(param, REALTIME_SAMPLE_RATE, &state->ff_filter.coeffs[biquad_idx]);
    calculate_ff_response(state);
    float new_loss = calculate_loss(state);
    
    // 判断是否接受更新
    if (new_loss < original_loss) {
        // 接受更新
        state->eq_update.current_loss = new_loss;
        log_printf("  Biquad[%d] %s: %.4f->%.4f, loss: %.6f->%.6f (ACCEPT)\n",
               biquad_idx, param_name, original_value, new_value, original_loss, new_loss);
        return 1;
    } else {
        // 拒绝更新，恢复原值
        *param_ptr = original_value;
        eq_to_biquad_coeffs(param, REALTIME_SAMPLE_RATE, &state->ff_filter.coeffs[biquad_idx]);
        calculate_ff_response(state);
        log_printf("  Biquad[%d] %s: %.4f (no change, loss would increase)\n",
               biquad_idx, param_name, original_value);
        return 0;
    }
}

// ============ 更新EQ参数（依次梯度下降） ============
void update_eq_params(SystemState *state) {
    // 计算当前损失
    calculate_ff_response(state);
    state->eq_update.current_loss = calculate_loss(state);
    
    log_printf("\n=== EQ Parameter Update (Sequential Gradient Descent) ===\n");
    log_printf("Initial Loss (baseline): %.6f\n", state->eq_update.init_loss);
    log_printf("Current Loss: %.6f\n", state->eq_update.current_loss);
    
    // 判断当前参数是否已经足够接近目标
    float loss_threshold = state->eq_update.init_loss * LOSS_IMPROVEMENT_FACTOR;
    
    if (state->eq_update.current_loss <= loss_threshold) {
        log_printf("Current loss already good (< %.6f), skipping parameter update\n", loss_threshold);
        state->eq_update.update_accepted = 1;
        return;
    }
    
    log_printf("Attempting sequential gradient descent update (DSP-friendly)...\n");
    log_printf("Strategy: Update Gain, Q, fc for each Biquad sequentially\n\n");
    
    int total_accepted = 0;
    
    // ========== 依次优化每个Biquad的每个参数 ==========
    for (int i = 0; i < NUM_BIQUADS; i++) {
        log_printf("Biquad[%d] (type=%d):\n", i, state->eq_update.params[i].type);
        
        // 先优化Gain
        if (update_single_param(state, i, 0)) total_accepted++;
        
        // 再优化Q
        if (update_single_param(state, i, 1)) total_accepted++;
        
        // 最后优化fc
        if (update_single_param(state, i, 2)) total_accepted++;
    }
    
    // ========== 优化总增益 ==========
    log_printf("\nTotal Gain:\n");
    float original_total_gain = state->eq_update.total_gain_dB;
    float original_loss = state->eq_update.current_loss;
    
    // 计算梯度
    state->eq_update.total_gain_dB += EPSILON_TOTAL_GAIN;
    state->ff_filter.total_gain = powf(10.0f, state->eq_update.total_gain_dB / 20.0f);
    calculate_ff_response(state);
    float loss_plus = calculate_loss(state);
    
    float gradient = (loss_plus - original_loss) / EPSILON_TOTAL_GAIN;
    
    // 恢复并更新
    state->eq_update.total_gain_dB = original_total_gain;
    float delta = -LEARNING_RATE_TOTAL_GAIN * gradient;
    delta = clamp_value(delta, -MAX_DELTA_TOTAL_GAIN, MAX_DELTA_TOTAL_GAIN);
    
    float new_total_gain = original_total_gain + delta;
    new_total_gain = clamp_value(new_total_gain, MIN_TOTAL_GAIN_DB, MAX_TOTAL_GAIN_DB);
    
    state->eq_update.total_gain_dB = new_total_gain;
    state->ff_filter.total_gain = powf(10.0f, new_total_gain / 20.0f);
    calculate_ff_response(state);
    float new_loss = calculate_loss(state);
    
    if (new_loss < original_loss) {
        state->eq_update.current_loss = new_loss;
        log_printf("  Total Gain: %.2f->%.2f dB, loss: %.6f->%.6f (ACCEPT)\n",
               original_total_gain, new_total_gain, original_loss, new_loss);
        total_accepted++;
    } else {
        state->eq_update.total_gain_dB = original_total_gain;
        state->ff_filter.total_gain = powf(10.0f, original_total_gain / 20.0f);
        calculate_ff_response(state);
        log_printf("  Total Gain: %.2f dB (no change, loss would increase)\n", original_total_gain);
    }
    
    // ========== 最终判断 ==========
    log_printf("\n--- Update Summary ---\n");
    log_printf("Parameters accepted: %d / %d\n", total_accepted, NUM_BIQUADS * 3 + 1);
    log_printf("Final loss: %.6f\n", state->eq_update.current_loss);
    
    if (state->eq_update.current_loss < state->eq_update.init_loss) {
        state->eq_update.update_accepted = 1;
        log_printf("Overall: ACCEPTED (final loss < init loss)\n");
    } else {
        state->eq_update.update_accepted = 0;
        log_printf("Overall: REJECTED (final loss >= init loss)\n");
    }
}

// ============ EQ参数转Biquad系数 ============
void eq_to_biquad_coeffs(BiquadParam *eq_param, float sample_rate, BiquadCoeffs *coeffs) {
    float A = powf(10.0f, eq_param->gain_dB / 40.0f);  // 幅度
    float omega0 = 2.0f * M_PI * eq_param->fc / sample_rate;
    float alpha = sinf(omega0) / (2.0f * eq_param->q);
    float cos_omega0 = cosf(omega0);
    
    switch (eq_param->type) {
        case BIQUAD_LOWSHELF:
            coeffs->b0 = A * ((A + 1) - (A - 1) * cos_omega0 + 2 * sqrtf(A) * alpha);
            coeffs->b1 = 2 * A * ((A - 1) - (A + 1) * cos_omega0);
            coeffs->b2 = A * ((A + 1) - (A - 1) * cos_omega0 - 2 * sqrtf(A) * alpha);
            coeffs->a0 = (A + 1) + (A - 1) * cos_omega0 + 2 * sqrtf(A) * alpha;
            coeffs->a1 = -2 * ((A - 1) + (A + 1) * cos_omega0);
            coeffs->a2 = (A + 1) + (A - 1) * cos_omega0 - 2 * sqrtf(A) * alpha;
            break;
            
        case BIQUAD_HIGHSHELF:
            coeffs->b0 = A * ((A + 1) + (A - 1) * cos_omega0 + 2 * sqrtf(A) * alpha);
            coeffs->b1 = -2 * A * ((A - 1) + (A + 1) * cos_omega0);
            coeffs->b2 = A * ((A + 1) + (A - 1) * cos_omega0 - 2 * sqrtf(A) * alpha);
            coeffs->a0 = (A + 1) - (A - 1) * cos_omega0 + 2 * sqrtf(A) * alpha;
            coeffs->a1 = 2 * ((A - 1) - (A + 1) * cos_omega0);
            coeffs->a2 = (A + 1) - (A - 1) * cos_omega0 - 2 * sqrtf(A) * alpha;
            break;
            
        case BIQUAD_PEAKING:
            coeffs->b0 = 1 + alpha * A;
            coeffs->b1 = -2 * cos_omega0;
            coeffs->b2 = 1 - alpha * A;
            coeffs->a0 = 1 + alpha / A;
            coeffs->a1 = -2 * cos_omega0;
            coeffs->a2 = 1 - alpha / A;
            break;
    }
    
    // 归一化
    coeffs->b0 /= coeffs->a0;
    coeffs->b1 /= coeffs->a0;
    coeffs->b2 /= coeffs->a0;
    coeffs->a1 /= coeffs->a0;
    coeffs->a2 /= coeffs->a0;
    coeffs->a0 = 1.0f;
}

// ============ 更新滤波器系数到375kHz ============
void update_filter_coeffs(SystemState *state) {
    // 使用梯度下降更新后的EQ参数
    // 转换为375kHz采样率下的Biquad系数
    
    log_printf("Updating filter coefficients to %d Hz sample rate...\n", REALTIME_SAMPLE_RATE);
    
    for (int i = 0; i < NUM_BIQUADS; i++) {
        // 使用当前优化后的参数
        eq_to_biquad_coeffs(&state->eq_update.params[i], REALTIME_SAMPLE_RATE, 
                            &state->ff_filter.coeffs[i]);
        
        log_printf("  Biquad[%d]: type=%d, gain=%.2f dB, Q=%.3f, fc=%.1f Hz\n",
               i, state->eq_update.params[i].type,
               state->eq_update.params[i].gain_dB,
               state->eq_update.params[i].q,
               state->eq_update.params[i].fc);
    }
    
    // 更新总增益
    state->ff_filter.total_gain = powf(10.0f, state->eq_update.total_gain_dB / 20.0f);
    log_printf("  Total Gain: %.2f dB (linear: %.4f)\n", 
           state->eq_update.total_gain_dB, state->ff_filter.total_gain);
    
    log_printf("Filter coefficients updated successfully\n");
}

// ============ 处理音频帧 ============
void process_audio_frame(float *ff_in, float *fb_in, float *spk_in, int frame_len) {
    // 1. 抗混叠降采样到32kHz
    float ff_decimated[SAMPLES_PER_INTERVAL];
    float fb_decimated[SAMPLES_PER_INTERVAL];
    float spk_decimated[SAMPLES_PER_INTERVAL];
    
    anti_alias_decimate(ff_in, frame_len, ff_decimated, SAMPLES_PER_INTERVAL);
    anti_alias_decimate(fb_in, frame_len, fb_decimated, SAMPLES_PER_INTERVAL);
    anti_alias_decimate(spk_in, frame_len, spk_decimated, SAMPLES_PER_INTERVAL);
    
    // 2. 将数据填入buffer
    TimeBuffer *ff_buf = &g_system_state.ff_buffer;
    TimeBuffer *fb_buf = &g_system_state.fb_buffer;
    TimeBuffer *spk_buf = &g_system_state.spk_buffer;
    
    for (int i = 0; i < SAMPLES_PER_INTERVAL; i++) {
        ff_buf->data[ff_buf->write_index] = ff_decimated[i];
        fb_buf->data[fb_buf->write_index] = fb_decimated[i];
        spk_buf->data[spk_buf->write_index] = spk_decimated[i];
        
        ff_buf->write_index = (ff_buf->write_index + 1) % FFT_LENGTH;
        fb_buf->write_index = (fb_buf->write_index + 1) % FFT_LENGTH;
        spk_buf->write_index = (spk_buf->write_index + 1) % FFT_LENGTH;
    }
    
    ff_buf->sample_count += SAMPLES_PER_INTERVAL;
    fb_buf->sample_count += SAMPLES_PER_INTERVAL;
    spk_buf->sample_count += SAMPLES_PER_INTERVAL;
    
    // 3. 状态机处理
    switch (g_system_state.state) {
        case SIGNAL_PROCESS:
            // 检查是否累积了足够的样本开始FFT
            if (g_system_state.frame_count == 0 && ff_buf->sample_count >= FFT_LENGTH) {
                // 第一次FFT
                g_system_state.fft_count = 0;
                memset(&g_system_state.fft_accum, 0, sizeof(FFTAccumulator));
            }
            
            // 每个hop执行一次FFT（75% overlap）
            if (ff_buf->sample_count >= FFT_HOP_SIZE && g_system_state.fft_count < NUM_FFT_AVERAGE) {
                // 执行FFT
                float windowed[FFT_LENGTH];
                Complex fft_result[FFT_HALF_LENGTH];
                Complex ff_fft[FFT_HALF_LENGTH];
                Complex fb_fft[FFT_HALF_LENGTH];
                
                // FF通道 (参考麦 Srr)
                apply_window(ff_buf->data, windowed, FFT_LENGTH);
                perform_fft(windowed, ff_fft, FFT_LENGTH);
                accumulate_fft_results(ff_fft, &g_system_state.fft_accum.ff_accum);
                
                // FB通道 (误差麦 Sre)
                apply_window(fb_buf->data, windowed, FFT_LENGTH);
                perform_fft(windowed, fb_fft, FFT_LENGTH);
                accumulate_fft_results(fb_fft, &g_system_state.fft_accum.fb_accum);
                
                // SPK通道
                apply_window(spk_buf->data, windowed, FFT_LENGTH);
                perform_fft(windowed, fft_result, FFT_LENGTH);
                accumulate_fft_results(fft_result, &g_system_state.fft_accum.spk_accum);
                
                // 计算主路径传函: PP = Sre/Srr = FB/FF (误差麦/参考麦)
                for (int i = 0; i < FFT_HALF_LENGTH; i++) {
                    Complex pp = complex_div(fb_fft[i], ff_fft[i]);
                    // 累积主路径传函
                    g_system_state.fft_accum.pp_accum[i].real += pp.real;
                    g_system_state.fft_accum.pp_accum[i].imag += pp.imag;
                }
                
                g_system_state.fft_accum.accum_count++;
                g_system_state.fft_count++;
                
                // 移动buffer指针（hop）
                ff_buf->sample_count -= FFT_HOP_SIZE;
                fb_buf->sample_count -= FFT_HOP_SIZE;
                spk_buf->sample_count -= FFT_HOP_SIZE;
            }
            
            // 完成10次FFT平均
            if (g_system_state.fft_count >= NUM_FFT_AVERAGE) {
                average_fft_results(&g_system_state.fft_accum, 
                                    &g_system_state.ff_avg, 
                                    &g_system_state.fb_avg, 
                                    &g_system_state.spk_avg,
                                    g_system_state.pp_average);
                
                g_system_state.state = CAL_MU;
            }
            
            g_system_state.frame_count++;
            break;
            
        case CAL_MU:
            // 计算各频点步长
            calculate_mu(&g_system_state);
            g_system_state.state = CAL_FF_RESPONSE;
            break;
            
        case CAL_FF_RESPONSE:
            // 计算当前前馈滤波器频响（必须先于CAL_TARGET_FF）
            calculate_ff_response(&g_system_state);
            g_system_state.state = CAL_TARGET_FF;
            break;
            
        case CAL_TARGET_FF:
            // 计算目标前馈响应（使用已计算的current_ff）
            calculate_target_ff(&g_system_state);
            g_system_state.state = STABLE_CHECK;
            break;
            
        case STABLE_CHECK:
            // 检测目标频响是否稳定/异常
            g_system_state.target_valid = check_target_stability(&g_system_state);
            
            if (g_system_state.target_valid) {
                // 通过稳定性检测，保存当前目标作为下次检测的参考
                memcpy(g_system_state.prev_target_ff, g_system_state.target_ff, 
                       sizeof(g_system_state.target_ff));
                
                // 继续下一步
                g_system_state.state = CAL_FF_INIT_LOSS;
            } else {
                // 未通过检测，跳过本次更新，直接重置状态
                log_printf("WARNING: Target response failed stability check, skipping update\n");
                g_system_state.state = SIGNAL_PROCESS;
                g_system_state.frame_count = 0;
                g_system_state.fft_count = 0;
            }
            break;
            
        case CAL_FF_INIT_LOSS:
            // 计算初始loss(当前FF参数与目标的拟合误差)
            // 这个loss作为后续梯度下降更新的基准阈值
            calculate_ff_init_loss(&g_system_state);
            g_system_state.state = UPDATE_EQ_PARAMS;
            break;
            
        case UPDATE_EQ_PARAMS:
            // 更新EQ参数
            update_eq_params(&g_system_state);
            g_system_state.state = UPDATE_FILTER_COEFFS;
            break;
            
        case UPDATE_FILTER_COEFFS:
            // 更新滤波器系数到375kHz
            update_filter_coeffs(&g_system_state);
            
            // 完成一轮自适应，重置状态
            g_system_state.state = SIGNAL_PROCESS;
            g_system_state.frame_count = 0;
            g_system_state.fft_count = 0;
            break;
            
        default:
            g_system_state.state = SIGNAL_PROCESS;
            break;
    }
}
