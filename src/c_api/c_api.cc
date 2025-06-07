//
// Created by liangtao_deng on 2025/3/17.
//

#include "c_api.h"
#include "c_api_define.h"
#include "../KwsPipeline.h"



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
    predictor->impl.RunAEC(mic,ref,res);
};

void SL_EchoNoiseCancelForWav1C16khz(SL_AudioProcesser *predictor,short *in,short *out){
    predictor->impl.RunNS(out,in);
};

int SL_AudioProcessFor8Khz(SL_AudioProcesser *predictor,short *in,short *ref,short *out){
    int code = predictor->impl.Run_Aec_Ns(in,ref);
//    printf("code:%d\n",code);
    if (code != -2){

        auto x = predictor->impl.getOutputs();
        for (int i = 0; i < predictor->impl.CaffeLens; ++i) {
            out[i] = x[i];

        }
        predictor->impl.ReSetNsOutAudioCaffe();
    }
    return code;
}

void SL_AudioOpenKWS(SL_AudioProcesser *predictor,const char *model_path,const char *token_file){
    predictor->impl.kws(model_path, token_file);
}


void SL_AudioKillKWS(SL_AudioProcesser *predictor){
    predictor->impl.killkws();
}


int SL_Audio_kws_ns(SL_AudioProcesser *predictor,short *audio){
    int code = predictor->impl.run_kws_ns(audio);
    printf("code:%d",code);
    return code;
}


int SL_Audio_signle_kws(SL_AudioProcesser *predictor,short *audio){
    int code = predictor->impl.run_kws(audio);
    return code;
}

int SL_Audio_demo(SL_AudioProcesser *predictor,short *in,short *out,int vadcode){
    int code = predictor->impl.demo(in,vadcode);
    printf("code:%d\n",code);
    if (code != -2){
        auto x = predictor->impl.getOutputs();
        for (int i = 0; i < predictor->impl.CaffeLens; ++i) {
            out[i] = x[i];
        }
        predictor->impl.ReSetNsOutAudioCaffe();
    }
    return code;
}
