//
// Created by hideeee on 2025/3/28.
//

#ifndef NKF_MNN_DEPLOY_R328_DENOSIE_H
#define NKF_MNN_DEPLOY_R328_DENOSIE_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "kiss_fft.h"
#include "common.h"
#include <math.h>
#include "pitch.h"
#include "arch.h"
#include "rnn.h"
#include "rnn_data.h"
#include <dirent.h>
#include <time.h>

#define BLOCK_SIZE 8000
#define FRAME_SIZE_SHIFT 2
// FRAME_SIZE = 160
#define FRAME_SIZE (40<<FRAME_SIZE_SHIFT)
#define WINDOW_SIZE (2*FRAME_SIZE)
#define FREQ_SIZE (FRAME_SIZE + 1)
#define SAMPLE_RATE 16000

/*for 16K speech files*/
#define PITCH_MIN_PERIOD 20
#define PITCH_MAX_PERIOD 256
#define PITCH_FRAME_SIZE 320

#define PITCH_BUF_SIZE (PITCH_MAX_PERIOD+PITCH_FRAME_SIZE)

#define SQUARE(x) ((x)*(x))

#define SMOOTH_BANDS 1

#if SMOOTH_BANDS
#define NB_BANDS 18
#else
#define NB_BANDS 17
#endif

#define CEPS_MEM 8
#define NB_DELTA_CEPS 6

#define NB_FEATURES (NB_BANDS+3*NB_DELTA_CEPS+2)



static const opus_int16 eband5ms[] = {
/*0  200 400 600 800  1k 1.2 1.4 1.6  2k 2.4 2.8 3.2  4k 4.8 5.6 6.8  8k 9.6 12k 15.6 20k*/
        0,  1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 20, 24, 28, 34, 40, 48, 60, 78, 100
};


typedef struct {
    int init;
    kiss_fft_state *kfft;
    float half_window[FRAME_SIZE];
    float dct_table[NB_BANDS*NB_BANDS];
} CommonState;

struct DenoiseState {
    float analysis_mem[FRAME_SIZE];
    float cepstral_mem[CEPS_MEM][NB_BANDS];
    int memid;
    float synthesis_mem[FRAME_SIZE];
    float pitch_buf[PITCH_BUF_SIZE];
    float pitch_enh_buf[PITCH_BUF_SIZE];
    float last_gain;
    int last_period;
    float mem_hp_x[2];
    float lastg[NB_BANDS];
    RNNState rnn;
};

class NosieCancel {

public:
    NosieCancel(){
        st = (DenoiseState*)malloc(sizeof(DenoiseState)); // 分配内存
    };
    ~NosieCancel(){
        free(st);  // 释放内存
    }

    typedef struct DenoiseState DenoiseState;

    float rnnoise_process_frame(short *out, const short *in);

    int getvad();
private:
    DenoiseState* st;
    int silence = 0;                  // 是否为静音

};


#endif //NKF_MNN_DEPLOY_R328_DENOSIE_H
