//
// Created by s4552 on 25-6-18.
//

#include "CommandRecognizer.h"


// 需要头文件
#include <chrono>
#include "pinyin_dict.h"
using namespace std::chrono;

int CommandRecognizer::onNewWord(const std::string &word) {
    auto now = steady_clock::now();

    // 1. 检查是否超过 2 秒没人说话
    if (duration_cast<milliseconds>(now - lastWordTime).count() > 1500) {
        wordWindow.clear();
        // std::cout << "[Info] 超过2秒没人说话，清空窗口" << std::endl;
    }

    // 2. 记录本次说话时间
    lastWordTime = now;

    // 3. 更新滑动窗口
    wordWindow.push_back(word);
    if (wordWindow.size() > maxWindowSize) {
        wordWindow.pop_front();
    }

    // 4. 拼接窗口内容
    std::string joined;
    for (const auto &w : wordWindow) {
        joined += w;
    }

    std::cout << "[Window] 拼接后: " << joined << std::endl;

    // 5. 模糊匹配
    std::string matched = fuzzyMatch(joined, commandList, 0.6);
    if (!matched.empty()) {
        if (duration_cast<milliseconds>(now - lastTriggerTime).count() >= cooldownMs) {
            lastTriggerTime = now;

            wordWindow.clear();
            if (matched == "你好小零" || matched=="你好小明"|| matched=="您好小零"|| matched=="小云小云"|| matched=="你好小云") {
                std::cout << "[Command Triggered] "<< "小霖小霖"<< std::endl;
                return 1;
            }
            if (matched == "打开龙头" || matched=="打开农头" || matched=="打开农投"|| matched=="打开"  ) {
                std::cout << "[Command Triggered] "<< "打开龙头"<< std::endl;
                return 2;
            }
            if (matched == "关闭"|| matched=="关必"|| matched=="关闭农投" || matched == "关必龙头" ) {
                std::cout << "[Command Triggered] "<< "关闭龙头"<< std::endl;
                return 3;
            }
            // if (matched == "出一杯水") {
            //     return 4;
            // }

            // TODO: 执行动作
        }return 0;
    }return 0;
}



int countChineseCharacters(const std::string& str) {
    int count = 0;
    for (size_t i = 0; i < str.size(); ) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if ((c & 0xF0) == 0xE0) {
            // 3-byte UTF-8，通常是汉字
            ++count;
            i += 3;
        } else if ((c & 0x80) == 0) {
            // ASCII
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte UTF-8（不常见的符号）
            i += 2;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte（比如 Emoji）
            i += 4;
        } else {
            // 非法字符
            ++i;
        }
    }
    return count;
}


// 中文转拼音函数
std::string CommandRecognizer::getPinyin(const std::string& chinese) {

    static const auto& pinyinMap = fullPinyinMap;
    std::string result;

    for (size_t i = 0; i < chinese.length(); ) {
        if (chinese[i] & 0x80) { // 中文字符
            if (i + 2 < chinese.length()) {
                std::string charStr = chinese.substr(i, 3);
                auto it = pinyinMap.find(charStr);
                result += (it != pinyinMap.end()) ? it->second : "?";
                i += 3;
            } else {
                i++;
            }
        } else { // ASCII字符
            result += tolower(chinese[i]);
            i++;
        }
    }
    return result;
}

// 计算相似度（编辑距离算法）
float CommandRecognizer::calculateSimilarity(const std::string& s1, const std::string& s2) {
    const size_t len1 = s1.size(), len2 = s2.size();
    std::vector<std::vector<size_t>> dp(len1+1, std::vector<size_t>(len2+1));

    for (size_t i = 0; i <= len1; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= len2; ++j) dp[0][j] = j;

    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            dp[i][j] = std::min({
                dp[i-1][j] + 1,
                dp[i][j-1] + 1,
                dp[i-1][j-1] + (s1[i-1] == s2[j-1] ? 0 : 1)
            });
        }
    }

    return 1.0f - (float)dp[len1][len2] / std::max(len1, len2);
}

std::string CommandRecognizer::fuzzyMatch(const std::string& input,
                                          const std::vector<std::string>& commands,
                                          float threshold) {
    std::string inputPinyin = getPinyin(input);

    std::string bestMatch;
    float maxScore = 0.0f;

    for (const auto& cmd : commands) {
        std::string cmdPinyin = getPinyin(cmd);
        float score = calculateSimilarity(inputPinyin, cmdPinyin);

        if (score >= maxScore) {
            maxScore = score;
            bestMatch = cmd;
        }
    }

    printf("maxScore: %.3f\n", maxScore);


    // 汉字数量不足两个，跳过
    if (countChineseCharacters(input) <= 2) {
        if ((bestMatch == "关闭" || bestMatch == "关必") and (maxScore > 0.6)) {
            return bestMatch;
        }
        if ((bestMatch == "打开") and (maxScore > 0.9)) {
            return bestMatch;
        }
        return "";
    }

    // 匹配到的命令如果属于“ling/ming/lin 限制类”，则再做一次拼音检查
    static const std::vector<std::string> restrictedCommands = {
        "你好小零", "你好小明", "您好小零", "你好像零", "你要下零","你好小云","小云小云",
    };

    if (std::find(restrictedCommands.begin(), restrictedCommands.end(), bestMatch) != restrictedCommands.end()) {
        std::vector<std::string> requiredSyllables = {"ling", "lin", "ming","yun"};
        bool hasRequiredPinyin = false;
        for (const auto& syllable : requiredSyllables) {
            if (inputPinyin.find(syllable) != std::string::npos) {
                hasRequiredPinyin = true;
                break;
            }
        }
        if (!hasRequiredPinyin) {
            return "";  // 未包含必要拼音，禁止匹配这些特殊命令
        }

    }

    if (bestMatch == "关闭" || bestMatch == "关必") {
        if (maxScore > 0.6) {
            return bestMatch;
        }return "";
    }

    if (bestMatch == "打开") {
        if (maxScore > 0.9) {
            return bestMatch;
        }return "";
    }



    // 处理“打开龙头”、“打开农头”、“打开农投”——必须识别出“打开”、“大开”或“把开”
    static const std::vector<std::string> requireKaikai = {
        "打开龙头", "打开农头", "打开农投"
    };
    if (std::find(requireKaikai.begin(), requireKaikai.end(), bestMatch) != requireKaikai.end()) {
        // 满足以下条件之一才能匹配
        bool hasValidOpen =
            (input.find("打开") != std::string::npos) ||
            (inputPinyin.find("da") != std::string::npos) ||
            (inputPinyin.find("kai") != std::string::npos);

        if (!hasValidOpen) {
            return "";  // 没有“打开”含义，禁止匹配
        }
    }



    return (maxScore >= threshold) ? bestMatch : "";
}
