//
// Created by liangtao_deng on 2025/3/17.
//

#ifndef NKF_MNN_DEPLOY_R328_MNN_ADAPTER_H
#define NKF_MNN_DEPLOY_R328_MNN_ADAPTER_H

#include <MNN/ImageProcess.hpp>
#include <MNN/Interpreter.hpp>
#include <MNN/MNNDefine.h>
#include <MNN/Tensor.hpp>
#include <MNN/MNNForwardType.h>

#include <iostream>
#include <fstream>
#include <vector>
#include "../model_loader/model_loader.h"

class MNNAudioAdapter {
public:

    MNNAudioAdapter(const std::string &model, int thread, bool use_model_bin = false) {
//        std::cout << "MNNAudioAdapter constructor called with: " << model << std::endl;
//        if (model.empty()) {
//            std::cerr << "Error: modelPath is empty!" << std::endl;
//            return;
//        }

        backend_ = MNN_FORWARD_CPU;
        detect_model_ = std::shared_ptr<MNN::Interpreter>(
                MNN::Interpreter::createFromFile(model.c_str()));

        _config.type = backend_;
        _config.numThread = thread;
        MNN::BackendConfig backendConfig;
        backendConfig.precision = MNN::BackendConfig::Precision_High;
        backendConfig.power = MNN::BackendConfig::Power_High;
        _config.backendConfig = &backendConfig;
        std::cout << "MNNAudioAdapter end" << std::endl;
    }

    MNNAudioAdapter(const solex::Model *model, int thread) {
        backend_ = MNN_FORWARD_CPU;
        detect_model_ =
                std::shared_ptr<MNN::Interpreter>(MNN::Interpreter::createFromBuffer(
                        model->caffemodelBuffer, model->modelsize.caffemodel_size));
        _config.type = backend_;
        _config.numThread = thread;

    }


    ~MNNAudioAdapter() {
        detect_model_->releaseModel();
        detect_model_->releaseSession(sess);
    }

    void Init() {
//        MNN_PRINT("Creating MNN session...\n");
        sess = detect_model_->createSession(_config);
//        MNN_PRINT("Session created!\n");

    }

    void Infer(const std::vector<float>& input_real, const std::vector<float>& input_imag,const  std::vector<std::vector<float>>& inputs) {
        auto in_real =detect_model_->getSessionInput(sess, "in_real");
        auto in_imag = detect_model_->getSessionInput(sess, "in_imag");
        auto in_hrr =detect_model_->getSessionInput(sess, "in_hrr");
        auto in_hir = detect_model_->getSessionInput(sess, "in_hir");
        auto in_hri =detect_model_->getSessionInput(sess, "in_hri");
        auto in_hii = detect_model_->getSessionInput(sess, "in_hii");

        // 复制 nearEnd 数据到 inputTensor
        memcpy(in_real->host<float>(), input_real.data(), input_real.size() * sizeof(float));
        memcpy(in_imag->host<float>(), input_imag.data(), input_imag.size() * sizeof(float));

        memcpy(in_hrr->host<float>(), inputs[0].data(), inputs[0].size() * sizeof(float));
        memcpy(in_hir->host<float>(), inputs[1].data(), inputs[1].size() * sizeof(float));
        memcpy(in_hri->host<float>(), inputs[2].data(), inputs[2].size() * sizeof(float));
        memcpy(in_hii->host<float>(), inputs[3].data(), inputs[3].size() * sizeof(float));


        // 运行 AEC 计算
        detect_model_->runSession(sess);
    }

    MNN::Tensor *  getOutput(const std::string& outputname){
        // 获取去除回声后的输出
        auto outputTensor = detect_model_->getSessionOutput(sess, outputname.c_str());
        return outputTensor;
    }


private:
    std::shared_ptr<MNN::Interpreter> detect_model_;
    MNN::Session *sess{};
    MNN::ScheduleConfig _config;
    MNNForwardType backend_;

};


#endif //NKF_MNN_DEPLOY_R328_MNN_ADAPTER_H
