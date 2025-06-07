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
#include "agc/agc.h"
#include <chrono>


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

class AudioProcess {
public:
    const int AEC_BLOCK_SHIFT = 160;
    const int Ns_BLOCK_WINDOWS = (40 << 2);
    const int CaffeLens = 5120;
    const int Samplerate = 16000;
//    2560/16000 = 160ms

#if pocketsphinxkws
    const char *hmm_path = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/models/model-cn-5.2/zh_cn.cd_cont_5000";
    const char *lm_path  = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/models/xiaosong_kws_data/9445.lm";
    const char *dict_path = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/models/xiaosong_kws_data/9445.dic";
#endif
    vector<short> NsOutAudioCaffe;
public:
    AudioProcess() {}

    ~AudioProcess() {
        WebRtcAecm_Free(aecmInst);
        WebRtcAgc_Free(agcInst);
    }

    void Init() {
        nsProcessor = std::make_shared<NosieCancel>();
//       webrtc aec
        config.cngMode = AecmTrue;
        config.echoMode = 3;// 0, 1, 2, 3 (default), 4
        aecmInst = WebRtcAecm_Create();
        if (aecmInst == NULL)  LOGD("WebRtcAecm_Init fail\n");
        int status = WebRtcAecm_Init(aecmInst, Samplerate);//8000 or 16000 Sample rate
        if (status != 0) {
            LOGD("WebRtcAecm_Init fail\n");
            WebRtcAecm_Free(aecmInst);
        }
        status = WebRtcAecm_set_config(aecmInst, config);
        if (status != 0) {
            LOGD("WebRtcAecm_set_config fail\n");
            WebRtcAecm_Free(aecmInst);
        }

    //webrtc agc
        agcConfig.compressionGaindB = 15; // default 9 dB
        agcConfig.limiterEnable = 1; // default kAgcTrue (on)
        agcConfig.targetLevelDbfs = 1; // default 3 (-3 dBOv)
        int minLevel = 0;
        int maxLevel = 255;
        short agcMode = kAgcModeAdaptiveDigital;
        agcInst = WebRtcAgc_Create();
        if (agcInst == NULL)  LOGD("WebRtcAgc_Init fail\n");
        status = WebRtcAgc_Init(agcInst, minLevel, maxLevel, agcMode, 16000);
        if (status != 0) {
            LOGD("WebRtcAgc_Init fail\n");
            WebRtcAgc_Free(agcInst);
        }
        status = WebRtcAgc_set_config(agcInst, agcConfig);
        if (status != 0) {
            LOGD("WebRtcAgc_set_config fail\n");
            WebRtcAgc_Free(agcInst);
        }

    }

    int RunAEC(short *mic, short *ref,short *out_buffer) {
        if (WebRtcAecm_BufferFarend(aecmInst, ref, AEC_BLOCK_SHIFT) != 0) {
            LOGD("WebRtcAecm_BufferFarend() failed.\n");
            WebRtcAecm_Free(aecmInst);
            return -1;
        }

        int nRet = WebRtcAecm_Process(aecmInst, mic, NULL, out_buffer, AEC_BLOCK_SHIFT, msInSndCardBuf);

        if (nRet != 0) {
            LOGD("failed in WebRtcAecm_Process\n");
            WebRtcAecm_Free(aecmInst);
            return -1;
        }
        return 1;

    }

    void releaseAEC() {

    }


    int RunAGC(short *input) {
        size_t num_bands = 1;
        int inMicLevel = 0;
        int outMicLevel = -1;
        int16_t out_buffer[320];
        int16_t *out16 = out_buffer;
        uint8_t saturationWarning = 1;                 //是否有溢出发生，增益放大以后的最大值超过了65536
        int16_t echo = 0;                                 //增益放大是否考虑回声影响
        int nAgcRet = WebRtcAgc_Process(agcInst, (const int16_t *const *) &input, num_bands, 160,
                                                (int16_t *const *) &out16, inMicLevel, &outMicLevel, echo,
                                                &saturationWarning);

        if (nAgcRet != 0) {
            printf("failed in WebRtcAgc_Process\n");
            WebRtcAgc_Free(agcInst);
            return -1;
        }
        return 1;
    }

    float RunNS(short *out, short *in) {
        float prob = nsProcessor->rnnoise_process_frame(out, in);
        return prob;
    }

     int Run_ALL(short *mic, short *ref) {
        using namespace std::chrono;
        auto start = high_resolution_clock::now();


//      输入 5120  SHORT 音频
        for (int i=0;i<32;i++) {
            short nkfout[160];
            RunAEC(mic+i*160,ref+i*160, nkfout);

            short nsout[160];
            float prob = nsProcessor->rnnoise_process_frame(nsout, nkfout);
            RunAGC(nsout);

            for (int j = 0; j < Ns_BLOCK_WINDOWS; ++j) {
                NsOutAudioCaffe.push_back(nsout[j]);
            }

        }
        //
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start);
        std::cout << "耗时: " << duration.count() << " ms" << std::endl;




#if fsmnkws
        if( NsOutAudioCaffe.size() == CaffeLens){
            if (enable_use_kws_){
                std::vector<int16_t>kwswavdata(CaffeLens);
                for (int i = 0; i < CaffeLens; ++i) {
                    kwswavdata[i] = static_cast<int16_t>(NsOutAudioCaffe[i]);
                }
               int code = kwspoint->run(kwswavdata);
               return code;
            }
            return -1;
        }return -2;
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
        }return -2;
    }

    int demo(short *mic,short *ref,int vad){
        //      输入 5120  SHORT 音频
        for (int i=0;i<32;i++) {
            short nkfout[160];
            RunAEC(mic+i*160,ref+i*160, nkfout);

            short nsout[160];
            float prob = nsProcessor->rnnoise_process_frame(nsout, nkfout);
            RunAGC(nsout);

            for (int j = 0; j < Ns_BLOCK_WINDOWS; ++j) {
                NsOutAudioCaffe.push_back(nsout[j]);
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
            if(code == 2){
                in_speech = false;
            }
            return code;
        }
        return -2;
    }

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
    AecmConfig config;
    int16_t msInSndCardBuf = 30;
    void *aecmInst;

// agc
    WebRtcAgcConfig agcConfig;
    void *agcInst;
    // ns
    std::shared_ptr<NosieCancel> nsProcessor;

    bool in_speech{false};
    bool enable_use_kws_{false};
    int count = 0;
    int last_voice_count = 0;

#if fsmnkws
    std::shared_ptr<KwsPipeline> kwspoint;
#endif

#if pocketsphinxkws
    ps_config_t *config = nullptr;
    ps_decoder_t *ps = nullptr;

#endif

};

#endif //NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H
