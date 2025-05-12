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
    File.setSampleRate(SampleRate);
    File.save(Filename, AudioFileFormat::Wave);
}

int main(int argc, char *argv[]){
    if (argc < 5)
    {
        printf("Usage:%s model_path mic_input_path ref_path\n", argv[0]);
//        return -1;
    }
//    char *model_path = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/models/MODEL/nkfsim.mnn";
//    char *in_audio = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/resource/mic.wav";
//    char *lpb_audio = "/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/resource/ref.wav";
//    char *out_audio_wav ="/home/hideeee/CLionProjects/AudioProcess-Deploy-R328/resource/out.wav";

    char *model_path = "/tmp/lib/nkfsim.mnn";
    char *in_audio = "/mnt/UDISK/mic.wav";
    char *lpb_audio = "/mnt/UDISK/ref.wav";
    char *out_audio_wav ="/mnt/UDISK/out.wav";


    SL_AudioProcesser* filter = SL_CreateAudioProcesser(model_path);

    AudioFile<float> inputfile;
    inputfile.load(in_audio);
    AudioFile<float> inputlpbfile;
    inputlpbfile.load(lpb_audio);
    std::vector<float>  outputdata;


    int audiolen=inputfile.getNumSamplesPerChannel();
    int audiolen2=inputlpbfile.getNumSamplesPerChannel();
    audiolen =audiolen2<audiolen ? audiolen2:audiolen;
    int shiftlens = 512;
    int process_num=audiolen/shiftlens;


    vector<short> in(audiolen);
    for (int i = 0; i < audiolen; ++i) {
        in[i] = (short(inputfile.samples[0][i]*32768));
    }

    vector<short> lpb(audiolen);
    for (int i = 0; i < audiolen; ++i) {
        lpb[i] = (short(inputlpbfile.samples[0][i]*32768));
    }

    printf("%d\n",audiolen);
    for (int i = 0; i < process_num; ++i) {
        printf("i=%d\n",i);
        short outputs[512] = {0};

        SL_EchoCancelFilterForWav1C16khz(filter,&in[i*shiftlens],&lpb[i*shiftlens],outputs);

        for(int j = 0;j<shiftlens;++j){
//            printf("%d\n",outputs[j]);
            outputdata.push_back(float(outputs[j])/32768.0);    //for one forward process save first BLOCK_SHIFT model output samples
        }
    }

    ExportWAV(out_audio_wav,outputdata,16000);

    SL_ReleaseAudioProcesser(filter);



}


