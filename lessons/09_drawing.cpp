/**
 * @file 09_drawing.cpp
 * @brief 第 09 课：组合 Canvas 基本图元绘制静态宠物和状态条。
 *
 * 该课不读取输入或修改 Pet，只演示文本、矩形和 paintPet() 在同一
 * 绝对坐标系中的叠加关系；状态条长度按固定的 0..100 比例计算。
 */
#include "course.h"
#include "display.h"

void lesson09() {
    // 用完整刷新确保屏幕上没有上一课程留下的像素。
    displayBegin();
    // 先初始化显示资源，再提交完整区域，避免回调访问尚未分配的 Canvas 缓冲区。
    displayPaint({0,0,240,240},[](Canvas &c,void *) {
        c.text(20,16,"MY FIRST PET",color::white,3);
        paintPet(c,80,65,0,false);
        c.text(20,160,"FOOD 80",color::accent,3);
        // 外框固定 200 像素，内部 160 像素对应 FOOD=80 的 80% 比例。
        c.rect({20,192,200,16},color::panel);
        c.rect({20,192,160,16},color::green);
    });
}
