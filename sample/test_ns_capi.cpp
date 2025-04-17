//
// Created by hideeee on 2025/4/9.
//
#define DR_MP3_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION
#define FRAME_SIZE 160
#include <cstdio>
#include <ctgmath>
#include "../src/c_api/c_api.h"
#include "../src/c_api/c_api_define.h"
#include <iostream>
#include "../src/ns/dr_mp3.h"
#include "../src/ns/dr_wav.h"
#include "AudioFile.h"
#define NN 160

#ifndef nullptr
#define nullptr 0
#endif
#ifndef MIN
#define MIN(A, B)        ((A) < (B) ? (A) : (B))
#endif

#define LOW_COEF 1


float *wavRead_f32(const char *filename, uint32_t *sampleRate, uint64_t *sampleCount, uint32_t *channels) {
    drwav_uint64 totalSampleCount = 0;
    float *input = drwav_open_file_and_read_pcm_frames_f32(filename, channels, sampleRate, &totalSampleCount);
    if (input == NULL) {
        drmp3_config pConfig;
        input = drmp3_open_file_and_read_f32(filename, &pConfig, &totalSampleCount);
        if (input != NULL) {
            *channels = pConfig.outputChannels;
            *sampleRate = pConfig.outputSampleRate;
        }
    }
    if (input == NULL) {
        fprintf(stderr, "read file [%s] error.\n", filename);
        exit(1);
    }
    *sampleCount = totalSampleCount * (*channels);
    for (int32_t i = 0; i < *sampleCount; ++i) {
        input[i] = input[i] * 32768.0f;
    }
    return input;
}


void wavWrite_f32_to_int16(char *filename, const short *buffer, int sampleRate, uint32_t totalSampleCount, uint32_t channels) {
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = channels;
    format.sampleRate = (drwav_uint32) sampleRate;
    format.bitsPerSample = 16;
    int16_t *newbuffer = (int16_t *)malloc(sizeof(int16_t) * (totalSampleCount - FRAME_SIZE));



    for (int32_t i = FRAME_SIZE; i < totalSampleCount; ++i) {
        //buffer[i] = drwav_clamp(buffer[i], -32768, 32767) * (1.0f / 32768.0f);
        newbuffer[i-FRAME_SIZE] = (int16_t)drwav_clamp(buffer[i], -32768, 32767);
    }
    drwav *pWav = drwav_open_file_write(filename, &format);
    if (pWav) {
        drwav_uint64 samplesWritten = drwav_write(pWav, totalSampleCount-FRAME_SIZE, newbuffer);
        drwav_uninit(pWav);
        if (samplesWritten != totalSampleCount-FRAME_SIZE) {
            fprintf(stderr, "write file [%s] error.\n", filename);
            exit(1);
        }
    }
}

void ExportWAV(
        const std::string & Filename,
        const std::vector<float>& Data,
        unsigned SampleRate) {
    AudioFile<float>::AudioBuffer Buffer;
    Buffer.resize(1);

    Buffer[0] = Data;
    size_t BufSz = Data.size();

    AudioFile<float> File;
    File.setAudioBuffer(Buffer);
    File.setAudioBufferSize(1, (int)BufSz);
    File.setNumSamplesPerChannel((int)BufSz);
    File.setNumChannels(1);
    File.setBitDepth(16);
    File.setSampleRate(8000);
    File.save(Filename, AudioFileFormat::Wave);
}

int main(int argc, char *argv[]){
    if (argc < 3){
        printf("Usage:./testcapi [inputWav] [RNNnoise_output] \n");
        return -1;
    }
    char *in_file = argv[1];
    if (argc == 4) {
        printf("Start doing noise supreesion\n");

        char *out_file = argv[2];
        char *model_path = argv[3];


        uint32_t sampleRate = 0;
        uint64_t sampleCount = 0;
        uint32_t channels = 0;

        float* buffer = wavRead_f32(in_file, &sampleRate, &sampleCount, &channels);
        vector<short> input(sampleCount);
        for (int i = 0; i < sampleCount; ++i) {
            input[i] = short (buffer[i]);
        }

        vector<float> outputdata;
        size_t frames = sampleCount / NN;


        SL_AudioProcesser* filter = SL_CreateAudioProcesser(model_path);

        for (int i = 0; i < frames; ++i) {
            short out[160] = {0};
            SL_EchoNoiseCancelForWav1C16khz(filter,&input[i*160],out);

            for (short j : out) {
                printf("%d\n",j);
                outputdata.push_back(float(j)/32768.0);
            }
        }

        printf("Finished RNNnoise Noise Supression \n");

        ExportWAV(out_file,outputdata,sampleRate);
        SL_ReleaseAudioProcesser(filter);
        return 0;
    } else {
        printf("Usage:./rnn_gao [inputWav] [outputWav]\n");
    }
}
