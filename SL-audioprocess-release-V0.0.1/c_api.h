//
// Created by liangtao_deng on 2025/3/17.
//

#ifndef NKF_MNN_DEPLOY_R328_C_API_H
#define NKF_MNN_DEPLOY_R328_C_API_H


#include <stdint.h>

#if defined(_WIN32)
#ifdef SL_BUILD_SHARED_LIB
#define SL_CAPI_EXPORT __declspec(dllexport)
#else
#define SL_CAPI_EXPORT
#endif
#else
#define SL_CAPI_EXPORT __attribute__((visibility("default")))
#endif // _WIN32

#ifdef __cplusplus
extern "C"
{
#endif
#if 0
    // Snore Audio Predictor - 鼾声音频检测器
    typedef struct  SL_EchoCancelFilter SL_EchoCancelFilter;


    //    SL_CAPI_EXPORT extern int
    SL_CAPI_EXPORT extern SL_EchoCancelFilter * SL_CreateEchoCancelFilter(const char *model_path);

    SL_CAPI_EXPORT extern void SL_ReleaseEchoCancelFilter(SL_EchoCancelFilter *predictor);

    SL_CAPI_EXPORT extern void SL_EchoCancelFilterForWav1C16khz(SL_EchoCancelFilter *predictor,float *mic,float *ref,float * res);

    SL_CAPI_EXPORT extern void SL_EchoNoiseCancelForWav1C16khz(SL_EchoCancelFilter *predictor,float *in,float *out);
#endif

    typedef struct SL_AudioProcesser SL_AudioProcesser;
    //    SL_CAPI_EXPORT extern int
    SL_CAPI_EXPORT extern SL_AudioProcesser * SL_CreateAudioProcesser(const char *model_path);

    SL_CAPI_EXPORT extern void SL_ReleaseAudioProcesser(SL_AudioProcesser *predictor);

    SL_CAPI_EXPORT extern void SL_EchoCancelFilterForWav1C16khz(SL_AudioProcesser *predictor,float *mic,float *ref,float * res);

    SL_CAPI_EXPORT extern void SL_EchoNoiseCancelForWav1C16khz(SL_AudioProcesser *predictor,float *in,float *out);



#ifdef __cplusplus
}
#endif





#endif //NKF_MNN_DEPLOY_R328_C_API_H
