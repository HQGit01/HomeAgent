/**
 * @file 06_ble_led.cpp
 * @brief 第 06 课：使用 BLE/旋钮命令控制 LED，并周期发布状态快照。
 *
 * GATT 写入和本地输入最终都成为 Event；本课只接受 LedOn/LedOff，
 * 其余事件记录为拒绝。每秒发布一次 Read/Notify 可见的示例状态。
 */
#include "course.h"
#include "radio.h"

void lesson06() {
    // status 仅作为协议演示数据，尚未接入完整宠物模型。
    ESP_ERROR_CHECK(storageBegin()); inputsBegin(); ledBegin(); radioBegin();
    Pet status; uint32_t last=nowMs(); bool accepted=true;
    // status 仅提供合法的三项属性给 BLE 快照；accepted 表示最近命令结果。
    for (;;) {
        Event event;
        // 每轮最多取 8 项，避免命令洪水阻塞下一次状态发布。
        for (int i=0;i<8 && readEvent(event);++i) {
            // 先计算是否属于本课支持的两个事件，再决定是否改 GPIO。
            accepted=event==Event::LedOn || event==Event::LedOff;
            if (accepted) ledSet(event==Event::LedOn);
            ESP_LOGI("L06","%s accepted=%d",eventName(event),accepted);
        }
        // 每秒发布同一格式的示例状态，用于观察 Read 与 Notify 的区别。
        if (uint32_t(nowMs()-last)>=1000) { last=nowMs(); radioPublish(status,accepted); }
        pauseMs(2);
    }
}
