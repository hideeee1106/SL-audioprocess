//
// Created by zyj on 24-4-24.
//

#ifndef WEKWS_KEYWORD_SPOTTING_MNN_H
#define WEKWS_KEYWORD_SPOTTING_MNN_H

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <MNN/Interpreter.hpp>
#include <MNN/MNNDefine.h>
#include <MNN/Tensor.hpp>

namespace wekwsMNN {

    class KeywordSpottingMNN {
    public:
        KeywordSpottingMNN(const std::string &model_path) ;

        void Forward(
                const std::vector<std::vector<float>> &feats,
                std::vector<std::vector<float>> *prob);


    private:
        std::shared_ptr<MNN::Interpreter> interpreter_;
        MNN::Session *session_;
        std::vector<std::string> in_names_{"input", "cache"};
        std::vector<std::string> out_names_{"output", "r_cache"};


        MNN::Tensor *inputTensor;
        MNN::Tensor *cacheTensor;
        MNN::Tensor *outputTensor;
    };

}  // namespace wekws

#endif //WEKWS_KEYWORD_SPOTTING_MNN_H
