/**
 * @file 11_submenu.cpp
 * @brief 第 11 课：在顶层菜单下加入 SETTINGS 信息子菜单。
 *
 * Settings 页面有八项但一次显示四项；PetUi 保存每个 Menu 的选择和
 * 首项，返回父级时不会重置用户的导航位置。
 */
#include "course.h"
#include "display.h"

void lesson11() {
    // 本课仍使用全屏刷新，重点观察页面层级和返回路径。
    inputsBegin(); displayBegin();
    PetUi ui; Pet pet;
    // page 和两个 Menu 都封装在 ui 内，课程只负责投递输入和触发绘制。
    ui.begin(false,true,true);
    for (;;) {
        Event event;
        for (int i=0;i<8 && readEvent(event);++i) {
            Event action=ui.input(event);
            // 子菜单导航通常返回 None，真正业务动作才会显示在页脚。
            if (action!=Event::None) ui.message(eventName(action));
        }
        // SETTINGS 有八项而只显示四行；返回保留父级的 selected/first。
        ui.draw(pet,0,false);
        pauseMs(2);
    }
}
