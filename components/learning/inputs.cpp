/**
 * @file inputs.cpp
 * @brief 物理输入层：初始化旋转编码器/按键，并把解码结果送入事件队列。
 *
 * 输入任务以固定周期读取 GPIO。编码器的相位解码和按键消抖由
 * model.h 中的纯逻辑类完成，本文件只负责 ESP-IDF GPIO、FreeRTOS
 * 队列以及任务调度之间的连接。队列满时丢弃新事件并累计计数，
 * 从而避免输入设备阻塞蓝牙或界面任务。
 */
#include "course.h"
#include "freertos/queue.h"
#include <atomic>

namespace {
QueueHandle_t events = nullptr;
std::atomic<uint32_t> dropped{0};
bool useEncoder = true;
uint8_t readAB() {
    // A 相移到高位、B 相保留低位，得到 0..3 的相位编号供解码器查表。
    return uint8_t((gpio_get_level(gpio_num_t(board::encoderA)) << 1) |
                  gpio_get_level(gpio_num_t(board::encoderB)));
}
void inputTask(void *) {
    ButtonLogic button;
    EncoderLogic encoder;
    button.begin(gpio_get_level(gpio_num_t(board::encoderButton)) == 0, nowMs());
    encoder.begin(readAB(), board::encoderTransitions);
    TickType_t next = xTaskGetTickCount();
    // next 保存绝对唤醒刻度；DelayUntil 比“本轮结束后再延时”更不易漂移。
    // 使用 vTaskDelayUntil 保持约 1 ms 的采样节拍，时间抖动不会逐轮累积。
    for (;;) {
        if (useEncoder) {
            int d = encoder.sample(readAB());
            // d 只在累计到完整一步时非零，零值表示本次采样尚未形成动作。
            if (board::encoderReverse) d = -d;
            if (d) postEvent(d > 0 ? Event::Down : Event::Up);
        }
        // 按键按下为低电平，先转换为逻辑 true，再交给同一套消抖状态机。
        postEvent(button.sample(gpio_get_level(gpio_num_t(board::encoderButton)) == 0, nowMs()));
        // 每 1 ms 采样；只适合手动低速旋钮，不能保证高速旋转绝不漏步。
        vTaskDelayUntil(&next, pdMS_TO_TICKS(1) ? pdMS_TO_TICKS(1) : 1);
    }
}
}

void ledBegin() {
    // LED 只配置为推挽输出；初始状态关闭，避免启动时短暂误亮。
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << board::led;
    cfg.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&cfg));
    ledSet(false);
}

void ledSet(bool on) {
    // 把逻辑“开/关”转换为板级配置指定的有效电平。
    ESP_ERROR_CHECK(gpio_set_level(gpio_num_t(board::led), on == board::ledActiveHigh));
}

void inputsBegin(bool encoder) {
    // 该入口只允许初始化一次；重复创建队列/任务会破坏全局输入状态。
    configASSERT(events == nullptr);
    useEncoder = encoder;
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = (1ULL << board::encoderA) | (1ULL << board::encoderB) |
                      (1ULL << board::encoderButton);
    // 位掩码一次选择三个输入脚，避免分别配置造成中途读到不一致的状态。
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&cfg));
    events = xQueueCreate(16, sizeof(Event)); // 按值复制枚举，不传临时对象指针。
    // 队列深度按事件个数计算；消费者短暂繁忙时最多缓存 16 个动作。
    configASSERT(events != nullptr);
    // ESP-IDF 的任务栈参数单位为字节；此任务不绘图、不通信、不存档。
    const BaseType_t created=xTaskCreate(inputTask, "encoder", 3072, nullptr, 3, nullptr);
    // 不把有副作用的任务创建放进断言，关闭断言也必须执行创建操作。
    if (created!=pdPASS) { ESP_LOGE("input","Cannot create input task"); abort(); }
}

bool postEvent(Event event) {
    if (event == Event::None) return true;
    // None 是“无动作”占位值，不进入队列，避免无效采样耗尽容量。
    if (events && xQueueSend(events, &event, 0) == pdTRUE) return true;
    ++dropped; // 队列满：拒绝新事件，不阻塞蓝牙主机；调用方可用计数诊断丢步。
    return false;
}
bool readEvent(Event &event) {
    // 零等待读取让主循环保持非阻塞；没有事件时立即把控制权还给调用方。
    return events && xQueueReceive(events, &event, 0) == pdTRUE;
}
uint32_t droppedEvents() { return dropped.load(); }
