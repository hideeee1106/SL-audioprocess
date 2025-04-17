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

    SL_CAPI_EXPORT extern void SL_EchoCancelFilterForWav1C16khzSingle(SL_EchoCancelFilter *predictor,float *mic,float *ref,float * res);

#endif

    typedef struct SL_AudioProcesser SL_AudioProcesser;
    //    SL_CAPI_EXPORT extern int
    SL_CAPI_EXPORT extern SL_AudioProcesser * SL_CreateAudioProcesser(const char *model_path);

    SL_CAPI_EXPORT extern void SL_ReleaseAudioProcesser(SL_AudioProcesser *predictor);

    SL_CAPI_EXPORT extern void SL_EchoCancelFilterForWav1C16khz(SL_AudioProcesser *predictor,short *mic,short *ref,short * res);

/*!
 * 开始进行回声消除
 */
    SL_CAPI_EXPORT extern void SL_EchoNoiseCancelForWav1C16khz(SL_AudioProcesser *predictor,short *in,short *out);

/*!
 * 降噪算法
 */
    SL_CAPI_EXPORT extern int SL_AudioProcessFor8Khz(SL_AudioProcesser *predictor,short *in,short *ref,short *out);

/*!
 * 开启语音唤醒功能
 */
    SL_CAPI_EXPORT extern void SL_AudioOpenKWS(SL_AudioProcesser *predictor);


#ifdef __cplusplus
}
#endif





#endif //NKF_MNN_DEPLOY_R328_C_API_H
