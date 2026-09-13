/**
 * @file 16_ble_pet.cpp
 * @brief 第 16 课：将 Pet 模型接入 BLE 命令和周期状态通知。
 *
 * 本课验证模型动作与 GATT 状态快照的连接，但故意不初始化显示或
 * 存储；通知按 250 ms 合并状态，不能当作逐命令可靠确认。
 */
#include "course.h"
#include "radio.h"

void lesson16() {
    // 先更新时间，再消费最多八个事件，随后按节拍发布当前快照。
    ESP_ERROR_CHECK(storageBegin()); inputsBegin(); radioBegin();
    Pet pet; uint32_t last=nowMs(),published=last; bool accepted=true;
    // last 驱动模型时间，published 单独驱动通知频率，二者不能共用。
    for (;;) {
        const uint32_t now=nowMs();
        // 先推进自然衰减，再处理用户动作，通知反映本轮全部变化。
        pet.advance(uint32_t(now-last)); last=now;
        Event event;
        for (int i=0;i<8 && readEvent(event);++i) {
            // apply 返回 false 时不改模型；accepted 让手机知道最近动作是否被拒绝。
            accepted=pet.apply(event);
            ESP_LOGI("L16","%s accepted=%d",eventName(event),accepted);
        }
        // 这课专注协议和模型，屏幕与存档在最终课接入。
        if (uint32_t(now-published)>=250) { published=now; radioPublish(pet,accepted); }
        pauseMs(5);
    }
}
