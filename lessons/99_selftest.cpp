/**
 * @file 99_selftest.cpp
 * @brief 第 99 课：在目标或主机上运行无硬件依赖的模型边界测试。
 *
 * 测试覆盖菜单环绕、按键抖动/回绕、编码器方向、宠物规则、时间批处理
 * 以及存档损坏检查；它调用生产实现，不复制另一套业务算法。
 */
#include "course.h"
#include "../tests/model_cases.h"

void modelSelfTest() {
    // 失败由 assert 立即暴露；全部通过后才打印成功日志。
    // 用例共享生产模型实现，覆盖硬件课程不会主动触发的边界输入。
    runModelCases();
    ESP_LOGI("TEST","All model boundary tests passed.");
}
