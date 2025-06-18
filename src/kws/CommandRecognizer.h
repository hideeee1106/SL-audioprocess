//
// Created by s4552 on 25-6-18.
//

#ifndef COMMANDRECOGNIZER_H
#define COMMANDRECOGNIZER_H



#include <iostream>
#include <deque>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <map>

using namespace std;
using namespace std::chrono;

class CommandRecognizer {
public:
    CommandRecognizer(int max_window_size = 9, int cooldown_ms = 2000)
        : maxWindowSize(max_window_size), cooldownMs(cooldown_ms) {
        lastTriggerTime = steady_clock::now() - milliseconds(cooldown_ms);
        initCommands();
    }

    void onNewWord(const string& word);
    static map<std::string, std::string> createPinyinMap();
    static std::string getPinyin(const std::string& chinese);
    static float calculateSimilarity(const std::string& s1, const std::string& s2);

    std::string fuzzyMatch(const std::string& input,
                          const std::vector<std::string>& commands,
                          float threshold);

private:
    deque<string> wordWindow;
    int maxWindowSize;
    int cooldownMs;
    time_point<steady_clock> lastTriggerTime;

    vector<std::string> commandList;

    void initCommands() {
        commandList = {
            "你好", "早上好", "晚安", "再见", "你真可爱", "我回来了", "我饿了", "好累",
            "心情不好", "谢谢你", "我喜欢你", "好无聊", "你好笨", "你真聪明", "对不起",
            "生日快乐", "做个鬼脸", "夸我", "生气", "今天天气", "你在干什么"
        };
    }

};


#endif //COMMANDRECOGNIZER_H
