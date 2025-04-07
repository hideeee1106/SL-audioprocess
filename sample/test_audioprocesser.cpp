//
// Created by hideeee on 2025/4/2.
//

#include "AudioFile.h"
#include "../src/c_api/c_api.h"
#include "../src/c_api/c_api_define.h"
#include <iostream>

void ExportWAV(
        const std::string & Filename,
        const std::vector<float>& Data,
        unsigned SampleRate) {
    AudioFile<float>::AudioBuffer Buffer;
    Buffer.resize(1);

    Buffer[0] = Data;
    size_t BufSz = Data.size();

    AudioFile<float> File;
    File.setAudioBuffer(Buffer);
    File.setAudioBufferSize(1, (int)BufSz);
    File.setNumSamplesPerChannel((int)BufSz);
    File.setNumChannels(1);
    File.setBitDepth(16);
    File.setSampleRate(8000);
    File.save(Filename, AudioFileFormat::Wave);
}

int main(int argc, char *argv[]){
    if (argc < 4)
    {
        printf("Usage:%s model_path mic_input_path ref_path\n", argv[0]);
        return -1;
    }
    char *model_path = argv[1];
    char *in_audio = argv[2];
    char *lpb_audio = argv[3];
    char *out_audio = argv[4];


    SL_AudioProcesser* filter = SL_CreateAudioProcesser(model_path);

    AudioFile<float> inputfile;
    inputfile.load(in_audio);
    AudioFile<float> inputlpbfile;
    inputlpbfile.load(lpb_audio);
    std::vector<float>  outputdata;


    int audiolen=inputfile.getNumSamplesPerChannel();
    int audiolen2=inputlpbfile.getNumSamplesPerChannel();
    audiolen =audiolen2<audiolen ? audiolen2:audiolen;
    int process_num=audiolen/256;

    for (int i = 0; i < process_num; ++i) {
        float outputs[256] = {0.0};
        SL_EchoCancelFilterForWav1C16khz(filter,&inputfile.samples[0][i*256],&inputlpbfile.samples[0][i*256],outputs);
        for(float output : outputs){
            outputdata.push_back(output);    //for one forward process save first BLOCK_SHIFT model output samples
        }
    }
    ExportWAV(out_audio,outputdata,8000);

    SL_ReleaseAudioProcesser(filter);



}


