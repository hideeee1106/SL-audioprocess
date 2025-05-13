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

#if fsmnkws
#include "KwsPipeline.h"
#endif

#if pocketsphinxkws
extern "C" {
    #include <pocketsphinx.h>
}
#endif

class AudioProcess {
public:
    const int AEC_BLOCK_SHIFT = 512;
    const int Ns_BLOCK_WINDOWS = (40 << 2);
    const int CaffeLens = 5120;
//    2560/16000 = 160ms

#if pocketsphinxkws
    const char *hmm_path = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/models/model-cn-5.2/zh_cn.cd_cont_5000";
    const char *lm_path  = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/models/xiaosong_kws_data/9445.lm";
    const char *dict_path = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/models/xiaosong_kws_data/9445.dic";
#endif
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

    short *RunAEC(short *mic, short *ref) {

        auto start=std::chrono::high_resolution_clock::now();

        nkfProcessor->enhance(mic, ref);
        auto out = nkfProcessor->getoutput();

        auto end=std::chrono::high_resolution_clock::now();
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end-start);
        int time=static_cast<int>(milliseconds.count());
        std::cout<<"回声消除时间 "<<time<<" :ms"<<std::endl;
        return out;
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
            } else{
                return -1;
            }
        } else{
            return -2;
        }
#endif

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


    };

    short *getOutputs() {
        return NsOutAudioCaffe.data();
    }

    void ReSetNsOutAudioCaffe() {
        NsOutAudioCaffe.clear();
    }


#if pocketsphinxkws
    void resetkws(){
            count = 0;
            in_speech = false;
            wait_count = 0;
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
#elif fsmnkws
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
//    const int MAX_SPEECH_TIME = 31;
//    31 *0.16 = 5s

#if fsmnkws
    std::shared_ptr<KwsPipeline> kwspoint;
#endif

#if pocketsphinxkws
    ps_config_t *config = nullptr;
    ps_decoder_t *ps = nullptr;

#endif

};

#endif //NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H
