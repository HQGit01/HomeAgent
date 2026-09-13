/**
 * @file host_main.cpp
 * @brief 主机端模型测试入口，不初始化 ESP32 外设。
 *
 * 通过普通 C++ 可执行文件运行与硬件无关的边界用例，便于在刷写固件
 * 前验证菜单、输入解码、宠物规则和存档编解码。
 */
#include "model_cases.h"
#include <stdio.h>
// 主机单元测试入口，不需要 GPIO、屏幕或手机。
int main() {
    // 测试函数内部用 assert 在第一个不变量失败处中止进程。
    runModelCases();
    puts("All model boundary tests passed.");
}
