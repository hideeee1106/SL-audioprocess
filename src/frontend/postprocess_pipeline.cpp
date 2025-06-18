//
// Created by zyj on 24-4-15.
//

#include "postprocess_pipeline.h"
#include <locale>
#include <codecvt>
#include "../utils/keyword_instruction.h"

PostDecoder::PostDecoder() {
    commandsrecognizer = std::make_shared<CommandRecognizer>();
}


void PostDecoder::decode(const std::vector<std::vector<float>> &probs) {
    for (const auto& frame_probs : probs) {
        // 在每个时间步上选择概率最高的标签
        int max_idx = std::max_element(frame_probs.begin(), frame_probs.end()) - frame_probs.begin();
        // 如果该标签不是空白标签，且与前一个标签不同，大于置信度阈值则添加到输出序列中
        if (max_idx != 0 && (decoded_seq.empty() || max_idx != decoded_seq.back())&&frame_probs[max_idx]>thersh_score) //
        {
            decoded_seq.push_back(max_idx);

            // std::cout<<"token_idx:"<<max_idx<<" score:"<<frame_probs[max_idx]<<std::endl;
        }
    }

}

int PostDecoder::read_token(const std::string &token_file) {

    std::ifstream fin(token_file);
    if (!fin.is_open()) {
        std::cerr << "Failed to open token file: " << token_file << std::endl;
        return -1;
    }

    std::string line;
    while (std::getline(fin, line)) {
        std::istringstream iss(line);
        std::string token;
        int index;
        if (!(iss >> token >> index)) {
            std::cerr << "Error reading token file: " << token_file << std::endl;
            return -1;
        }
        tokens_table[token] = index - 1; //模型输出idx从0开始,token中对应减1
    }
    std::cout<<"read token success!"<<std::endl;
    fin.close();

    //获取关键词对应的指令序列
    for (const auto& keyword:KEY_WORDS::keywords)
    {
        std::vector<int>single_keyword_seq;
        for (wchar_t ch: keyword)
        {
            // 使用字符串流进行字符编码转换
            std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
            std::string utf8_str = converter.to_bytes(ch);

            for (const auto& token_pair:tokens_table)
            {
                if (token_pair.first==utf8_str)
                {
                    single_keyword_seq.push_back(token_pair.second);
                    break;
                }

            }

        }
        default_keywords_seq.push_back(single_keyword_seq);
    }


    return -1;
}

int PostDecoder::match_and_output(){


    //输出检测结果
    std::cout << "output_results:" << std::endl;
    for (int index : decoded_seq)
    {
        for (const auto& pair : tokens_table)
        {
            if (pair.second == index)
            {
                std::cout << pair.first << " ";
                commandsrecognizer->onNewWord(pair.first);
                break;
            }
        }
    }


    for (const auto & i : default_keywords_seq) {
        int code = isSubArray(i,decoded_seq);
        if (code ==1 ){

            if (decoded_seq.size() == 1) {
                if (decoded_seq[0] == 2494) {
                    return 2;
                }
            }else if (decoded_seq.size() == 2){
                if (decoded_seq[1] == 2494) {
                    return 2;
                }
            }
            printf("识别到唤醒词\n");
            return 1;
        }
    }
    return 0;

//    if (isSubArray(default_keywords_seq[0],decoded_seq))
//
//        return 1;
//    else
//        return 0;


//    //统计关键字匹配到的字数
//    std::vector<int>count(int(default_keywords_seq.size()),0);
//    for (int i = 0; i < default_keywords_seq.size(); ++i)
//    {
//        for (auto str_code:decoded_seq)
//        {
//            for (auto seq_code:default_keywords_seq[i])
//            {
//                if (str_code==seq_code)
//                    count[i]++;
//
//            }
//
//        }
//
//    }
//
//    //获取最大下标值
//    int maxValue=0;
//    int maxIndex=0;
//    for (int i = 0; i < count.size(); ++i)
//    {
//        if (count[i]>=maxValue)
//        {
//            maxValue=count[i];
//            maxIndex=i;
//        }
//
//    }
//
//
//
//    if (maxValue>=2)//指定至少匹配2个以上字符
//    {
//        printf("inner_output_idx:%d\n",maxIndex+1);
//        return maxIndex+1;//因为指令序列从1开始
//    }
//    else
//        return 0;
//


}

void PostDecoder::Reset() {
    decoded_seq.clear();

}

