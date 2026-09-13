/**
 * @file 07_menu_serial.cpp
 * @brief 第 07 课：让编码器和 BLE 单字节 U/D 命令共同驱动菜单。
 *
 * 菜单只消费统一 Event，不关心事件来源；Menu 维护选择项和四行可见
 * 窗口，越过边界时环绕并同步滚动窗口。
 */
#include "course.h"
#include "radio.h"

void lesson07() {
    // labels 数组与 Menu::count 一一对应，日志用于观察窗口滚动。
    ESP_ERROR_CHECK(storageBegin()); inputsBegin(); radioBegin();
    Menu menu{5,4,0,0};
    // 聚合初始化依次对应 count、visible、selected、first 四个菜单字段。
    const char *labels[]={"PET","FEED","PLAY","SLEEP","SETTINGS"};
    ESP_LOGI("L07","selected=%s",labels[menu.selected]);
    for (;;) {
        Event event;
        // U/D 命令和编码器事件都在此被消费，后续逻辑不再区分来源。
        for (int i=0;i<8 && readEvent(event);++i) {
            if (event==Event::Up || event==Event::Down) {
                // Up 传 -1、Down 传 +1，Menu 内部负责首尾环绕和窗口滚动。
                menu.move(event==Event::Up?-1:1);
            }
            // 手机 U/D/E 与编码器投递同一种枚举，菜单不判断输入来源。
            ESP_LOGI("L07","%s selected=%d %s first=%d",eventName(event),menu.selected,
                     labels[menu.selected],menu.first);
        }
        pauseMs(2);
    }
}
