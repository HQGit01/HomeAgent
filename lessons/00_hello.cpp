/**
 * @file 00_hello.cpp
 * @brief 第 00 课：验证 ESP-IDF 工程启动和最基本的 GPIO 输出。
 *
 * 课程只初始化板级 LED 并点亮它，用日志打印实际 GPIO。函数返回后
 * app_main 任务结束，但 GPIO 输出寄存器保持当前状态，复位才会重复。
 */
#include "course.h"

void lesson00() {
    // 该课只验证 LED 初始化、有效电平转换和 ESP_LOG 输出。
    // app_main 是 FreeRTOS 主任务内的函数，允许等待；GPIO 编号在 board_config.h。
    ledBegin();
    // 使用逻辑值而不是直接写 GPIO，验证板级 active-high/active-low 配置生效。
    ledSet(true);
    ESP_LOGI("L00","Native ESP-IDF ready; LED GPIO=%d. Reset to repeat.",board::led);
    // 返回 app_main 后主任务结束，但 GPIO 输出状态仍然保留。
}
