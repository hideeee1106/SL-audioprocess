//
// Created by s4552 on 25-6-18.
//

#include "CommandRecognizer.h"


// 需要头文件
#include <chrono>
using namespace std::chrono;

int CommandRecognizer::onNewWord(const std::string &word) {
    auto now = steady_clock::now();

    // 1. 检查是否超过 2 秒没人说话
    if (duration_cast<milliseconds>(now - lastWordTime).count() > 2000) {
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
            std::cout << "[Command Triggered] "<< matched<< std::endl;
            wordWindow.clear();
            if (matched == "小松小松") {
                return 1;
            }
            if (matched == "打开龙头") {
                return 2;
            }
            if (matched == "关闭龙头") {
                return 3;
            }
            if (matched == "出一杯水") {
                return 4;
            }

            // TODO: 执行动作
        }return 0;
    }return 0;
}

std::map<std::string, std::string> CommandRecognizer::createPinyinMap() {
    return {
        // “你好”
        {"你", "ni"}, {"尼", "ni"}, {"泥", "ni"}, {"倪", "ni"},
        {"好", "hao"}, {"号", "hao"}, {"浩", "hao"}, {"郝", "hao"},

        // “你好小霖”
        {"小", "xiao"}, {"晓", "xiao"}, {"笑", "xiao"}, {"肖", "xiao"}, {"效", "xiao"},
        {"霖", "lin"}, {"林", "lin"}, {"琳", "lin"}, {"零", "ling"}, {"邻", "lin"},
        {"少","shao"},{"像","xiang"},
           {"松","song"},{"嵩","song"},{"宋","song"},{"送","song"},{"怂","song"},{"颂","song"},


        // “早上好”
        {"早", "zao"}, {"枣", "zao"}, {"灶", "zao"},
        {"上", "shang"}, {"尚", "shang"}, {"裳", "shang"},

        // “晚安”
        {"晚", "wan"}, {"完", "wan"}, {"碗", "wan"},
        {"安", "an"}, {"岸", "an"}, {"按", "an"},

        // “再见”
        {"再", "zai"}, {"在", "zai"}, {"载", "zai"},
        {"见", "jian"}, {"件", "jian"}, {"健", "jian"},

        // “你真可爱”
        {"真", "zhen"}, {"针", "zhen"}, {"甄", "zhen"},
        {"可", "ke"}, {"柯", "ke"}, {"克", "ke"},
        {"爱", "ai"}, {"碍", "ai"}, {"矮", "ai"},

        // “我回来了”
        {"我", "wo"}, {"握", "wo"}, {"窝", "wo"},
        {"回", "hui"}, {"灰", "hui"}, {"挥", "hui"},
        {"来", "lai"}, {"莱", "lai"}, {"赖", "lai"},

        // “我饿了”
        {"饿", "e"}, {"鹅", "e"}, {"额", "e"},
        {"了", "le"},

        // “好累”
        {"累", "lei"}, {"雷", "lei"}, {"泪", "lei"},

        // “心情不好”
        {"心", "xin"}, {"新", "xin"}, {"辛", "xin"},
        {"情", "qing"}, {"请", "qing"}, {"青", "qing"},
        {"不", "bu"}, {"布", "bu"},

        // “谢谢你”
        {"谢", "xie"}, {"械", "xie"}, {"写", "xie"},

        // “我喜欢你”
        {"喜", "xi"}, {"洗", "xi"}, {"溪", "xi"},
        {"欢", "huan"}, {"环", "huan"},

        // “好无聊”
        {"无", "wu"}, {"吴", "wu"}, {"屋", "wu"},
        {"聊", "liao"}, {"撩", "liao"},

        // “你好笨”
        {"笨", "ben"}, {"本", "ben"},

        // “你真聪明”
        {"聪", "cong"}, {"葱", "cong"},
        {"明", "ming"}, {"鸣", "ming"}, {"铭", "ming"},

        // “对不起”
        {"对", "dui"}, {"队", "dui"}, {"兑", "dui"},
        {"起", "qi"}, {"其", "qi"}, {"齐", "qi"},

        // “生日快乐”
        {"生", "sheng"}, {"声", "sheng"},
        {"日", "ri"}, {"热", "re"},
        {"快", "kuai"}, {"块", "kuai"},
        {"乐", "le"}, {"约", "yue"}, {"悦", "yue"},

        // “做个鬼脸”
        {"做", "zuo"}, {"坐", "zuo"},
        {"个", "ge"}, {"各", "ge"},
        {"鬼", "gui"}, {"轨", "gui"},
        {"脸", "lian"}, {"莲", "lian"},

        // “夸我”
        {"夸", "kua"}, {"跨", "kua"},

        // “生气”
        {"气", "qi"}, {"汽", "qi"},

        // “今天天气”
        {"今天", "jintian"}, {"天天", "tiantian"},
        {"天", "tian"}, {"添", "tian"},
        {"气", "qi"},

        // “你在干什么”
        {"在", "zai"}, {"载", "zai"},
        {"干", "gan"}, {"敢", "gan"},
        {"嘛", "ma"}, {"吗", "ma"},
        {"什", "shen"}, {"甚", "shen"},
        {"么", "me"}, {"摩", "mo"},

                // 补充一些常见识别混淆字
        {"啦", "la"}, {"啊", "a"}, {"呀", "ya"},// 出一杯水
        {"出", "chu"}, {"初", "chu"}, {"楚", "chu"}, {"畜", "chu"},
        {"一", "yi"}, {"衣", "yi"}, {"伊", "yi"}, {"医", "yi"},
        {"杯", "bei"}, {"背", "bei"}, {"悲", "bei"}, {"北", "bei"},
        {"水", "shui"}, {"谁", "shui"}, {"税", "shui"},

        // 打开龙头
        {"打", "da"}, {"大", "da"}, {"答", "da"},
        {"开", "kai"}, {"凯", "kai"}, {"慨", "kai"},
        {"龙", "long"}, {"笼", "long"}, {"隆", "long"}, {"聋", "long"},
        {"头", "tou"}, {"投", "tou"}, {"透", "tou"},

        // 关闭龙头
        {"关", "guan"}, {"官", "guan"}, {"管", "guan"}, {"观", "guan"},
        {"闭", "bi"}, {"必", "bi"}, {"毕", "bi"}, {"碧", "bi"},
        // "龙头" 已列于上方



    };
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
    static const auto pinyinMap = createPinyinMap();
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

    // 汉字数量不足两个，跳过
    if (countChineseCharacters(input) < 2) {
        return "";
    }

    std::string inputPinyin = getPinyin(input);

    // 必须包含 lin / ling / ming 才能匹配
    // if (inputPinyin.find("lin") == std::string::npos &&
    //     inputPinyin.find("ling") == std::string::npos &&
    //     inputPinyin.find("ming") == std::string::npos) {
    //     return "";
    //     }

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
    return (maxScore >= threshold) ? bestMatch : "";
}
