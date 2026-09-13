/**
 * @file 04_queue.cpp
 * @brief 第 04 课：从输入队列取出事件，并限制每轮最多处理八项。
 *
 * 生产者是 inputs.cpp 的采样任务，当前课程任务是消费者；消费上限让
 * 500 ms LED 定时工作即使遇到输入洪水也能获得执行机会。
 */
#include "course.h"

void lesson04() {
    // 队列按值传递 Event，不共享临时对象指针。
    inputsBegin(); ledBegin();
    uint32_t last=nowMs(); bool on=false;
    // last 只服务于 LED 节拍，读取队列不会重置这个独立计时器。
    for (;;) {
        Event event;
        // event 由队列按值写入，变量生命周期覆盖本轮日志使用即可。
        // 每轮处理数量有上限，输入洪水不能让定时任务一直得不到执行。
        for (int i=0;i<8 && readEvent(event);++i)
            ESP_LOGI("L04","event=%s dropped=%lu",eventName(event),(unsigned long)droppedEvents());
        if (uint32_t(nowMs()-last)>=500) {
            // 消费完本轮事件后再检查定时任务，保证输入和指示灯都能前进。
            last=nowMs(); on=!on; ledSet(on);
        }
        pauseMs(2);
    }
}
