/**
 * @file 10_menu.cpp
 * @brief 第 10 课：演示顶层菜单导航和全屏重绘。
 *
 * PetUi 负责把 Up/Down/Select 转成页面或业务动作；本课关闭局部刷新，
 * 每次状态变化都提交完整画面，便于先验证布局而不引入脏区域问题。
 */
#include "course.h"
#include "display.h"

void lesson10() {
    // 从根菜单开始且隐藏子菜单，输入动作只显示在页脚消息中。
    inputsBegin(); displayBegin();
    PetUi ui; Pet pet;
    // Pet 使用默认属性，ui 单独保存页面/菜单状态，两者可独立演示。
    ui.begin(false,true,false); // 全屏刷新、从菜单开始、暂不启用子菜单。
    for (;;) {
        Event event;
        for (int i=0;i<8 && readEvent(event);++i) {
            Event action=ui.input(event);
            // UI 消费导航事件；返回的 action 才是需要交给业务模型的动作。
            if (action!=Event::None) ui.message(eventName(action));
        }
        ui.draw(pet,0,false);
        pauseMs(2);
    }
}
