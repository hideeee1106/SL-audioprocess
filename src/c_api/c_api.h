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

    typedef struct SL_AudioProcesser SL_AudioProcesser;
    //    SL_CAPI_EXPORT extern int
    SL_CAPI_EXPORT extern SL_AudioProcesser * SL_CreateAudioProcesser(const char *model_path);

    SL_CAPI_EXPORT extern void SL_ReleaseAudioProcesser(SL_AudioProcesser *predictor);

    SL_CAPI_EXPORT extern void SL_EchoCancelFilterForWav1C16khz(SL_AudioProcesser *predictor,short *mic,short *ref,short * res);


    SL_CAPI_EXPORT extern void SL_EchoNoiseCancelForWav1C16khz(SL_AudioProcesser *predictor,short *in,short *out);

/*!
 * AEC + 降噪算法
 * * @return 返回指令编号
 *  1: 唤醒词 识别到小问小问
 *  -1:没有开启唤醒词，降噪和回声消除都完成了
 *  0: 未识别到唤醒词
 *  -2:数据传输中
 */
    SL_CAPI_EXPORT extern int SL_AudioProcessFor8Khz(SL_AudioProcesser *predictor,short *in,short *ref,short *out);

/*!
 * 开启语音唤醒功能
 */
    SL_CAPI_EXPORT extern void SL_AudioOpenKWS(SL_AudioProcesser *predictor, const char *model_path,const char *token_file);

/*!
// * 回收资源并且关闭语音唤醒功能
// */
    SL_CAPI_EXPORT extern void SL_AudioKillKWS(SL_AudioProcesser *predictor);

    SL_CAPI_EXPORT extern int SL_Audio_kws_ns(SL_AudioProcesser *predictor,short *audio);
    SL_CAPI_EXPORT extern int SL_Audio_signle_kws(SL_AudioProcesser *predictor,short *audio);
#ifdef __cplusplus
}
#endif





#endif //NKF_MNN_DEPLOY_R328_C_API_H
