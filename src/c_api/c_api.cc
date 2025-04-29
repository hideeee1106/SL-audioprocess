//
// Created by liangtao_deng on 2025/3/17.
//

#include "c_api.h"
#include "c_api_define.h"
#include "../KwsPipeline.h"
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
    predict->impl.Init(model_path);
    return predict;
};

void SL_ReleaseAudioProcesser(SL_AudioProcesser *predictor){
    {
        delete predictor;
    }
};

void SL_EchoCancelFilterForWav1C16khz(SL_AudioProcesser *predictor,short *mic,short *ref,short * res){

    auto out = predictor->impl.RunAEC(mic,ref);


    for (int i = 0; i < predictor->impl.AEC_BLOCK_SHIFT; ++i) {

        res[i] = out[i];
//        printf("outdata:%f,",res[i]);
    }
    predictor->impl.ResetAEC();
};

void SL_EchoNoiseCancelForWav1C16khz(SL_AudioProcesser *predictor,short *in,short *out){
    predictor->impl.RunNS(out,in);
};

int SL_AudioProcessFor8Khz(SL_AudioProcesser *predictor,short *in,short *ref,short *out){
    int code = predictor->impl.Run_Aec_Ns(in,ref,out);
    if (code != 0){
        printf("code:%d\n",code);
        auto x = predictor->impl.getOutputs();
        for (int i = 0; i < predictor->impl.CaffeLens; ++i) {
            out[i] = x[i];

        }
        predictor->impl.ReSetNsOutAudioCaffe();
    }
    return code;
}

void SL_AudioOpenKWS(SL_AudioProcesser *predictor,const char* hmm_model,const char* dict_model, const char*lm_model){
    predictor->impl.kws(hmm_model,dict_model,lm_model);
}
void SL_AudioKillKWS(SL_AudioProcesser *predictor){
    predictor->impl.killkws();
}




long SL_createKWSPipeline(char *model_path,char *token_file)
{
    KwsPipeline*predictor=new KwsPipeline(model_path,token_file);

    return long (predictor);

}



int SL_StartDetectFromStream(long handle, int16_t *pcm, long len)
{
    KwsPipeline *kws_predictor=(KwsPipeline*)handle;

    std::vector<int16_t>wav(pcm,pcm+len);

    return kws_predictor->run(wav);
}

void SL_releaseKWSPipeline(long handle) {
    KwsPipeline *kws_predictor=(KwsPipeline*)handle;
    delete kws_predictor;

}
