//
// Created by hideeee on 2025/3/28.
//

#include "denoise.h"
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

void compute_band_energy(float *bandE, const kiss_fft_cpx *X) {
    int i;
    float sum[NB_BANDS] = {0};
    for (i=0;i<NB_BANDS-1;i++)
    {
        int j;
        int band_size;
        //printf(eband5ms[i]);
        band_size = (eband5ms[i+1]-eband5ms[i])<<FRAME_SIZE_SHIFT;
        for (j=0;j<band_size;j++) {
            float tmp;
            float frac = (float)j/band_size;
            tmp = SQUARE(X[(eband5ms[i]<<FRAME_SIZE_SHIFT) + j].r);
            tmp += SQUARE(X[(eband5ms[i]<<FRAME_SIZE_SHIFT) + j].i);
            sum[i] += (1-frac)*tmp;
            sum[i+1] += frac*tmp;
        }
    }
    sum[0] *= 2;
    sum[NB_BANDS-1] *= 2;
    for (i=0;i<NB_BANDS;i++)
    {
        bandE[i] = sum[i];
    }
}

void compute_band_corr(float *bandE, const kiss_fft_cpx *X, const kiss_fft_cpx *P) {
    int i;
    float sum[NB_BANDS] = {0};
    for (i=0;i<NB_BANDS-1;i++)
    {
        int j;
        int band_size;
        band_size = (eband5ms[i+1]-eband5ms[i])<<FRAME_SIZE_SHIFT;
        for (j=0;j<band_size;j++) {
            float tmp;
            float frac = (float)j/band_size;
            tmp = X[(eband5ms[i]<<FRAME_SIZE_SHIFT) + j].r * P[(eband5ms[i]<<FRAME_SIZE_SHIFT) + j].r;
            tmp += X[(eband5ms[i]<<FRAME_SIZE_SHIFT) + j].i * P[(eband5ms[i]<<FRAME_SIZE_SHIFT) + j].i;
            sum[i] += (1-frac)*tmp;
            sum[i+1] += frac*tmp;
        }
    }
    sum[0] *= 2;
    sum[NB_BANDS-1] *= 2;
    for (i=0;i<NB_BANDS;i++)
    {
        bandE[i] = sum[i];
    }
}

void interp_band_gain(float *g, const float *bandE) {
    int i;
    memset(g, 0, FREQ_SIZE);
    for (i=0;i<NB_BANDS-1;i++)
    {
        int j;
        int band_size;
        band_size = (eband5ms[i+1]-eband5ms[i])<<FRAME_SIZE_SHIFT;
        for (j=0;j<band_size;j++) {
            float frac = (float)j/band_size;
            g[(eband5ms[i]<<FRAME_SIZE_SHIFT) + j] = (1-frac)*bandE[i] + frac*bandE[i+1];
        }
    }
}

CommonState common;

static void check_init() {
    int i;
    if (common.init) return;
    common.kfft = opus_fft_alloc_twiddles(2*FRAME_SIZE, NULL, NULL, NULL, 0);
    for (i=0;i<FRAME_SIZE;i++)
        common.half_window[i] = sin(.5*M_PI*sin(.5*M_PI*(i+.5)/FRAME_SIZE) * sin(.5*M_PI*(i+.5)/FRAME_SIZE));
    for (i=0;i<NB_BANDS;i++) {
        int j;
        for (j=0;j<NB_BANDS;j++) {
            common.dct_table[i*NB_BANDS + j] = cos((i+.5)*j*M_PI/NB_BANDS);
            if (j==0) common.dct_table[i*NB_BANDS + j] *= sqrt(.5);
        }
    }
    common.init = 1;
}

static void dct(float *out, const float *in) {
    int i;
    check_init();
    for (i=0;i<NB_BANDS;i++) {
        int j;
        float sum = 0;
        for (j=0;j<NB_BANDS;j++) {
            sum += in[j] * common.dct_table[j*NB_BANDS + i];
        }
        out[i] = sum*sqrt(2./22);
    }
}

static void forward_transform(kiss_fft_cpx *out, const float *in) {
    int i;
    kiss_fft_cpx x[WINDOW_SIZE];
    kiss_fft_cpx y[WINDOW_SIZE];
    check_init();
    for (i=0;i<WINDOW_SIZE;i++) {
        x[i].r = in[i];
        x[i].i = 0;
    }
    opus_fft(common.kfft, x, y, 0);
    for (i=0;i<FREQ_SIZE;i++) {
        out[i] = y[i];
    }
}

static void inverse_transform(float *out, const kiss_fft_cpx *in) {
    int i;
    kiss_fft_cpx x[WINDOW_SIZE];
    kiss_fft_cpx y[WINDOW_SIZE];
    check_init();
    for (i=0;i<FREQ_SIZE;i++) {
        x[i] = in[i];
    }
    for (;i<WINDOW_SIZE;i++) {
        x[i].r = x[WINDOW_SIZE - i].r;
        x[i].i = -x[WINDOW_SIZE - i].i;
    }
    opus_fft(common.kfft, x, y, 0);
    /* output in reverse order for IFFT. */
    out[0] = WINDOW_SIZE*y[0].r;
    for (i=1;i<WINDOW_SIZE;i++) {
        out[i] = WINDOW_SIZE*y[WINDOW_SIZE - i].r;
    }
}

static void apply_window(float *x) {
    int i;
    check_init();
    for (i=0;i<FRAME_SIZE;i++) {
        x[i] *= common.half_window[i];
        x[WINDOW_SIZE - 1 - i] *= common.half_window[i];
    }
}


static void frame_analysis(DenoiseState *st, kiss_fft_cpx *X, float *Ex, const float *in) {
    int i;
    float x[WINDOW_SIZE];
    RNN_COPY(x, st->analysis_mem, FRAME_SIZE);
    for (i=0;i<FRAME_SIZE;i++) x[FRAME_SIZE + i] = in[i];
    RNN_COPY(st->analysis_mem, in, FRAME_SIZE);
    apply_window(x);
    forward_transform(X, x);
    compute_band_energy(Ex, X);
}

static int compute_frame_features(DenoiseState *st, kiss_fft_cpx *X, kiss_fft_cpx *P,
                                  float *Ex, float *Ep, float *Exp, float *features, const float *in) {
    int i;
    float E = 0;
    float *ceps_0, *ceps_1, *ceps_2;
    float spec_variability = 0;
    float Ly[NB_BANDS];
    float p[WINDOW_SIZE];
    float pitch_buf[PITCH_BUF_SIZE>>1];
    int pitch_index;
    float gain;
    float *(pre[1]);
    float tmp[NB_BANDS];
    float follow, logMax;
    frame_analysis(st, X, Ex, in);
    RNN_MOVE(st->pitch_buf, &st->pitch_buf[FRAME_SIZE], PITCH_BUF_SIZE-FRAME_SIZE);
    RNN_COPY(&st->pitch_buf[PITCH_BUF_SIZE-FRAME_SIZE], in, FRAME_SIZE);
    pre[0] = &st->pitch_buf[0];
    pitch_downsample(pre, pitch_buf, PITCH_BUF_SIZE, 1);
    pitch_search(pitch_buf+(PITCH_MAX_PERIOD>>1), pitch_buf, PITCH_FRAME_SIZE,
                 PITCH_MAX_PERIOD-3*PITCH_MIN_PERIOD, &pitch_index);
    pitch_index = PITCH_MAX_PERIOD-pitch_index;

    gain = remove_doubling(pitch_buf, PITCH_MAX_PERIOD, PITCH_MIN_PERIOD,
                           PITCH_FRAME_SIZE, &pitch_index, st->last_period, st->last_gain);
    st->last_period = pitch_index;
    st->last_gain = gain;
    for (i=0;i<WINDOW_SIZE;i++)
        p[i] = st->pitch_buf[PITCH_BUF_SIZE-WINDOW_SIZE-pitch_index+i];
    apply_window(p);
    forward_transform(P, p);
    compute_band_energy(Ep, P);
    compute_band_corr(Exp, X, P);
    for (i=0;i<NB_BANDS;i++) Exp[i] = Exp[i] * (1.0f / sqrtf(.001f + Ex[i] * Ep[i]));
    dct(tmp, Exp);
    for (i=0;i<NB_DELTA_CEPS;i++) features[NB_BANDS+2*NB_DELTA_CEPS+i] = tmp[i];
    features[NB_BANDS+2*NB_DELTA_CEPS] -= 1.3f;
    features[NB_BANDS+2*NB_DELTA_CEPS+1] -= 0.9f;
    features[NB_BANDS+3*NB_DELTA_CEPS] = .01f*(pitch_index-300);
    logMax = -2;
    follow = -2;
    for (i=0;i<NB_BANDS;i++) {
        Ly[i] = log10(1e-2f+Ex[i]);
        Ly[i] = MAX16(logMax-7, MAX16(follow-1.5f, Ly[i]));
        logMax = MAX16(logMax, Ly[i]);
        follow = MAX16(follow-1.5f, Ly[i]);
        E += Ex[i];
    }
    if (E < 0.04f) {
        /* If there's no audio, avoid messing up the state. */
        RNN_CLEAR(features, NB_FEATURES);
        return 1;
    }
    dct(features, Ly);
    features[0] -= 12;
    features[1] -= 4;
    ceps_0 = st->cepstral_mem[st->memid];
    ceps_1 = (st->memid < 1) ? st->cepstral_mem[CEPS_MEM+st->memid-1] : st->cepstral_mem[st->memid-1];
    ceps_2 = (st->memid < 2) ? st->cepstral_mem[CEPS_MEM+st->memid-2] : st->cepstral_mem[st->memid-2];
    for (i=0;i<NB_BANDS;i++) ceps_0[i] = features[i];
    st->memid++;
    for (i=0;i<NB_DELTA_CEPS;i++) {
        features[i] = ceps_0[i] + ceps_1[i] + ceps_2[i];
        features[NB_BANDS+i] = ceps_0[i] - ceps_2[i];
        features[NB_BANDS+NB_DELTA_CEPS+i] =  ceps_0[i] - 2*ceps_1[i] + ceps_2[i];
    }
    /* Spectral variability features. */
    if (st->memid == CEPS_MEM) st->memid = 0;
    for (i=0;i<CEPS_MEM;i++)
    {
        int j;
        float mindist = 1e15f;
        for (j=0;j<CEPS_MEM;j++)
        {
            int k;
            float dist=0;
            for (k=0;k<NB_BANDS;k++)
            {
                float tmp;
                tmp = st->cepstral_mem[i][k] - st->cepstral_mem[j][k];
                dist += tmp*tmp;
            }
            if (j!=i)
                mindist = MIN32(mindist, dist);
        }
        spec_variability += mindist;
    }
    features[NB_BANDS+3*NB_DELTA_CEPS+1] = spec_variability/CEPS_MEM-2.1f;
    return 0;
}

static void frame_synthesis(DenoiseState *st, float *out, const kiss_fft_cpx *y) {
    float x[WINDOW_SIZE];
    int i;
    inverse_transform(x, y);
    apply_window(x);
    for (i=0;i<FRAME_SIZE;i++) out[i] = x[i] + st->synthesis_mem[i];
    RNN_COPY(st->synthesis_mem, &x[FRAME_SIZE], FRAME_SIZE);
}

static void biquad(float *y, float mem[2], const float *x, const float *b, const float *a, int N) {
    int i;
    for (i=0;i<N;i++) {
        float xi, yi;
        xi = x[i];
        yi = x[i] + mem[0];
        mem[0] = mem[1] + (b[0]*(double)xi - a[0]*(double)yi);
        mem[1] = (b[1]*(double)xi - a[1]*(double)yi);
        y[i] = yi;
    }
}

void pitch_filter(kiss_fft_cpx *X, const kiss_fft_cpx *P, const float *Ex, const float *Ep,
                  const float *Exp, const float *g) {
    int i;
    float r[NB_BANDS];
    float rf[FREQ_SIZE] = {0};
    for (i=0;i<NB_BANDS;i++) {
        if (Exp[i]>g[i]) r[i] = 1;
        else r[i] = SQUARE(Exp[i])*(1-SQUARE(g[i]))/(.001 + SQUARE(g[i])*(1-SQUARE(Exp[i])));
        r[i] = sqrt(MIN16(1, MAX16(0, r[i])));
        r[i] *= sqrt(Ex[i]/(1e-8+Ep[i]));
    }
    interp_band_gain(rf, r);
    for (i=0;i<FREQ_SIZE;i++) {
        X[i].r += rf[i]*P[i].r;
        X[i].i += rf[i]*P[i].i;
    }
    float newE[NB_BANDS];
    compute_band_energy(newE, X);
    float norm[NB_BANDS];
    float normf[FREQ_SIZE]={0};
    for (i=0;i<NB_BANDS;i++) {
        norm[i] = sqrt(Ex[i]/(1e-8+newE[i]));
    }
    interp_band_gain(normf, norm);
    for (i=0;i<FREQ_SIZE;i++) {
        X[i].r *= normf[i];
        X[i].i *= normf[i];
    }
}

float  NosieCancel::rnnoise_process_frame(float *out, const float *in) {
    int i;
    kiss_fft_cpx X[FREQ_SIZE];   // 频域数据（FFT 结果）
    kiss_fft_cpx P[WINDOW_SIZE]; // 自相关函数
    float x[FRAME_SIZE];         // 经过高通滤波后的音频数据
    float Ex[NB_BANDS], Ep[NB_BANDS]; // 频带能量
    float Exp[NB_BANDS];          // 预测误差
    float features[NB_FEATURES];  // 计算出的特征
    float g[NB_BANDS];            // 频带增益
    float gf[FREQ_SIZE]={1};      // 频率增益（插值后）
    float vad_prob = 0;           // VAD 语音概率
    int silence;                  // 是否为静音
    static const float a_hp[2] = {-1.99599, 0.99600};
    static const float b_hp[2] = {-2, 1};
    biquad(x, st->mem_hp_x, in, b_hp, a_hp, FRAME_SIZE);
    silence = compute_frame_features(st, X, P, Ex, Ep, Exp, features, x);

    if (!silence) {
        compute_rnn(&st->rnn, g, &vad_prob, features);
        pitch_filter(X, P, Ex, Ep, Exp, g); //由于会造成截顶，所以不用pitch_filter
        for (i=0;i<NB_BANDS;i++) {
            float alpha = .6f;
            g[i] = MAX16(g[i], alpha*st->lastg[i]);
            st->lastg[i] = g[i];
            if(g[i]>1.0) {
                fprintf(stderr, "check gain ================ %f \n", g[i]);
            }


        }
        interp_band_gain(gf, g);
#if 1
        for (i=0;i<FREQ_SIZE;i++) {
            if (gf[i] > 1.0){
                fprintf(stderr, "check gain gf================ %f \n", gf[i]);
            }

            X[i].r *= gf[i];
            X[i].i *= gf[i];
        }
#endif
    }

    frame_synthesis(st, out, X);
    return vad_prob;
}
