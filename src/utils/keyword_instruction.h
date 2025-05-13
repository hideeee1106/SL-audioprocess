//
// Created by zyj on 24-4-28.
//

#ifndef WEKWS_KEYWORD_INSTRUCTION_H
#define WEKWS_KEYWORD_INSTRUCTION_H

#include "codecvt"

namespace KEY_WORDS{

    const std::wstring index1=L"问";

//    lin (lin): 437
//ling (ling): 439
//ning (ning): 518
//凛 (lin): 1107
//另 (ling): 1205
//宁 (ning): 1433
//岭 (ling): 1489
//您 (nin): 1584
//拎 (lin): 1642
//林 (lin): 1801
//泞 (ning): 1880
//灵 (ling): 1926
//赁 (lin): 2373
//零 (ling): 2525
//领 (ling): 2547

    const std::wstring index2=L"凛";

    const std::wstring index3=L"另";

    const std::wstring index4=L"宁";

    const std::wstring index5=L"岭";

    const std::wstring index6=L"您";

    const std::wstring index7=L"拎";

    const std::wstring index8=L"林";

    const std::wstring index9=L"灵";
    const std::wstring index10=L"泞";

    const std::wstring index11=L"赁";

    const std::wstring index12=L"领";

    const std::wstring index13=L"零";

    const std::vector<std::wstring>keywords={index1,index2,index3,index4,index5,
                                             index6,index7,index8,index9,index10,index11,index12,index13

    };
}



#endif //WEKWS_KEYWORD_INSTRUCTION_H
