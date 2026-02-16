#ifndef WAV_IO_H
#define WAV_IO_H

#include <stdint.h>

// WAV文件头结构体
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

// WAV文件数据结构
typedef struct {
    WavHeader header;
    float **channels;        // 每个通道的数据 (归一化到[-1, 1])
    int num_channels;
    int num_samples;
    int sample_rate;
    int valid;               // 文件是否有效
} WavData;

// 函数声明

/**
 * 读取多通道WAV文件
 * @param filename 文件路径
 * @param wav_data 输出的WAV数据结构
 * @return 0=成功, -1=失败
 */
int wav_read(const char *filename, WavData *wav_data);

/**
 * 写入多通道WAV文件
 * @param filename 文件路径
 * @param channels 通道数据数组
 * @param num_channels 通道数
 * @param num_samples 每通道样本数
 * @param sample_rate 采样率
 * @return 0=成功, -1=失败
 */
int wav_write(const char *filename, float **channels, int num_channels, 
              int num_samples, int sample_rate);

/**
 * 释放WAV数据内存
 * @param wav_data WAV数据结构
 */
void wav_free(WavData *wav_data);

/**
 * 检查WAV文件是否存在
 * @param filename 文件路径
 * @return 1=存在, 0=不存在
 */
int wav_file_exists(const char *filename);

#endif // WAV_IO_H
