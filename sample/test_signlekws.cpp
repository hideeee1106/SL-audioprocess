//
// Created by hideeee on 2025/4/30.
//
//
// Created by hideeee on 2025/4/29.

#define DR_MP3_IMPLEMENTATION
#define DR_WAV_IMPLEMENTATION
//#define FRAME_SIZE 160
#include <cstdio>
#include <ctgmath>
#include "../src/c_api/c_api.h"
#include "../src/c_api/c_api_define.h"
#include <iostream>
#include "../src/ns/dr_mp3.h"
#include "../src/ns/dr_wav.h"
#include "AudioFile.h"


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

int main(int argc, char *argv[]){


    char *in_file = argv[1];;
    char *model_path = argv[2];


    uint32_t micsampleRate = 0;
    uint64_t micsampleCount = 0;
    uint32_t micchannels = 0;
    float* micbuffer = wavRead_f32(in_file, &micsampleRate, &micsampleCount, &micchannels);
    printf("samplerate:%d\n",micsampleRate);
    vector<short> micinput(micsampleCount);
    for (int i = 0; i < micsampleCount; ++i) {
        micinput[i] = short (micbuffer[i]);

    }
    size_t frames = micsampleCount / 2560;
    printf("count:%zu\n",frames);

    SL_AudioProcesser* filter = SL_CreateAudioProcesser(model_path);
    SL_AudioOpenKWS(filter,argv[3],
                    argv[4]);

//    for (int i = 0; i < 100; ++i) {
//        printf("daya == %d\n",micinput[i]);
//    }

    int code;
    for (int i = 0; i < frames; ++i) {

        code = SL_Audio_signle_kws(filter,&micinput[i*2560]);
//        printf("i= %d,mic == %d\n",i,micinput[i*2560]);
    }
    SL_AudioKillKWS(filter);

    SL_ReleaseAudioProcesser(filter);
    return 0;

}
