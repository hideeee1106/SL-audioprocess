//
// Created by hideeee on 2025/5/16.
//

#ifndef NKF_MNN_DEPLOY_R328_VERSION_H
#define NKF_MNN_DEPLOY_R328_VERSION_H


/***
 * //版本号
           RV1106
AEC(回声消除)	 	nkf aec
NS（噪声消除）		rnnoise
AGC（语音增益）	 	webrtc agcm
VAD（静音检测）		webrtc vadm
KWS（唤醒词）		    wekws/pocketsphinx/snowboy(英文)
指令识别		        wekws/pocketsphinx
声源定位	            todo
推理框架		        mnn/rknn
精度		            f32 /f16/int8
***/

namespace ver {
    const static int X = 0;

    const static int Y = 1;

    const static int Z = 0;

}

#endif //NKF_MNN_DEPLOY_R328_VERSION_H
