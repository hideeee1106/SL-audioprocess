//
// Created by liangtao_deng on 2025/3/17.
//

#include "c_api.h"
#include "c_api_define.h"

#if 0
SL_EchoCancelFilter * SL_CreateEchoCancelFilter(const char *model_path){
    auto *predict = new SL_EchoCancelFilter();
    predict->impl.Aec_Init(model_path);

    return predict;
};

void SL_ReleaseEchoCancelFilter(SL_EchoCancelFilter *predictor){
    {
        delete predictor;
    }
};

void SL_EchoCancelFilterForWav1C16khzSingle(SL_EchoCancelFilter *predictor,float *mic,float *ref,float* res){
    predictor->impl.enhance(mic,ref);
    auto out = predictor->impl.getoutput();
    for (int i = 0; i < 256; ++i) {
        res[i] = out[i];
    }
    predictor->impl.reset();
};
#endif


SL_AudioProcesser * SL_CreateAudioProcesser(const char *model_path){
    auto *predict = new SL_AudioProcesser();
    predict->impl.AECInit(model_path);
    return predict;
};

void SL_ReleaseAudioProcesser(SL_AudioProcesser *predictor){
    {
        delete predictor;
    }
};

void SL_EchoCancelFilterForWav1C16khz(SL_AudioProcesser *predictor,float *mic,float *ref,float * res){

    auto out = predictor->impl.RunAEC(mic,ref);


    for (int i = 0; i < predictor->impl.AEC_BLOCK_SHIFT; ++i) {
        res[i] = out[i];
    }
    predictor->impl.ResetAEC();
};

void SL_EchoNoiseCancelForWav1C16khz(SL_AudioProcesser *predictor,float *in,float *out){
    predictor->impl.RunNS(out,in);
};

