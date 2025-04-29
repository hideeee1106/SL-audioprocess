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

#define pocketsphinxkws 0
#define fsmnkws 1


#if pocketsphinxkws
extern "C" {
    #include <pocketsphinx.h>
}
#endif

class AudioProcess {
public:
    const int AEC_BLOCK_SHIFT =  512;
    const int Ns_BLOCK_WINDOWS = (40<<2);
    const int CaffeLens = 2560;
//    2560/16000 = 160ms

#if pocketsphinxkws
    const char *hmm_path = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/models/model-cn-5.2/zh_cn.cd_cont_5000";
    const char *lm_path  = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/models/xiaosong_kws_data/9445.lm";
    const char *dict_path = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/models/xiaosong_kws_data/9445.dic";
#endif
    vector <short> NkfOutAudioCaffe;
    vector <short> NsOutAudioCaffe;
public:
    AudioProcess(){
    };
    ~AudioProcess(){};

    void Init(const char *model_path){
        nkfProcessor = std::make_shared<NKFProcessor>();
        nsProcessor =  std::make_shared<NosieCancel>();
        nkfProcessor->Aec_Init(model_path);



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
//      输入 512  SHORT 音频
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

#if pocketsphinxkws
        if( M == 0){
            if (enable_use_kws_){

                int silencecode = simple_vad_int16_2560(NsOutAudioCaffe.data(),NsOutAudioCaffe.size());
//                printf("silence:%d\n",silencecode);
                if(silencecode == 1){
                    if(not in_speech){
                        in_speech = true;
                        ps_start_utt(ps);

                    }

                    ps_process_raw(ps, NsOutAudioCaffe.data(), NsOutAudioCaffe.size(), FALSE, FALSE);
                    count = count + 1;
                    if (count > MAX_SPEECH_TIME){
                        LOGD("MAX_SPEECH_TIME");
                        ps_end_utt(ps);
                        const char *hyp = ps_get_hyp(ps, nullptr);
                        resetkws();
                        if (hyp != nullptr) {
                            printf("识别结果：%s\n", hyp);
                            return 3;
                        } else {
                            printf("无识别结果\n");
                            return 4;
                        }


                    }

                    return 2;
//                  识别到语音，准备唤醒检测
                }
                else{
                    if(in_speech){
                        wait_count = wait_count + 1;
                    }

//                    count
                    if(in_speech && wait_count == 3){
                        ps_end_utt(ps);
                        const char *hyp = ps_get_hyp(ps, nullptr);
                        resetkws();
                        if (hyp != nullptr) {
                            printf("识别成功！！！！！！！！！！！，识别结果：%s\n", hyp);
                            return 3;

                        } else {
                            printf("无识别结果\n");
                            return 4;
                        }

                    }

                }
            }

            return 1;
        } else{
            return 0;
        }
#endif

    }



    short *  getOutputs(){
        return NsOutAudioCaffe.data();
    }

    void ReSetNsOutAudioCaffe(){
        NsOutAudioCaffe.clear();
    }


#if pocketsphinxkws
    void resetkws(){
        count = 0;
        in_speech = false;
        wait_count = 0;
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
//        printf("energy:%f,zcr:%f\n",energy,zcr);
        // === 阈值可调 ===
        if (energy > 50000.0) {
            return 1; // 有语音
        } else {
            return 0; // 无语音
        }
    }

    void kws(const char* hmm_model,const char* dict_model, const char*lm_model){
        enable_use_kws_ = true;
        // 参数配置
        config = ps_config_init(NULL);
        ps_default_search_args(config);  // 设置默认参数

        ps_config_set_str(config, "hmm", hmm_model);
        ps_config_set_str(config, "dict", dict_model);
        ps_config_set_str(config, "lm", lm_model);
        ps_config_set_str(config, "loglevel", "INFO");

        if (config == NULL) {
            std::cerr << "Error initializing config!" << std::endl;
            return;
        }
        // 创建解码器
        ps = ps_init(config);
        if (ps == nullptr) {
            fprintf(stderr, "Failed to create decoder\n");
        } else {
            printf("Decoder successfully created.\n");
        }


    }
    void killkws(){
        ps_free(ps);
        ps_config_free(config);
        enable_use_kws_ = false;
    }
#endif

private:
    // 回声消除实例
    std::shared_ptr<NKFProcessor> nkfProcessor;
    // ns
    std::shared_ptr<NosieCancel> nsProcessor;

#if pocketsphinxkws
    bool enable_use_kws_{false};

    bool in_speech{false};

    int count = 0;

    int wait_count = 0;

    const int MAX_SPEECH_TIME = 31;
//    31 *0.16 = 5s

    ps_config_t *config = nullptr;
    ps_decoder_t *ps = nullptr;

#endif

};


#endif //NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H
