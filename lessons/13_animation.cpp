/**
 * @file 13_animation.cpp
 * @brief 第 13 课：以 250 ms 节拍切换动画帧并验证局部重绘无残影。
 *
 * paintPet() 在相邻帧使用同一局部区域，PetUi 会先绘制背景再画新宠物，
 * 因而无需保存旧精灵位图即可清理旧位置。
 */
#include "course.h"
#include "display.h"

void lesson13() {
    // 输入仍可切换页面；动画帧只在固定时间到达时翻转。
    inputsBegin(); displayBegin();
    PetUi ui; Pet pet; ui.begin();
    uint32_t last=nowMs(); int frame=0;
    // frame 只取 0/1 两个姿态，时间戳负责把循环频率与动画频率解耦。
    for (;;) {
        Event event;
        for (int i=0;i<8 && readEvent(event);++i) ui.input(event);
        if (uint32_t(nowMs()-last)>=250) {
            // 异或 1 在两个合法帧之间切换，不创建额外的动画状态。
            last=nowMs(); frame^=1;
        }
        // 旧位置和新位置都在宠物区域中，重绘背景后不会留下残影。
        ui.draw(pet,frame,false);
        pauseMs(2);
    }
}
