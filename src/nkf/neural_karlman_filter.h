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



using namespace std;

#define SAMEPLERATE  (16000)  // 采样率 16 kHz
#define BLOCK_LEN    (1024)    // 处理块长度，每次处理 1024 个采样点
#define BLOCK_SHIFT  (256)    // 块移位，每次移动 256 采样点（50% 重叠）
#define FFT_OUT_SIZE (513)    // STFT 变换后单边频谱大小
#define NKF_LEN (4)          // NKF 滤波器的 tap 数
typedef complex<double> cpx_type;  // 复数数据类型

struct nkf_engine {
    float mic_buffer[BLOCK_LEN] = { 0 };  // 近端麦克风信号缓冲区
    float out_buffer[BLOCK_LEN] = { 0 };  // 处理后增强的音频缓冲区
    float lpb_buffer[BLOCK_LEN] = { 0 };  // 低功率带信号（参考信号）缓冲区

    float lpb_real[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 低功率带信号的实部
    float lpb_imag[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 低功率带信号的虚部
    double h_prior_real[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 先验滤波器实部
    double h_prior_imag[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 先验滤波器虚部
    double h_posterior_real[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 后验滤波器实部
    double h_posterior_imag[FFT_OUT_SIZE*NKF_LEN] = { 0 }; // 后验滤波器虚部

    std::vector<std::vector<float>> instates; // 存储 ONNX 模型的内部状态
};


class NKFProcessor{
public:
    // 禁止拷贝
    NKFProcessor(const NKFProcessor &) = delete;
    NKFProcessor &operator=(const NKFProcessor &) = delete;
    NKFProcessor() = default;
    ~NKFProcessor(){};



    std::vector<float > outputbuffer;

    void Aec_Init(const char *modelPath){
        nkf_net = std::make_shared<MNNAudioAdapter>(modelPath, 1);
        nkf_net->Init();
        for (int i=0;i<BLOCK_LEN;i++){
            hanning_windows[i]=sinf(M_PI*i/(BLOCK_LEN-1))*sinf(M_PI*i/(BLOCK_LEN-1));
        }
        ResetInout();
    }

    void enhance(const float * micdata,const float * refdata){
        memmove(m_pEngine.mic_buffer, m_pEngine.mic_buffer + BLOCK_SHIFT, (BLOCK_LEN - BLOCK_SHIFT) * sizeof(float));
        memmove(m_pEngine.lpb_buffer, m_pEngine.lpb_buffer + BLOCK_SHIFT, (BLOCK_LEN - BLOCK_SHIFT) * sizeof(float));

        for(int n=0;n<BLOCK_SHIFT;n++){
            m_pEngine.mic_buffer[n+BLOCK_LEN-BLOCK_SHIFT]=micdata[n];
            m_pEngine.lpb_buffer[n+BLOCK_LEN-BLOCK_SHIFT]=refdata[n];
            printf(",%f",refdata[n]);
        }
        AEC_Infer();

        for(int j=0;j<BLOCK_SHIFT;j++){
            outputbuffer.push_back(m_pEngine.out_buffer[j]);    //for one forward process save first BLOCK_SHIFT model output samples
        }
    }

    void reset(){
        outputbuffer.clear();
    }

    float * getoutput(){
        return  outputbuffer.data();
    }


    void AEC_Infer(){

        double estimated_block[BLOCK_LEN]={0};
        float mic_real[FFT_OUT_SIZE]={0};
        float mic_imag[FFT_OUT_SIZE]={0};

        double mic_in[BLOCK_LEN]={0};
        std::vector<cpx_type> mic_res(BLOCK_LEN);
        double lpb_in[BLOCK_LEN]={0};
        std::vector<cpx_type> lpb_res(BLOCK_LEN);

        std::vector<size_t> shape;
        shape.push_back(BLOCK_LEN);
        std::vector<size_t> axes;
        axes.push_back(0);
        std::vector<ptrdiff_t> stridel, strideo;
        strideo.push_back(sizeof(cpx_type));
        stridel.push_back(sizeof(double));

        for (int i = 0; i < BLOCK_LEN; i++){
            mic_in[i] = m_pEngine.mic_buffer[i]*hanning_windows[i];
            lpb_in[i]= m_pEngine.lpb_buffer[i]*hanning_windows[i];
        }

        pocketfft::r2c(shape, stridel, strideo, axes, pocketfft::FORWARD, mic_in, mic_res.data(), 1.0);
        pocketfft::r2c(shape, stridel, strideo, axes, pocketfft::FORWARD, lpb_in,lpb_res.data(), 1.0);

        memmove(m_pEngine.lpb_real,m_pEngine.lpb_real+FFT_OUT_SIZE,(NKF_LEN-1)*FFT_OUT_SIZE*sizeof(float));
        memmove(m_pEngine.lpb_imag,m_pEngine.lpb_imag+FFT_OUT_SIZE,(NKF_LEN-1)*FFT_OUT_SIZE*sizeof(float));
        for (int i=0;i<FFT_OUT_SIZE;i++){
            m_pEngine.lpb_real[(NKF_LEN-1)*FFT_OUT_SIZE+i]=static_cast<float>(lpb_res[i].real());
            m_pEngine.lpb_imag[(NKF_LEN-1)*FFT_OUT_SIZE+i]=static_cast<float>(lpb_res[i].imag());
            mic_real[i]=static_cast<float>(mic_res[i].real());
            mic_imag[i]=static_cast<float>(mic_res[i].imag());
        }
        float dh_real[NKF_LEN*FFT_OUT_SIZE]={0};
        float dh_imag[NKF_LEN*FFT_OUT_SIZE]={0};
        for (int i=0;i<NKF_LEN*FFT_OUT_SIZE;i++){
            dh_real[i]=static_cast<float>(m_pEngine.h_posterior_real[i]-m_pEngine.h_prior_real[i]);
            dh_imag[i]=static_cast<float>(m_pEngine.h_posterior_imag[i]-m_pEngine.h_prior_imag[i]);

        }

        memcpy(m_pEngine.h_prior_real,m_pEngine.h_posterior_real,NKF_LEN*FFT_OUT_SIZE*sizeof(double));
        memcpy(m_pEngine.h_prior_imag,m_pEngine.h_posterior_imag,NKF_LEN*FFT_OUT_SIZE*sizeof(double));

        vector<float> input_feature_real((2*NKF_LEN+1)*FFT_OUT_SIZE);
        vector<float> input_feature_imag((2*NKF_LEN+1)*FFT_OUT_SIZE);
        double e_real[FFT_OUT_SIZE]={0};
        double e_imag[FFT_OUT_SIZE]={0};


        int k=2*NKF_LEN+1;
        int is_tensor=1;
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

        double echohat_real[FFT_OUT_SIZE]={0};
        double echohat_imag[FFT_OUT_SIZE]={0};

        for (int i=0;i<FFT_OUT_SIZE;i++){
            for (int j=0;j<NKF_LEN;j++){
                echohat_real[i] +=m_pEngine.lpb_real[j*FFT_OUT_SIZE+i]*m_pEngine.h_posterior_real[NKF_LEN*i+j] -m_pEngine.lpb_imag[j*FFT_OUT_SIZE+i]*m_pEngine.h_posterior_imag[NKF_LEN*i+j];
                echohat_imag[i] +=m_pEngine.lpb_real[j*FFT_OUT_SIZE+i]*m_pEngine.h_posterior_imag[NKF_LEN*i+j] +m_pEngine.lpb_imag[j*FFT_OUT_SIZE+i]*m_pEngine.h_posterior_real[NKF_LEN*i+j];

            }
            mic_res[i] = cpx_type(mic_real[i]-echohat_real[i],mic_imag[i]-echohat_imag[i]);
        }
        double mic_out[BLOCK_LEN]={0};

        pocketfft::c2r(shape, strideo, stridel, axes, false, mic_res.data(), mic_out, 1.0);

        for (int i = 0; i < BLOCK_LEN; i++){
            estimated_block[i] = (mic_out[i])/1024.0;
        }


        memmove(m_pEngine.out_buffer, m_pEngine.out_buffer + BLOCK_SHIFT,
                (BLOCK_LEN - BLOCK_SHIFT) * sizeof(float));
        memset(m_pEngine.out_buffer + (BLOCK_LEN - BLOCK_SHIFT),
               0, BLOCK_SHIFT * sizeof(float));
        for (int i = 0; i < BLOCK_LEN; i++){
            m_pEngine.out_buffer[i] += estimated_block[i]*hanning_windows[i];
        }


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
        memset(m_pEngine.h_posterior_real,0,FFT_OUT_SIZE*NKF_LEN*sizeof(double));
        memset(m_pEngine.h_posterior_imag,0,FFT_OUT_SIZE*NKF_LEN*sizeof(double));
        memset(m_pEngine.h_prior_real,0,FFT_OUT_SIZE*NKF_LEN*sizeof(double));
        memset(m_pEngine.h_prior_imag,0,FFT_OUT_SIZE*NKF_LEN*sizeof(double));

    };

private:
    nkf_engine m_pEngine;
    float hanning_windows[BLOCK_LEN]={0};

    std::shared_ptr<MNNAudioAdapter> nkf_net;




};
#endif //NKF_MNN_DEPLOY_R328_NEURAL_KARLMAN_FILTER_H
