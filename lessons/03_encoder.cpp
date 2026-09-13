/**
 * @file 03_encoder.cpp
 * @brief 第 03 课：按 1 ms 周期采样 GPIO 并解码正交旋转编码器。
 *
 * EncoderLogic 根据相邻 AB 相位累计合法转移，抖动会抵消，两个位同时
 * 改变的非法跳变被丢弃。position 额外限制在 [-1000,1000] 防止溢出。
 */
#include "course.h"

void lesson03() {
    // 编码器 A/B 使用内部上拉，旋钮按相位方向输出增减事件。
    gpio_config_t cfg={};
    cfg.pin_bit_mask=(1ULL<<board::encoderA)|(1ULL<<board::encoderB);
    // A/B 共用同一配置，读取时可以把两相组合成一个 2 位状态。
    cfg.mode=GPIO_MODE_INPUT; cfg.pull_up_en=GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&cfg));
    auto ab=[] {
        // A 放在 bit1、B 放在 bit0，与 EncoderLogic 的 0..3 查表格式一致。
        return uint8_t((gpio_get_level(gpio_num_t(board::encoderA))<<1) |
                                gpio_get_level(gpio_num_t(board::encoderB))); };
    EncoderLogic encoder;
    encoder.begin(ab(),board::encoderTransitions);
    int position=0;
    for (;;) {
        int direction=encoder.sample(ab());
        // 每次采样最多产生一步；0 表示仍在累计合法相位转移。
        if (board::encoderReverse) direction=-direction;
        if (direction) {
            // 教学计数限制范围，避免长期运行发生有符号溢出。
            position+=direction;
            if (position>1000) position=1000;
            if (position<-1000) position=-1000;
            ESP_LOGI("L03","direction=%d position=%d",direction,position);
        }
        pauseMs(1);
    }
}
