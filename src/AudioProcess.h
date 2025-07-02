//
// Created by hideeee on 2025/3/28.
//

#ifndef NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H
#define NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H

#include <iostream>
#include <memory>
#include <sys/syslog.h>
#include "nkf/neural_karlman_filter.h"
#include "ns/denoise.h"
#include "aecm/echo_control_mobile.h"

#define fsmnkws 1

#if fsmnkws
#include "KwsPipeline.h"
#endif


class AudioProcess {
public:
    const int AEC_BLOCK_SHIFT = 512;
    const int Ns_BLOCK_WINDOWS = (40 << 2);
    const int CaffeLens = 5120;

    vector<short> NkfOutAudioCaffe;
    vector<short> NsOutAudioCaffe;
public:
    AudioProcess() {
    };

    ~AudioProcess() {};

    void Init(const char *model_path) {
        nkfProcessor = std::make_shared<NKFProcessor>();
        nsProcessor = std::make_shared<NosieCancel>();
        nkfProcessor->Aec_Init(model_path);
    }

    void *RunAEC(short *mic, short *ref,short* out) {

        for (int i = 0; i < 10; ++i) {
            nkfProcessor->enhance(mic+512*i, ref+512*i);
            for (int j = 0; j < 512; ++j) {
                out[i*512+j] = nkfProcessor->outputbuffer[i];
            }
            nkfProcessor->reset();
        }
    }

    void ResetAEC() {
        nkfProcessor->reset();
    }

    float RunNS(short *out, short *in) {
        float prob = nsProcessor->rnnoise_process_frame(out, in);
        return prob;
    }

    static void remove_front_n(std::vector<short> &vec, size_t n) {
        if (n == 0 || vec.empty()) return;

        if (n >= vec.size()) {
            vec.clear();
            return;
        }

        std::move(vec.begin() + n, vec.end(), vec.begin());
        vec.resize(vec.size() - n);
    }

    // 简单能量 + 过零率判断
    static int simple_vad_int16_5120(const short *input, int len) {
        if (len != 5120) return 0;

        double energy = 0.0;
        int zero_crossings = 0;

        for (int i = 0; i < len; i++) {
            energy += input[i] * input[i];  // 累加能量
            if (i > 0 && ((input[i - 1] >= 0 && input[i] < 0) || (input[i - 1] < 0 && input[i] >= 0))) {
                zero_crossings++;  // 过零点检测
            }
        }

        energy /= len;  // 均方能量
        float zcr = (float) zero_crossings / len;
//        printf("energy:%f,zcr:%f\n",energy,zcr);
        // === 阈值可调 ===
        if (energy > 50000.0) {
            return 1; // 有语音
        } else {
            return 0; // 无语音
        }
    }

    int run_kws(const short *wav){
        std::vector<int16_t>kwswavdata(CaffeLens);
        for (int i = 0; i < CaffeLens; ++i) {
            kwswavdata[i] = static_cast<int16_t>(wav[i]);

        }
        int code = kwspoint->run(kwswavdata);

        return code;
    }


    int run_kws_ns(short *audio){
//        printf("run kws ns\n");
        for (int i = 0; i < 32; ++i) {
            short out[160] = {0};
            float prob = nsProcessor->rnnoise_process_frame(out,audio+i*160);
            for (int j = 0; j < Ns_BLOCK_WINDOWS; ++j) {
                NsOutAudioCaffe.push_back(out[j]);
            }
        }

        if(NsOutAudioCaffe.size() == 5120){
            std::vector<int16_t>kwswavdata(CaffeLens);
            for (int i = 0; i < CaffeLens; ++i) {
                kwswavdata[i] = static_cast<int16_t>(NsOutAudioCaffe[i]);
            }
            NsOutAudioCaffe.clear();
            int code = kwspoint->run(kwswavdata);
            return code;
        } else{
            return -2;
        }
    }

    int demo(short *audio,int vad){
//        512
        for (int i = 0; i < 32; ++i) {
            short out[160] = {0};
            float prob = nsProcessor->rnnoise_process_frame(out,audio+i*160);
            for (int j = 0; j < Ns_BLOCK_WINDOWS; ++j) {
                NsOutAudioCaffe.push_back(out[j]);
            }
        }

        if(NsOutAudioCaffe.size() == 5120){

            int silence = simple_vad_int16_5120(&NsOutAudioCaffe[0],5120);
            if(silence == 1){
                in_speech = true;
            }
            printf("vad code = %d,silence = %d,in_speech = %d\n",vad,silence,in_speech);
            if(vad and in_speech){
                count = count + 1 ;
                if(silence == 1){
                    last_voice_count = count;
                }
                printf("last_voice_count = %d,count = %d\n",last_voice_count,count);
                if (silence == 0 and count - last_voice_count >= 2){
                    count = 0;
                    last_voice_count = 0;
                    in_speech = false;
                    return 3;
                }
            }

            std::vector<int16_t>kwswavdata(CaffeLens);
            for (int i = 0; i < CaffeLens; ++i) {
                kwswavdata[i] = static_cast<int16_t>(NsOutAudioCaffe[i]);

            }
            NsOutAudioCaffe.clear();
            int code = kwspoint->run(kwswavdata);
            if(code == 1){
                in_speech = false;
            }
            return code;
        } else{
            return -2;
        }

    }

    int Run_Aec_Ns(short *mic, short *ref) {
//      输入 512  SHORT 音频

        nkfProcessor->enhance(mic, ref);
        auto nkfout = nkfProcessor->getoutput();
        for (int i = 0; i < AEC_BLOCK_SHIFT; ++i) {
            NkfOutAudioCaffe.push_back(nkfout[i]);
        }
        nkfProcessor->reset();
        size_t N = NkfOutAudioCaffe.size() / Ns_BLOCK_WINDOWS;
        size_t M = NkfOutAudioCaffe.size() % Ns_BLOCK_WINDOWS;

        for (int i = 0; i < N; ++i) {
            short NSINPUT[160] = {0};
            short NSOUTPUT[160] = {0};
            for (int j = 0; j < Ns_BLOCK_WINDOWS; ++j) {
                NSINPUT[j] = NkfOutAudioCaffe[j + i * Ns_BLOCK_WINDOWS];
            }
            float prob = nsProcessor->rnnoise_process_frame(NSOUTPUT, NSINPUT);
            for (int j = 0; j < Ns_BLOCK_WINDOWS; ++j) {
                NsOutAudioCaffe.push_back(NSOUTPUT[j]);
            }
        }
        remove_front_n(NkfOutAudioCaffe, N * Ns_BLOCK_WINDOWS);

#if fsmnkws
        if( M == 0){
            if (enable_use_kws_){
                auto start_1=std::chrono::high_resolution_clock::now();
                std::vector<int16_t>kwswavdata(CaffeLens);
                for (int i = 0; i < CaffeLens; ++i) {
                    kwswavdata[i] = static_cast<int16_t>(NsOutAudioCaffe[i]);
                }

               int code = kwspoint->run(kwswavdata);
               return code;
            }
            return -1;
        } else{
            return -2;
        }
#endif
    };

    short *getOutputs() {
        return NsOutAudioCaffe.data();
    }

    void ReSetNsOutAudioCaffe() {
        NsOutAudioCaffe.clear();
    }


#if fsmnkws
    void kws(const std::string& model_path, const std::string& token_file){
        kwspoint = std::make_shared<KwsPipeline>(model_path,token_file);
        enable_use_kws_ = true;
    };
    void killkws(){
        enable_use_kws_ = false;
        kwspoint.reset();
    }


#endif

private:
    // 回声消除实例
    std::shared_ptr<NKFProcessor> nkfProcessor;
    // ns
    std::shared_ptr<NosieCancel> nsProcessor;

    bool in_speech{false};
    bool enable_use_kws_{false};
    int count = 0;
    int last_voice_count = 0;

#if fsmnkws
    std::shared_ptr<KwsPipeline> kwspoint;
#endif

};

#endif //NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H
