/**
 * @file 08_screen.cpp
 * @brief 第 08 课：显示屏硬件验收图，检查颜色、字节序、偏移和裁剪。
 *
 * 三条 RGB 颜色带覆盖整屏，四角标记用于判断坐标方向和可视区域；
 * 若颜色或角落不正确，应先调整 board_config.h，而不是改绘图逻辑。
 */
#include "course.h"
#include "display.h"

void lesson08() {
    // 只初始化一次 LCD，然后提交一块完整的 240x240 测试画面。
    displayBegin();
    // 初始化阶段会建立 SPI、面板对象和 DMA 扫描带，后续绘制复用这些资源。
    displayPaint({0,0,240,240},[](Canvas &c,void *) {
        // 回调在每条扫描带执行；同一组绝对坐标因此可被裁剪重绘。
        // 色条检验 RGB/BGR 和字节序；四角标记检验偏移与裁剪。
        c.rect({0,0,80,240},color::red);
        c.rect({80,0,80,240},color::green);
        c.rect({160,0,80,240},color::blue);
        c.text(4,4,"TL",color::white); c.text(218,4,"TR",color::white);
        c.text(4,224,"BL",color::white); c.text(218,224,"BR",color::white);
    });
    ESP_LOGI("L08","Check all four corners and RGB bars before continuing.");
}
