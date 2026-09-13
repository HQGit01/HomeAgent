/**
 * @file 05_ble_connect.cpp
 * @brief 第 05 课：初始化 BLE 并把连接状态映射到板载 LED。
 *
 * BLE 协议栈在独立 NimBLE 任务中运行，本课只轮询 radioConnected()；
 * 收到的命令会从队列取出并丢弃，因此专注观察连接/断开和自动重广播。
 */
#include "course.h"
#include "radio.h"

void lesson05() {
    // 初始化顺序先准备 NVS，再启动输入、LED 和 BLE 服务。
    ESP_ERROR_CHECK(storageBegin());
    inputsBegin(); ledBegin(); radioBegin();
    bool old=false;
    // old 用于边沿检测，只在连接状态变化时打印日志，避免每轮刷屏。
    for (;;) {
        bool connected=radioConnected();
        // 读取一次连接快照，同时驱动 LED 和比较日志，避免两次读取不一致。
        ledSet(connected);
        if (old!=connected) ESP_LOGI("L05","connected=%d",connected);
        old=connected;
        // 本课只观察连接；共用 GATT 服务已注册，但不执行其业务命令。
        Event event; for (int i=0;i<8 && readEvent(event);++i) {}
        pauseMs(20);
    }
}
