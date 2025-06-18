//
// Created by zyj on 24-4-15.
//

#ifndef WEKWS_POSTPROCESS_PIPELINE_H
#define WEKWS_POSTPROCESS_PIPELINE_H
#include <iostream>
#include <vector>
#include <unordered_map>
#include <queue>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <memory>
#include <sstream>
#include "unordered_set"
#include "functional"
#include "fstream"
#include "kws/CommandRecognizer.h"


class PostDecoder {
public:
    PostDecoder();

    void decode(const std::vector<std::vector<float>>& probs);

    int read_token(const std::string& token_file);

    // 匹配搜索tokens_table中的内容并输出
    int match_and_output();

    void Reset();


public:
    std::vector<int> decoded_seq;
    std::vector<std::vector<int>>default_keywords_seq;
private:
    std::unordered_map<std::string, int> tokens_table;
    float thersh_score=0.25;

    shared_ptr<CommandRecognizer> commandsrecognizer;

private:
    bool isSubArray(const std::vector<int>& A, const std::vector<int>& B) {
        // 如果 A 的长度大于 B 的长度，则 A 不可能是 B 的子串
        if (A.size() > B.size()) {
            return false;
        }

        // 使用滑动窗口方法判断 A 是否为 B 的子串
        for (size_t i = 0; i <= B.size() - A.size(); ++i) {
            bool isMatch = true;
            for (size_t j = 0; j < A.size(); ++j) {
                if (A[j] != B[i + j]) {
                    isMatch = false;
                    break;
                }
            }
            if (isMatch) {
                return true;
            }
        }
        return false;
    }


};





#endif //WEKWS_POSTPROCESS_PIPELINE_H
