//
// Created by hideeee on 2025/3/28.
//

#ifndef NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H
#define NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H

#include <iostream>
#include <memory>
#include "nkf/neural_karlman_filter.h"
#include "ns/denoise.h"
#include <sphinxbase/ad.h>
#include <sphinxbase/err.h>
#include <sphinxbase/cmd_ln.h>
#include <pocketsphinx.h>

class AudioProcess {
public:
    const int AEC_BLOCK_SHIFT =  512;
    const int Ns_BLOCK_WINDOWS = (40<<2);
    const int CaffeLens = 2560;
    const int kwschunk = 2048;

    const char *hmm_path = "./model-cn-5.2/zh_cn.cd_cont_5000";
    const char *lm_path  = "./xiaosong_kws_data/9445.lm";
    const char *dict_path = "./xiaosong_kws_data/9445.dic";

    vector <short> NkfOutAudioCaffe;
    vector <short> NsOutAudioCaffe;
public:
    AudioProcess(){};
    ~AudioProcess(){};

    void Init(const char *model_path){
        nkfProcessor = std::make_shared<NKFProcessor>();
        nsProcessor =  std::make_shared<NosieCancel>();
        nkfProcessor->Aec_Init(model_path);
        if(enable_use_kws_ ){
            // 参数配置
            config = cmd_ln_init(nullptr, (const arg_t *) ps_args(), TRUE,
                                 "-hmm", hmm_path,
                                 "-lm", lm_path,
                                 "-dict", dict_path,
                                 "-samprate", "16000",
                                 "-logfn", "/dev/null",
                                 NULL);
            if (config == nullptr) {
//                fprintf(stderr, "Failed to create config object.\n");
                  LOGD("Failed to create config object.\n");
            }

            // 初始化识别器
            ps = ps_init(config);
            if (ps == nullptr) {
                LOGD("Failed to create recognizer.\n");
                fprintf(stderr, "Failed to create recognizer.\n");

            }

        }


    }

    short* RunAEC(short *mic,short *ref){
        nkfProcessor->enhance(mic,ref);
        auto out = nkfProcessor->getoutput();
        return out;
    }

    void ResetAEC(){
        nkfProcessor->reset();
    }

    float RunNS(short* out,short* in){
        float prob = nsProcessor->rnnoise_process_frame(out,in);
        return prob;
    }

    static void remove_front_n(std::vector<short>& vec, size_t n) {
        if (n == 0 || vec.empty()) return;

        if (n >= vec.size()) {
            vec.clear();
            return;
        }

        std::move(vec.begin() + n, vec.end(), vec.begin());
        vec.resize(vec.size() - n);
    }

    int Run_Aec_Ns(short *mic,short *ref,short *outdata){
//      输入 512
        nkfProcessor->enhance(mic,ref);
        auto nkfout = nkfProcessor->getoutput();
        for (int i = 0; i < AEC_BLOCK_SHIFT; ++i) {
            NkfOutAudioCaffe.push_back(nkfout[i]);
        }
        nkfProcessor->reset();
        size_t N = NkfOutAudioCaffe.size()/Ns_BLOCK_WINDOWS;
        size_t M = NkfOutAudioCaffe.size() % Ns_BLOCK_WINDOWS;

        for (int i = 0; i < N; ++i) {
            short NSINPUT[160] = {0};
            short NSOUTPUT[160] = {0};
            for (int j = 0; j < Ns_BLOCK_WINDOWS; ++j) {
                NSINPUT[j] = NkfOutAudioCaffe[j+i*Ns_BLOCK_WINDOWS];
            }
            float prob = nsProcessor->rnnoise_process_frame(NSOUTPUT,NSINPUT);
            for (int j = 0; j < Ns_BLOCK_WINDOWS; ++j) {
                NsOutAudioCaffe.push_back(NSOUTPUT[j]);
            }
        }
        remove_front_n(NkfOutAudioCaffe,N*Ns_BLOCK_WINDOWS);

        if( M == 0){
            if (enable_use_kws_){
                int silencecode = simple_vad_int16_2560(NsOutAudioCaffe.data(),NsOutAudioCaffe.size());
                if(silencecode == 1){
                    ps_start_utt(ps);
                    ps_process_raw(ps, NsOutAudioCaffe.data(), NsOutAudioCaffe.size(), FALSE, FALSE);
                    return 2;
//                  识别到语音，准备唤醒检测
                }
            }

            return 1;
        } else{
            return 0;
        }
    }

    // 简单能量 + 过零率判断
    static int simple_vad_int16_2560(const short *input, int len) {
        if (len != 2560) return 0;

        double energy = 0.0;
        int zero_crossings = 0;

        for (int i = 0; i < len; i++) {
            energy += input[i] * input[i];  // 累加能量
            if (i > 0 && ((input[i - 1] >= 0 && input[i] < 0) || (input[i - 1] < 0 && input[i] >= 0))) {
                zero_crossings++;  // 过零点检测
            }
        }

        energy /= len;  // 均方能量
        float zcr = (float)zero_crossings / len;

        // === 阈值可调 ===
        if (energy > 500000.0 && zcr > 0.01f && zcr < 0.2f) {
            return 1; // 有语音
        } else {
            return 0; // 无语音
        }
    }

    short *  getOutputs(){
        return NsOutAudioCaffe.data();
    }

    void ReSetNsOutAudioCaffe(){
        NsOutAudioCaffe.clear();
    }

    void kws(){
        enable_use_kws_ = true;
    }
private:
    // 回声消除实例
    std::shared_ptr<NKFProcessor> nkfProcessor;
    // ns
    std::shared_ptr<NosieCancel> nsProcessor;

    bool enable_use_kws_{false};

    cmd_ln_t *config ;
    ps_decoder_t *ps;
    
};


#endif //NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H
