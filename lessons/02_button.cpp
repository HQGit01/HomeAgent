/**
 * @file 02_button.cpp
 * @brief 第 02 课：读取低电平有效的按键并演示消抖、短按与长按。
 *
 * 本课把 GPIO 采样和 ButtonLogic 放在同一任务中，便于观察状态变化；
 * 约 25 ms 稳定后才确认电平，长按约 700 ms 产生一次 Back 事件。
 */
#include "course.h"

void lesson02() {
    // 只配置按键和 LED，尚未引入异步事件队列。
    ledBegin();
    gpio_config_t cfg={};
    cfg.pin_bit_mask=1ULL<<board::encoderButton; cfg.mode=GPIO_MODE_INPUT;
    // 一个 GPIO 的掩码仍用 64 位表达，避免移位时被 32 位整数截断。
    cfg.pull_up_en=GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&cfg));
    ButtonLogic button;
    button.begin(gpio_get_level(gpio_num_t(board::encoderButton))==0,nowMs());
    bool on=false;
    // on 是应用逻辑状态；它与按键电平分离，所以松开后 LED 仍保持切换结果。
    for (;;) {
        // 本课在一个任务中理解消抖，下一阶段才引入队列。
        // GPIO 低电平表示按下，比较结果把硬件电平转换成 ButtonLogic 需要的 bool。
        Event event=button.sample(gpio_get_level(gpio_num_t(board::encoderButton))==0,nowMs());
        if (event==Event::Select) { on=!on; ledSet(on); }
        if (event!=Event::None) ESP_LOGI("L02","%s",eventName(event));
        pauseMs(1);
    }
}
