#include "../inc/wav_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 检查文件是否存在
int wav_file_exists(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file) {
        fclose(file);
        return 1;
    }
    return 0;
}

// 读取WAV文件
int wav_read(const char *filename, WavData *wav_data) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Error: Cannot open WAV file: %s\n", filename);
        wav_data->valid = 0;
        return -1;
    }
    
    // 读取头部
    size_t read_count = fread(&wav_data->header, sizeof(WavHeader), 1, file);
    if (read_count != 1) {
        printf("Error: Failed to read WAV header\n");
        fclose(file);
        wav_data->valid = 0;
        return -1;
    }
    
    // 验证格式
    if (strncmp(wav_data->header.chunk_id, "RIFF", 4) != 0 ||
        strncmp(wav_data->header.format, "WAVE", 4) != 0) {
        printf("Error: Not a valid WAV file\n");
        fclose(file);
        wav_data->valid = 0;
        return -1;
    }
    
    if (wav_data->header.audio_format != 1) {
        printf("Error: Only PCM format supported (got format %d)\n", 
               wav_data->header.audio_format);
        fclose(file);
        wav_data->valid = 0;
        return -1;
    }
    
    // 提取信息
    wav_data->num_channels = wav_data->header.num_channels;
    wav_data->sample_rate = wav_data->header.sample_rate;
    int bytes_per_sample = wav_data->header.bits_per_sample / 8;
    wav_data->num_samples = wav_data->header.subchunk2_size / 
                            (wav_data->num_channels * bytes_per_sample);
    
    printf("WAV Info: %d channels, %d Hz, %d bits, %d samples\n",
           wav_data->num_channels, wav_data->sample_rate,
           wav_data->header.bits_per_sample, wav_data->num_samples);
    
    // 分配内存
    wav_data->channels = (float **)malloc(wav_data->num_channels * sizeof(float *));
    for (int ch = 0; ch < wav_data->num_channels; ch++) {
        wav_data->channels[ch] = (float *)malloc(wav_data->num_samples * sizeof(float));
    }
    
    // 读取交织音频数据
    if (wav_data->header.bits_per_sample == 16) {
        int16_t *buffer = (int16_t *)malloc(wav_data->num_channels * sizeof(int16_t));
        
        for (int i = 0; i < wav_data->num_samples; i++) {
            read_count = fread(buffer, sizeof(int16_t), wav_data->num_channels, file);
            if (read_count != wav_data->num_channels) break;
            
            // 转换为归一化浮点数 [-1, 1]
            for (int ch = 0; ch < wav_data->num_channels; ch++) {
                wav_data->channels[ch][i] = (float)buffer[ch] / 32768.0f;
            }
        }
        
        free(buffer);
    } else if (wav_data->header.bits_per_sample == 32) {
        int32_t *buffer = (int32_t *)malloc(wav_data->num_channels * sizeof(int32_t));
        
        for (int i = 0; i < wav_data->num_samples; i++) {
            read_count = fread(buffer, sizeof(int32_t), wav_data->num_channels, file);
            if (read_count != wav_data->num_channels) break;
            
            for (int ch = 0; ch < wav_data->num_channels; ch++) {
                wav_data->channels[ch][i] = (float)buffer[ch] / 2147483648.0f;
            }
        }
        
        free(buffer);
    } else {
        printf("Error: Unsupported bit depth: %d\n", wav_data->header.bits_per_sample);
        fclose(file);
        wav_free(wav_data);
        wav_data->valid = 0;
        return -1;
    }
    
    fclose(file);
    wav_data->valid = 1;
    return 0;
}

// 写入WAV文件
int wav_write(const char *filename, float **channels, int num_channels, 
              int num_samples, int sample_rate) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        printf("Error: Cannot create WAV file: %s\n", filename);
        return -1;
    }
    
    // 构造头部
    WavHeader header;
    int bits_per_sample = 16;
    int byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
    int block_align = num_channels * (bits_per_sample / 8);
    int data_size = num_samples * num_channels * (bits_per_sample / 8);
    
    memcpy(header.chunk_id, "RIFF", 4);
    header.chunk_size = 36 + data_size;
    memcpy(header.format, "WAVE", 4);
    memcpy(header.subchunk1_id, "fmt ", 4);
    header.subchunk1_size = 16;
    header.audio_format = 1;  // PCM
    header.num_channels = num_channels;
    header.sample_rate = sample_rate;
    header.byte_rate = byte_rate;
    header.block_align = block_align;
    header.bits_per_sample = bits_per_sample;
    memcpy(header.subchunk2_id, "data", 4);
    header.subchunk2_size = data_size;
    
    // 写入头部
    fwrite(&header, sizeof(WavHeader), 1, file);
    
    // 写入交织音频数据
    int16_t *buffer = (int16_t *)malloc(num_channels * sizeof(int16_t));
    
    for (int i = 0; i < num_samples; i++) {
        for (int ch = 0; ch < num_channels; ch++) {
            // 浮点数转16位整数
            float sample = channels[ch][i];
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            buffer[ch] = (int16_t)(sample * 32767.0f);
        }
        fwrite(buffer, sizeof(int16_t), num_channels, file);
    }
    
    free(buffer);
    fclose(file);
    
    printf("WAV file written: %s (%d ch, %d samples, %d Hz)\n",
           filename, num_channels, num_samples, sample_rate);
    
    return 0;
}

// 释放内存
void wav_free(WavData *wav_data) {
    if (wav_data->channels) {
        for (int ch = 0; ch < wav_data->num_channels; ch++) {
            if (wav_data->channels[ch]) {
                free(wav_data->channels[ch]);
            }
        }
        free(wav_data->channels);
        wav_data->channels = NULL;
    }
    wav_data->valid = 0;
}
