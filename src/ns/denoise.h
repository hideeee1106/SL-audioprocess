//
// Created by hideeee on 2025/3/28.
//

#ifndef NKF_MNN_DEPLOY_R328_DENOSIE_H
#define NKF_MNN_DEPLOY_R328_DENOSIE_H


class NosieCancel {

public:
    NosieCancel(){};
    ~NosieCancel(){}

    typedef struct DenoiseState DenoiseState;

    float rnnoise_process_frame(float *out, const float *in);
private:
    DenoiseState* st;


};


#endif //NKF_MNN_DEPLOY_R328_DENOSIE_H
