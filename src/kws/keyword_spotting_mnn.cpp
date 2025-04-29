//
// Created by zyj on 24-4-26.
//
#include "keyword_spotting_mnn.h"
#include "cstring"

wekwsMNN::KeywordSpottingMNN::KeywordSpottingMNN(const std::string &model_path) {
    // 1. Create Interpreter
    interpreter_ = std::shared_ptr<MNN::Interpreter>(MNN::Interpreter::createFromFile(model_path.c_str()));
    // 2. Create Session
    MNN::ScheduleConfig config;
    session_ = interpreter_->createSession(config);


    inputTensor = interpreter_->getSessionInput(session_, "input");
    cacheTensor = interpreter_->getSessionInput(session_, "cache");
    outputTensor = interpreter_->getSessionOutput(session_, "output");


    auto inputShape = inputTensor->shape();
    auto cacheShape = cacheTensor->shape();


    std::cout << "inputTensor shape:" << std::endl;
    for (auto shapeValue: inputShape) { std::cout << shapeValue << " "; }
    std::cout << "\ncacheTensor shape:" << std::endl;
    for (auto shapeValue: cacheShape) { std::cout << shapeValue << " "; }
    std::cout << std::endl;
}


void wekwsMNN::KeywordSpottingMNN::Forward(const std::vector<std::vector<float>> &feats,
                                           std::vector<std::vector<float>> *prob) {

    prob->clear();
    if (feats.size() == 0) return;

    // 1. Prepare Input
    int num_frames = feats.size();
    int feature_dim = feats[0].size();
    std::vector<float> slice_feats;
    for (int i = 0; i < feats.size(); i++) {
        slice_feats.insert(slice_feats.end(), feats[i].begin(), feats[i].end());
    }

    std::vector<int> inputShape = {1, num_frames, feature_dim};
    std::shared_ptr<MNN::Tensor> inputTensorUser(
            MNN::Tensor::create<float>(inputShape, slice_feats.data(), MNN::Tensor::CAFFE));

    interpreter_->resizeTensor(inputTensor, inputShape);
    interpreter_->resizeSession(session_);

    inputTensor->copyFromHostTensor(inputTensorUser.get());


    // 2. MNN Forward
    interpreter_->runSession(session_);

    //缓存结果送回cacheTensor下次推理使用
    auto r_cache=interpreter_->getSessionOutput(session_,"r_cache");
    cacheTensor->copyFromHostTensor(r_cache);

    // 3. Get Keyword Prob
    int num_outputs = outputTensor->channel();
    int output_dim = outputTensor->height();
    prob->resize(num_outputs);
    for (int i = 0; i < num_outputs; i++) {
        (*prob)[i].resize(output_dim);
        ::memcpy((*prob)[i].data(), outputTensor->host<float>() + i * output_dim,
                 sizeof(float) * output_dim);
    }


}