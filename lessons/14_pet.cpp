/**
 * @file 14_pet.cpp
 * @brief 第 14 课：把输入动作接到 Pet 模型，并按经过时间更新属性。
 *
 * advance() 每十秒执行一次线性饱和变化，apply() 负责动作前置条件；
 * 只有属性确实变化时才打印状态，便于观察拒绝动作与睡眠规则。
 */
#include "course.h"

void lesson14() {
    // 本课不接屏幕或存档，重点是模型状态和事件接受/拒绝。
    inputsBegin(); Pet pet; uint32_t last=nowMs();
    // last 记录上次规则更新时刻，Pet::advance 会把差值折算为 10 秒步进。
    ESP_LOGI("L14","Turn DOWN to feed, UP to play; short press sleep, long press wake.");
    for (;;) {
        const uint32_t now=nowMs();
        // 先计算经过时间，再立即推进 last，防止本轮处理输入的耗时被重复计入。
        bool changed=pet.advance(uint32_t(now-last)); last=now;
        Event event;
        for (int i=0;i<8 && readEvent(event);++i) {
            // 本课把方向/按键映射为宠物动作，其他事件统一走 Wake 分支以便观察边界。
            const Event action=event==Event::Down?Event::Feed:event==Event::Up?Event::Play:
                               event==Event::Select?Event::Sleep:Event::Wake;
            const bool accepted=pet.apply(action); changed|=accepted;
            ESP_LOGI("L14","%s accepted=%d",eventName(action),accepted);
        }
        if (changed) ESP_LOGI("L14","food=%u happy=%u energy=%u sleeping=%d",
                             pet.food,pet.happy,pet.energy,pet.sleeping);
        pauseMs(5);
    }
}
