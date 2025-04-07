//
// Created by hideeee on 2025/3/28.
//

#ifndef NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H
#define NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H

#include <iostream>
#include <memory>
#include "nkf/neural_karlman_filter.h"
#include "ns/denoise.h"

class AudioProcess {
public:
    const int AEC_BLOCK_SHIFT =  256;
    const int Ns_BLOCK_WINDOWS = (40<<2);
public:
    AudioProcess(){};
    ~AudioProcess(){};

    void AECInit(const char *model_path){
        nkfProcessor = std::make_shared<NKFProcessor>();
        nsProcessor =  std::make_shared<NosieCancel>();
        nkfProcessor->Aec_Init(model_path);
    }

    float* RunAEC(float *mic,float *ref){
        nkfProcessor->enhance(mic,ref);
        auto out = nkfProcessor->getoutput();
        return out;
    }

    void ResetAEC(){
        nkfProcessor->reset();
    }

    float RunNS(float* out,float* in){
        float prob = nsProcessor->rnnoise_process_frame(out,in);
        return prob;
    }



private:
    // 回声消除实例
    std::shared_ptr<NKFProcessor> nkfProcessor;
    // ns
    std::shared_ptr<NosieCancel> nsProcessor;

    bool enable_use_aec_{true};

};


#endif //NKF_MNN_DEPLOY_R328_AUDIOPROCESS_H
