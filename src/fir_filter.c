#include "../inc/fir_filter.h"
#include <stdio.h>
#include <string.h>

// 初始化FIR滤波器
void fir_init(FIRFilter *fir, const float *coeffs, int length) {
    if (length > MAX_FIR_LENGTH) {
        printf("Warning: FIR length %d exceeds max %d, truncating\n", 
               length, MAX_FIR_LENGTH);
        length = MAX_FIR_LENGTH;
    }
    
    fir->length = length;
    fir->write_index = 0;
    
    // 复制系数
    memcpy(fir->coeffs, coeffs, length * sizeof(float));
    
    // 清空缓冲区
    memset(fir->buffer, 0, MAX_FIR_LENGTH * sizeof(float));
}

// FIR滤波单个样本
float fir_process(FIRFilter *fir, float input) {
    // 写入新样本到循环缓冲区
    fir->buffer[fir->write_index] = input;
    
    // 计算卷积: y[n] = Σ h[k] * x[n-k]
    float output = 0.0f;
    int read_index = fir->write_index;
    
    for (int k = 0; k < fir->length; k++) {
        output += fir->coeffs[k] * fir->buffer[read_index];
        
        // 循环索引递减
        read_index--;
        if (read_index < 0) {
            read_index = fir->length - 1;
        }
    }
    
    // 更新写指针
    fir->write_index++;
    if (fir->write_index >= fir->length) {
        fir->write_index = 0;
    }
    
    return output;
}

// 批量FIR滤波
void fir_process_block(FIRFilter *fir, const float *input, float *output, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        output[i] = fir_process(fir, input[i]);
    }
}

// 重置FIR滤波器状态
void fir_reset(FIRFilter *fir) {
    memset(fir->buffer, 0, MAX_FIR_LENGTH * sizeof(float));
    fir->write_index = 0;
}

// 从二进制文件加载FIR系数
int fir_load_coeffs(const char *filename, float *coeffs, int max_length) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Warning: Cannot open FIR coefficient file: %s\n", filename);
        return -1;
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    int num_coeffs = file_size / sizeof(float);
    if (num_coeffs > max_length) {
        printf("Warning: FIR file has %d coeffs, truncating to %d\n",
               num_coeffs, max_length);
        num_coeffs = max_length;
    }
    
    // 读取系数
    size_t read_count = fread(coeffs, sizeof(float), num_coeffs, file);
    fclose(file);
    
    if (read_count != num_coeffs) {
        printf("Error: Failed to read FIR coefficients\n");
        return -1;
    }
    
    printf("Loaded FIR coefficients: %d taps from %s\n", num_coeffs, filename);
    return num_coeffs;
}
