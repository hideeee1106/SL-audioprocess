//
// Created by zyj on 24-4-26.
//

#ifndef WEKWS_KWSPIPELINE_H
#define WEKWS_KWSPIPELINE_H

#include <utility>

#include "../src/kws/keyword_spotting_mnn.h"
#include "../src/frontend/feature_pipeline.h"
#include "../src/frontend/postprocess_pipeline.h"
#include "frontend/wav.h"
#include "utils/keyword_instruction.h"

class KwsPipeline {

public:
    KwsPipeline(const std::string& model_path, const std::string& token_file);


    int run(const std::string& wav_path);

    int run(const std::vector<int16_t> &wav);




private:

    int num_feature_bins=80;//80 bin
    int sample_rate=16000;//16k采样
    int input_batch_size=16;

    int offset = 0;

    wenet::FeaturePipelineConfig featurePipelineConfig{num_feature_bins,sample_rate};

    wenet::WavReader wavReader;

    wekwsMNN::KeywordSpottingMNN spotter_pipe;

    wenet::FeaturePipeline featurePipeline{featurePipelineConfig};

    PostDecoder postDecoder;



};


#endif //WEKWS_KWSPIPELINE_H
