/**
 * @file main.cpp
 * @brief ESP-IDF 应用入口和课程选择分发器。
 *
 * COURSE_LESSON 在构建配置中选择一个课程函数。所有课程仍参与编译，
 * 但最终只通过函数表调用所选课程；99 号选择无硬件模型自测。
 */
// 原生 ESP-IDF 支持 C/C++；这不是 Arduino，不存在 setup()/loop()。
#include "course.h"
void lesson00(); void lesson01(); void lesson02(); void lesson03(); void lesson04();
void lesson05(); void lesson06(); void lesson07(); void lesson08(); void lesson09();
void lesson10(); void lesson11(); void lesson12(); void lesson13(); void lesson14();
void lesson15(); void lesson16(); void lesson17(); void modelSelfTest();
extern "C" void app_main(void) {
    // ESP-IDF 从 app_main 开始，课程本身决定是否进入永久循环。
    // COURSE_LESSON 来自编译配置，日志先记录选择结果便于确认刷入的固件版本。
    ESP_LOGI("course", "Native ESP-IDF lesson %d", COURSE_LESSON);
#if COURSE_LESSON == 99
    modelSelfTest();
#else
    // 构建时选择一课；所有课程参与编译，未调用的代码由链接器回收。
    static void (*const lessons[])()={lesson00,lesson01,lesson02,lesson03,lesson04,
        lesson05,lesson06,lesson07,lesson08,lesson09,lesson10,lesson11,lesson12,
        lesson13,lesson14,lesson15,lesson16,lesson17};
    // 数组下标由构建配置保证在 00..17；调用后由课程自己管理生命周期。
    lessons[COURSE_LESSON]();
#endif
}
