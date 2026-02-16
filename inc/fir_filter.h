#ifndef FIR_FILTER_H
#define FIR_FILTER_H

#define MAX_FIR_LENGTH 8192

// FIR滤波器结构体
typedef struct {
    float coeffs[MAX_FIR_LENGTH];  // 滤波器系数(冲击响应)
    float buffer[MAX_FIR_LENGTH];  // 延迟线缓冲区
    int length;                     // 滤波器长度
    int write_index;                // 循环缓冲区写指针
} FIRFilter;

/**
 * 初始化FIR滤波器
 * @param fir 滤波器结构体
 * @param coeffs 滤波器系数数组
 * @param length 滤波器长度
 */
void fir_init(FIRFilter *fir, const float *coeffs, int length);

/**
 * FIR滤波单个样本
 * @param fir 滤波器结构体
 * @param input 输入样本
 * @return 输出样本
 */
float fir_process(FIRFilter *fir, float input);

/**
 * 批量FIR滤波
 * @param fir 滤波器结构体
 * @param input 输入样本数组
 * @param output 输出样本数组
 * @param num_samples 样本数
 */
void fir_process_block(FIRFilter *fir, const float *input, float *output, int num_samples);

/**
 * 重置FIR滤波器状态
 * @param fir 滤波器结构体
 */
void fir_reset(FIRFilter *fir);

/**
 * 从二进制文件加载FIR系数
 * @param filename 文件路径
 * @param coeffs 输出系数数组
 * @param max_length 最大长度
 * @return 实际读取的长度, -1表示失败
 */
int fir_load_coeffs(const char *filename, float *coeffs, int max_length);

#endif // FIR_FILTER_H
