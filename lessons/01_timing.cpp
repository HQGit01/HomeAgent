/**
 * @file 01_timing.cpp
 * @brief 第 01 课：用单调毫秒时钟实现互不干扰的周期任务。
 *
 * lastLed 和 lastLog 分别记录两项工作上次执行时间；无符号差值可
 * 正确处理一次 32 位毫秒计数回绕。循环末尾主动休眠，避免忙等占满 CPU。
 */
#include "course.h"

void lesson01() {
    // LED 每 250 ms 翻转，日志每 1000 ms 递增一次，不依赖阻塞式延时。
    ledBegin();
    uint32_t lastLed=nowMs(),lastLog=nowMs(),count=0;
    // 两个起始时间都取自同一单调时钟；count 只统计日志次数，不代表真实秒数。
    bool on=false;
    for (;;) {
        const uint32_t now=nowMs();
        // 每轮只读取一次 now，保证本轮两个比较使用同一个时间基准。
        // 两个独立时间戳：改变灯的周期不会改变日志周期。
        if (uint32_t(now-lastLed)>=250) {
            // 先更新时间戳再改变输出，下一轮从当前周期重新计时。
            lastLed=now; on=!on; ledSet(on);
        }
        if (uint32_t(now-lastLog)>=1000) {
            lastLog=now; ESP_LOGI("L01","seconds=%lu",(unsigned long)++count);
        }
        pauseMs(1); // 给其他任务运行机会，避免空转占用 CPU。
    }
}
