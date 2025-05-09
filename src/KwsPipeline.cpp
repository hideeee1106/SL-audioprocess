//
// Created by zyj on 24-4-26.
//

#include "KwsPipeline.h"

KwsPipeline::KwsPipeline(const std::string &model_path, const std::string &token_file) : spotter_pipe(model_path) {

    postDecoder.read_token(token_file);


}

int KwsPipeline::run(const std::string &wav_path)  {

    wavReader.Open(wav_path);
    std::vector<float>wav{wavReader.data(),wavReader.data()+wavReader.num_samples()};

    featurePipeline.AcceptWaveform(wav);
    featurePipeline.set_input_finished();


    while (true) {
        std::vector<std::vector<float>> feats;
        bool ok = featurePipeline.Read(input_batch_size, &feats);
        std::vector<std::vector<float>> prob;
        spotter_pipe.Forward(feats, &prob);

        postDecoder.decode(prob);

        // Reach the end of feature pipeline
        if (!ok) break;
        offset += prob.size();

    }

    int result_idx= postDecoder.match_and_output();
    postDecoder.Reset();
    return result_idx;
}

int KwsPipeline::run(const std::vector<int16_t> &wav)
{
    //printf("wav:%d\n",wav[0]);
    featurePipeline.AcceptWaveform(wav);
    featurePipeline.set_input_finished();

    while (true) {
        std::vector<std::vector<float>> feats;
        bool ok = featurePipeline.Read(input_batch_size, &feats);
//        printf("feats:%f\n",feats[0][0]);

        std::vector<std::vector<float>> prob;
        spotter_pipe.Forward(feats, &prob);

        postDecoder.decode(prob);

        // Reach the end of feature pipeline
        if (!ok) break;
        offset += prob.size();



    }

    int result_idx= postDecoder.match_and_output();
    postDecoder.Reset();
    featurePipeline.Reset();
    return result_idx;

}
