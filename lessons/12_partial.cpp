/**
 * @file 12_partial.cpp
 * @brief 第 12 课：启用 PetUi 的脏区域刷新并测量提交成本。
 *
 * 只有页眉连接标志、菜单选择、主体状态或页脚消息变化时才调用
 * displayPaint；displayBytes() 统计实际发送的像素字节，不含 SPI 命令。
 */
#include "course.h"
#include "display.h"

void lesson12() {
    // 与上一课保持相同界面逻辑，只替换刷新策略以便比较。
    inputsBegin(); displayBegin();
    PetUi ui; Pet pet;
    ui.begin(true,true,true); // 与上一课相比只启用脏区域提交。
    for (;;) {
        Event event;
        for (int i=0;i<8 && readEvent(event);++i) {
            Event action=ui.input(event);
            if (action!=Event::None) ui.message(eventName(action));
        }
        uint64_t before=displayBytes(); const int64_t started=esp_timer_get_time();
        // before 用于求本轮增量，started 使用微秒以便观察一次刷新耗时。
        ui.draw(pet,0,false);
        if (displayBytes()!=before)
            // 只有实际提交像素时才输出诊断，空闲轮次不会制造噪声日志。
            ESP_LOGI("L12","pixel bytes=%llu elapsed us=%lld",(unsigned long long)(displayBytes()-before),
                     (long long)(esp_timer_get_time()-started));
        pauseMs(2);
    }
}
