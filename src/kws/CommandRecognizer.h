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
    CommandRecognizer(int max_window_size = 4, int cooldown_ms = 2000)
        : maxWindowSize(max_window_size), cooldownMs(cooldown_ms) {

        lastTriggerTime = steady_clock::now() - milliseconds(cooldown_ms);
        initCommands();
    }

    int onNewWord(const string& word);
    static map<std::string, std::string> createPinyinMap();
    static std::string getPinyin(const std::string& chinese);
    static float calculateSimilarity(const std::string& s1, const std::string& s2);

    std::string fuzzyMatch(const std::string& input,
                          const std::vector<std::string>& commands,
                          float threshold);

private:
    std::chrono::steady_clock::time_point lastWordTime = std::chrono::steady_clock::now();
    deque<string> wordWindow;
    int maxWindowSize;
    int cooldownMs;
    time_point<steady_clock> lastTriggerTime;

    vector<std::string> commandList;
    vector<std::string> keyword = {"松", "嵩", "宋", "送"};

    void initCommands() {
        commandList = {
            "打开龙头","关闭龙头","出一杯水","小松小松"};
    }

};


#endif //COMMANDRECOGNIZER_H
