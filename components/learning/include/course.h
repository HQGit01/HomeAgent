/**
 * @file course.h
 * @brief 各课程共用的硬件门面、时间工具、输入队列和存档接口。
 *
 * 该头文件集中声明课程需要的 ESP-IDF/FreeRTOS 类型。nowMs() 使用
 * esp_timer 的单调微秒时钟换算为毫秒，pauseMs() 至少让出一个调度周期。
 */
#pragma once
#include "model.h"
#include "board_config.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 所有时间单位在接口名或注释中标明。esp_timer 为开机后单调时钟。
// 转换为 32 位后允许自然回绕；调用方使用无符号差值比较时间间隔。
inline uint32_t nowMs() {
    // esp_timer_get_time() 返回微秒；先除以 1000 再截断，接口统一使用毫秒。
    return uint32_t(esp_timer_get_time() / 1000);
}
inline void pauseMs(uint32_t ms) {
    // FreeRTOS 延时以 tick 计数，至少等待一个 tick，避免 0 ms 变成忙循环。
    vTaskDelay(pdMS_TO_TICKS(ms) ? pdMS_TO_TICKS(ms) : 1);
}
void ledBegin(); // 配置板级 LED GPIO 并关闭 LED。
void ledSet(bool on); // 按 board::ledActiveHigh 转换逻辑电平。
void inputsBegin(bool encoder = true); // 只初始化一次，启动输入任务和队列。
bool postEvent(Event event);           // 可从普通任务/回调调用，不可从 ISR 调用。
bool readEvent(Event &event);         // 零等待取出一个事件，并把枚举值写入引用参数。
uint32_t droppedEvents();             // 返回队列满导致丢弃的累计数量。

// NVS 只初始化一次；失败上报，不自动清空用户数据。
esp_err_t storageBegin();
esp_err_t loadPet(Pet &pet); // 无存档返回 ESP_ERR_NVS_NOT_FOUND，原 pet 不变。
esp_err_t savePet(const Pet &pet);
