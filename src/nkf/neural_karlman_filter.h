//
// Created by liangtao_deng on 2025/3/17.
//

#ifndef NKF_MNN_DEPLOY_R328_NEURAL_KARLMAN_FILTER_H
#define NKF_MNN_DEPLOY_R328_NEURAL_KARLMAN_FILTER_H


#include "vector"
#include "mnn_adapter.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <complex>
#include "../log.h"
#include "pocketfft_hdronly.h"
#include "cmath"
#include "fftw3.h"

//#define USE_NEON 0
//#ifdef USE_NEON
////    #include <arm_neon.h>
//#endif


using namespace std;

#define SAMEPLERATE  (16000)  // 采样率 16 kHz
#define BLOCK_LEN    (1024)    // 处理块长度，每次处理 1024 个采样点
#define BLOCK_SHIFT  (512)    // 块移位，每次移动 256 采样点（50% 重叠）
#define FFT_OUT_SIZE (513)    // STFT 变换后单边频谱大小
#define NKF_LEN (4)          // NKF 滤波器的 tap 数
//typedef complex<float> cpx_type;  // 复数数据类型

struct nkf_engine {
    float mic_buffer[BLOCK_LEN] = { 0 };  // 近端麦克风信号缓冲区
    float out_buffer[BLOCK_LEN] = { 0 };  // 处理后增强的音频缓冲区
    float lpb_buffer[BLOCK_LEN] = { 0 };  // 低功率带信号（参考信号）缓冲区

    float lpb_real[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 低功率带信号的实部
    float lpb_imag[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 低功率带信号的虚部
    float h_prior_real[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 先验滤波器实部
    float h_prior_imag[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 先验滤波器虚部
    float h_posterior_real[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 后验滤波器实部
    float h_posterior_imag[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 后验滤波器虚部

    std::vector<std::vector<float>> instates; // 存储 ONNX 模型的内部状态
};


class NKFProcessor{
public:
    // 禁止拷贝
    NKFProcessor(const NKFProcessor &) = delete;
    NKFProcessor &operator=(const NKFProcessor &) = delete;
    NKFProcessor() = default;
    ~NKFProcessor(){};



    std::vector<short > outputbuffer;

    void Aec_Init(const char *modelPath){
        nkf_net = std::make_shared<MNNAudioAdapter>(modelPath, 1);
        nkf_net->Init();
        for (int i=0;i<BLOCK_LEN;i++){
            hanning_windows[i]=sinf(M_PI*i/(BLOCK_LEN-1))*sinf(M_PI*i/(BLOCK_LEN-1));
        }
        ResetInout();
    }

    void enhance(const short * micdata,const short * refdata){
//       将后面1024-256的部分移动到前面
        memmove(m_pEngine.mic_buffer, m_pEngine.mic_buffer + BLOCK_SHIFT, (BLOCK_LEN - BLOCK_SHIFT) * sizeof(float));
        memmove(m_pEngine.lpb_buffer, m_pEngine.lpb_buffer + BLOCK_SHIFT, (BLOCK_LEN - BLOCK_SHIFT) * sizeof(float));

        for(int n=0;n<BLOCK_SHIFT;n++){
            m_pEngine.mic_buffer[n+BLOCK_LEN-BLOCK_SHIFT]=(float)micdata[n]/32768.0;
            m_pEngine.lpb_buffer[n+BLOCK_LEN-BLOCK_SHIFT]=(float)refdata[n]/32768.0;
        }
        AEC_Infer();

        for(int j=0;j<BLOCK_SHIFT;j++){
            outputbuffer.push_back(m_pEngine.out_buffer[j]*32768);    //for one forward process save first BLOCK_SHIFT model output samples
//            printf("outputbuffer:%d,",outputbuffer[j]);
        }
    }

    void reset(){
        outputbuffer.clear();
    }

    short * getoutput(){
        return  outputbuffer.data();
    }



    void AEC_Infer(){

        float estimated_block[BLOCK_LEN]={0};
        float mic_real[FFT_OUT_SIZE]={0};
        float mic_imag[FFT_OUT_SIZE]={0};

        float mic_in[BLOCK_LEN]={0};
        fftwf_complex *mic_res;
        float lpb_in[BLOCK_LEN]={0};
        fftwf_complex *lpb_res;


        for (int i = 0; i < BLOCK_LEN; i++){
            mic_in[i] = m_pEngine.mic_buffer[i]*hanning_windows[i];
            lpb_in[i]= m_pEngine.lpb_buffer[i]*hanning_windows[i];
        }

//      fftw3
        mic_res = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * FFT_OUT_SIZE);
        lpb_res = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * FFT_OUT_SIZE);

        // 创建 FFTW3 计划
        fftwf_plan mic_plan = fftwf_plan_dft_r2c_1d(BLOCK_LEN, mic_in, mic_res, FFTW_ESTIMATE);
        fftwf_plan lpb_plan = fftwf_plan_dft_r2c_1d(BLOCK_LEN, lpb_in, lpb_res, FFTW_ESTIMATE);

        // 执行 FFT（前向变换）
        fftwf_execute(mic_plan);
        fftwf_execute(lpb_plan);

        //printf("micres:%f\n",mic_res[0][0]);
//        pocketfft::r2c(shape, stridel, strideo, axes, pocketfft::FORWARD, mic_in, mic_res.data(), 1.0);
//        pocketfft::r2c(shape, stridel, strideo, axes, pocketfft::FORWARD, lpb_in,lpb_res.data(), 1.0);

        memmove(m_pEngine.lpb_real,m_pEngine.lpb_real+FFT_OUT_SIZE,(NKF_LEN-1)*FFT_OUT_SIZE*sizeof(float));
        memmove(m_pEngine.lpb_imag,m_pEngine.lpb_imag+FFT_OUT_SIZE,(NKF_LEN-1)*FFT_OUT_SIZE*sizeof(float));
        for (int i=0;i<FFT_OUT_SIZE;i++){
            m_pEngine.lpb_real[(NKF_LEN-1)*FFT_OUT_SIZE+i]=(lpb_res[i][0]);
            m_pEngine.lpb_imag[(NKF_LEN-1)*FFT_OUT_SIZE+i]=(lpb_res[i][1]);
            mic_real[i]=(mic_res[i][0]);
            mic_imag[i]=(mic_res[i][1]);
        }
        float dh_real[NKF_LEN*FFT_OUT_SIZE]={0};
        float dh_imag[NKF_LEN*FFT_OUT_SIZE]={0};
        for (int i=0;i<NKF_LEN*FFT_OUT_SIZE;i++){
            dh_real[i]=(m_pEngine.h_posterior_real[i]-m_pEngine.h_prior_real[i]);
            dh_imag[i]=(m_pEngine.h_posterior_imag[i]-m_pEngine.h_prior_imag[i]);

        }

        memcpy(m_pEngine.h_prior_real,m_pEngine.h_posterior_real,NKF_LEN*FFT_OUT_SIZE*sizeof(float));
        memcpy(m_pEngine.h_prior_imag,m_pEngine.h_posterior_imag,NKF_LEN*FFT_OUT_SIZE*sizeof(float));

        vector<float> input_feature_real((2*NKF_LEN+1)*FFT_OUT_SIZE);
        vector<float> input_feature_imag((2*NKF_LEN+1)*FFT_OUT_SIZE);
        float e_real[FFT_OUT_SIZE]={0};
        float e_imag[FFT_OUT_SIZE]={0};


        int k=2*NKF_LEN+1;
        int is_tensor=1;
//#ifdef USE_NEON
//        printf("USE_NEON");
//        for (int i = 0; i < FFT_OUT_SIZE; i++) {
//            float32x4_t sum_real = vdupq_n_f32(0.0f);
//            float32x4_t sum_imag = vdupq_n_f32(0.0f);
//
//            for (int j = 0; j < NKF_LEN; j += 4) {
//                // 加载 lpb 和 h_prior
//                float32x4_t lpb_real = vld1q_f32(&m_pEngine.lpb_real[j * FFT_OUT_SIZE + i]);
//                float32x4_t lpb_imag = vld1q_f32(&m_pEngine.lpb_imag[j * FFT_OUT_SIZE + i]);
//
//                float32x4_t h_real = vld1q_f32(&m_pEngine.h_prior_real[NKF_LEN * i + j]);
//                float32x4_t h_imag = vld1q_f32(&m_pEngine.h_prior_imag[NKF_LEN * i + j]);
//
//                // 储存特征输入
//                vst1q_f32(&input_feature_real[k * i + j], lpb_real);
//                vst1q_f32(&input_feature_imag[k * i + j], lpb_imag);
//                vst1q_f32(&input_feature_real[k * i + j + NKF_LEN + 1], h_real);
//                vst1q_f32(&input_feature_imag[k * i + j + NKF_LEN + 1], h_imag);
//
//                // 误差信号部分 (e_real / e_imag) 累加
//                // e_real += lpb_real * h_real - lpb_imag * h_imag
//                // e_imag += lpb_real * h_imag + lpb_imag * h_real
//                sum_real = vmlsq_f32(vmlaq_f32(sum_real, lpb_real, h_real), lpb_imag, h_imag);
//
//
//                sum_imag = vmlaq_f32(sum_imag, lpb_real, h_imag);
//                sum_imag = vmlaq_f32(sum_imag, lpb_imag, h_real);
//            }
//
//            // 水平加法累加 e_real / e_imag
//            float32_t tmp_real[4], tmp_imag[4];
//            vst1q_f32(tmp_real, sum_real);
//            vst1q_f32(tmp_imag, sum_imag);
//
//            float e_r = mic_real[i] - (tmp_real[0] + tmp_real[1] + tmp_real[2] + tmp_real[3]);
//            float e_i = mic_imag[i] - (tmp_imag[0] + tmp_imag[1] + tmp_imag[2] + tmp_imag[3]);
//
//            input_feature_real[k * i + NKF_LEN] = e_r;
//            input_feature_imag[k * i + NKF_LEN] = e_i;
//        }
//#else
        for (int i=0;i<FFT_OUT_SIZE;i++){

            for (int j=0;j<NKF_LEN;j++){
                input_feature_real[k*i+j]=m_pEngine.lpb_real[j*FFT_OUT_SIZE+i];
                input_feature_imag[k*i+j]=m_pEngine.lpb_imag[j*FFT_OUT_SIZE+i];
                input_feature_real[k*i+j+NKF_LEN+1]=dh_real[NKF_LEN*i+j];
                input_feature_imag[k*i+j+NKF_LEN+1]=dh_imag[NKF_LEN*i+j];

                e_real[i] +=m_pEngine.lpb_real[j*FFT_OUT_SIZE+i]*m_pEngine.h_prior_real[NKF_LEN*i+j] -m_pEngine.lpb_imag[j*FFT_OUT_SIZE+i]*m_pEngine.h_prior_imag[NKF_LEN*i+j];
                e_imag[i] +=m_pEngine.lpb_real[j*FFT_OUT_SIZE+i]*m_pEngine.h_prior_imag[NKF_LEN*i+j] +m_pEngine.lpb_imag[j*FFT_OUT_SIZE+i]*m_pEngine.h_prior_real[NKF_LEN*i+j];
            }
            e_real[i]=mic_real[i]-e_real[i];
            e_imag[i]=mic_imag[i]-e_imag[i];
            input_feature_real[k*i+NKF_LEN]=static_cast<float>(e_real[i]);
            input_feature_imag[k*i+NKF_LEN]=static_cast<float>(e_imag[i]);

        }

//#endif

        nkf_net->Infer(input_feature_real,input_feature_imag,m_pEngine.instates);


        auto enh_real = nkf_net->getOutput("enh_real");
        auto enh_imag = nkf_net->getOutput("enh_imag");
        auto out_hrr =nkf_net->getOutput( "out_hrr");
        auto out_hir = nkf_net->getOutput( "out_hir");
        auto out_hri =nkf_net->getOutput( "out_hri");
        auto out_hii = nkf_net->getOutput( "out_hii");

        auto enh_real_host_tensor = new MNN::Tensor(enh_real, enh_real->getDimensionType());
        enh_real->copyToHostTensor(enh_real_host_tensor);
        auto *kgreal = enh_real_host_tensor->host<float>();

        auto enh_imag_host_tensor = new MNN::Tensor(enh_imag, enh_imag->getDimensionType());
        enh_imag->copyToHostTensor(enh_imag_host_tensor);
        auto *kgimag = enh_imag_host_tensor->host<float>();



        auto out_hrr_host_tensor = new MNN::Tensor(out_hrr, out_hrr->getDimensionType());
        out_hrr->copyToHostTensor(out_hrr_host_tensor);
        auto *hrr_out_state = out_hrr_host_tensor->host<float>();
        memcpy(m_pEngine.instates[0].data(), hrr_out_state, FFT_OUT_SIZE*18 * sizeof(float));

        auto out_hir_host_tensor = new MNN::Tensor(out_hir, out_hir->getDimensionType());
        out_hir->copyToHostTensor(out_hir_host_tensor);
        auto *hir_out_state = out_hir_host_tensor->host<float>();
        memcpy(m_pEngine.instates[1].data(), hir_out_state, FFT_OUT_SIZE*18 * sizeof(float));

        auto out_hri_host_tensor = new MNN::Tensor(out_hri, out_hri->getDimensionType());
        out_hri->copyToHostTensor(out_hri_host_tensor);
        auto *hri_out_state = out_hri_host_tensor->host<float>();
        memcpy(m_pEngine.instates[2].data(), hri_out_state, FFT_OUT_SIZE*18 * sizeof(float));

        auto out_hii_host_tensor = new MNN::Tensor(out_hii, out_hii->getDimensionType());
        out_hii->copyToHostTensor(out_hii_host_tensor);
        auto *hii_out_state = out_hii_host_tensor->host<float>();
        memcpy(m_pEngine.instates[3].data(), hii_out_state, FFT_OUT_SIZE*18 * sizeof(float));

        for (int i=0;i<FFT_OUT_SIZE;i++){
            for (int j=0;j<NKF_LEN;j++){
                m_pEngine.h_posterior_real[NKF_LEN*i+j] =m_pEngine.h_prior_real[NKF_LEN*i+j] +e_real[i]*kgreal[NKF_LEN*i+j]-e_imag[i]*kgimag[NKF_LEN*i+j];
                m_pEngine.h_posterior_imag[NKF_LEN*i+j] =m_pEngine.h_prior_imag[NKF_LEN*i+j] +e_imag[i]*kgreal[NKF_LEN*i+j]+e_real[i]*kgimag[NKF_LEN*i+j];

            }
        }

        float echohat_real[FFT_OUT_SIZE]={0};
        float echohat_imag[FFT_OUT_SIZE]={0};

        for (int i=0;i<FFT_OUT_SIZE;i++){
            for (int j=0;j<NKF_LEN;j++){
                echohat_real[i] +=m_pEngine.lpb_real[j*FFT_OUT_SIZE+i]*m_pEngine.h_posterior_real[NKF_LEN*i+j] -m_pEngine.lpb_imag[j*FFT_OUT_SIZE+i]*m_pEngine.h_posterior_imag[NKF_LEN*i+j];
                echohat_imag[i] +=m_pEngine.lpb_real[j*FFT_OUT_SIZE+i]*m_pEngine.h_posterior_imag[NKF_LEN*i+j] +m_pEngine.lpb_imag[j*FFT_OUT_SIZE+i]*m_pEngine.h_posterior_real[NKF_LEN*i+j];

            }
            mic_res[i][0] = mic_real[i]-echohat_real[i];
            mic_res[i][1] = mic_imag[i]-echohat_imag[i];
//            mic_res[i] = cpx_type(mic_real[i]-echohat_real[i],mic_imag[i]-echohat_imag[i]);
        }
        float mic_out[BLOCK_LEN]={0};


        fftwf_plan plan_rfft = fftwf_plan_dft_c2r_1d(BLOCK_LEN, mic_res, mic_out, FFTW_ESTIMATE);
        fftwf_execute(plan_rfft);
//        pocketfft::c2r(shape, strideo, stridel, axes, false, mic_res.data(), mic_out, 1.0);

//#ifdef USE_NEON
//        // 使用SIMD加速复数转实数
//        float32x4_t scale = vdupq_n_f32(1.0f / 1024.0f);
//        for (int i = 0; i < BLOCK_LEN; i += 4) {
//            // 将 float 类型的 mic_out[i] 转换为 float，再进行 SIMD 操作
//            float tmp[4] = {(mic_out[i]),(mic_out[i + 1]),(mic_out[i + 2]),(mic_out[i + 3])};
//
//            float32x4_t out_vec = vld1q_f32(tmp);
//            vst1q_f32(&estimated_block[i], vmulq_f32(out_vec, scale));
//        }
//#else
        for (int i = 0; i < BLOCK_LEN; i++){
            estimated_block[i] = (mic_out[i])/1024.0;
        }
//#endif

        memmove(m_pEngine.out_buffer, m_pEngine.out_buffer + BLOCK_SHIFT,
                (BLOCK_LEN - BLOCK_SHIFT) * sizeof(float));
        memset(m_pEngine.out_buffer + (BLOCK_LEN - BLOCK_SHIFT),
               0, BLOCK_SHIFT * sizeof(float));
//#ifdef USE_NEON
//        constexpr int num_blocks = BLOCK_LEN / 4; // 1024 / 4 = 256 blocks
//
//        // 主循环（NEON向量化 + 循环展开）
//        for (int i = 0; i < num_blocks; i += 2) { // 每次处理2组4个float（8个float）
//            // 加载第一组数据
//            float32x4_t out1 = vld1q_f32(&m_pEngine.out_buffer[i*4 + 0]);
//            float32x4_t est1 = vld1q_f32(&estimated_block[i*4 + 0]);
//            float32x4_t win1 = vld1q_f32(&hanning_windows[i*4 + 0]);
//
//            // 加载第二组数据
//            float32x4_t out2 = vld1q_f32(&m_pEngine.out_buffer[i*4 + 4]);
//            float32x4_t est2 = vld1q_f32(&estimated_block[i*4 + 4]);
//            float32x4_t win2 = vld1q_f32(&hanning_windows[i*4 + 4]);
//
//            // 计算乘加（fma指令）
//            float32x4_t res1 = vfmaq_f32(out1, est1, win1);
//            float32x4_t res2 = vfmaq_f32(out2, est2, win2);
//
//            // 存回结果
//            vst1q_f32(&m_pEngine.out_buffer[i*4 + 0], res1);
//            vst1q_f32(&m_pEngine.out_buffer[i*4 + 4], res2);
//        }
//#else
        for (int i = 0; i < BLOCK_LEN; i++){
            m_pEngine.out_buffer[i] += estimated_block[i]*hanning_windows[i];
        }
//#endif
        delete enh_real_host_tensor;
        delete enh_imag_host_tensor;
        delete out_hrr_host_tensor;
        delete out_hir_host_tensor;
        delete out_hri_host_tensor;
        delete out_hii_host_tensor;

        fftwf_destroy_plan(mic_plan);
        fftwf_destroy_plan(lpb_plan);
        fftwf_destroy_plan(plan_rfft);
        fftwf_free(mic_res);
        fftwf_free(lpb_res);


    }




private:
    void ResetInout(){

        m_pEngine.instates.clear();
        m_pEngine.instates.resize(4);
        for (int i=0;i<4;i++){
            m_pEngine.instates[i].clear();
            m_pEngine.instates[i].resize(FFT_OUT_SIZE*18);
            std::fill(m_pEngine.instates[i].begin(),m_pEngine.instates[i].end(),0);
        }
        memset(m_pEngine.mic_buffer,0,BLOCK_LEN*sizeof(float));
        memset(m_pEngine.lpb_buffer,0,BLOCK_LEN*sizeof(float));
        memset(m_pEngine.out_buffer,0,BLOCK_LEN*sizeof(float));
        memset(m_pEngine.lpb_real,0,FFT_OUT_SIZE*NKF_LEN*sizeof(float));
        memset(m_pEngine.lpb_imag,0,FFT_OUT_SIZE*NKF_LEN*sizeof(float));
        memset(m_pEngine.h_posterior_real,0,FFT_OUT_SIZE*NKF_LEN*sizeof(float));
        memset(m_pEngine.h_posterior_imag,0,FFT_OUT_SIZE*NKF_LEN*sizeof(float));
        memset(m_pEngine.h_prior_real,0,FFT_OUT_SIZE*NKF_LEN*sizeof(float));
        memset(m_pEngine.h_prior_imag,0,FFT_OUT_SIZE*NKF_LEN*sizeof(float));

    };

private:
    nkf_engine m_pEngine;
    float hanning_windows[BLOCK_LEN]={0};

    std::shared_ptr<MNNAudioAdapter> nkf_net;




};
#endif //NKF_MNN_DEPLOY_R328_NEURAL_KARLMAN_FILTER_H
