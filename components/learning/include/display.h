/**
 * @file display.h
 * @brief ST7789 显示驱动之上的裁剪画布、宠物绘制和界面状态机接口。
 *
 * Canvas 的坐标仍是整屏绝对坐标，但像素只写入当前扫描带；displayPaint
 * 负责裁剪、DMA 提交和完成等待。PetUi 通过前后快照选择整屏或脏区域刷新。
 */
#pragma once
#include <stdint.h>
#include "model.h"

// RGB565 为 CPU 中的颜色值；驱动发送前转换成屏幕要求的高字节在前。
namespace color {
// 所有颜色均为 RGB565：5 位红、6 位绿、5 位蓝，绘制阶段保持整数格式。
constexpr uint16_t black=0x0000, white=0xffff, red=0xf800, green=0x07e0;
constexpr uint16_t blue=0x001f, background=0x0863, panel=0x1946, accent=0x07d8;
constexpr uint16_t yellow=0xff40, muted=0x9cf3;
}
struct Rect {
    // x/y 是绝对屏幕坐标，w/h 是宽高；绘图区域统一使用半开边界。
    int x, y, w, h;
};

// Canvas 只保存一个小矩形；绘图坐标始终使用屏幕绝对坐标。
// 所有绘图都裁剪到当前小块，因此同一 scene() 能重绘任意脏区域。
class Canvas {
public:
    Canvas(uint16_t *pixels, Rect area) : pixels_(pixels), area_(area) {}
    void fill(uint16_t c); // 填满当前紧密像素缓冲区。
    void pixel(int x, int y, uint16_t c); // 越出扫描带的坐标静默忽略。
    void rect(Rect r, uint16_t c); // 半开区间矩形，并裁剪到扫描带。
    void text(int x, int y, const char *s, uint16_t c, int scale=2); // ASCII 3x5 字模。
private:
    // pixels_ 指向当前扫描带的紧密 RGB565 数组，area_ 描述它覆盖的绝对范围。
    uint16_t *pixels_;
    Rect area_;
};
using Painter = void (*)(Canvas &, void *);
void displayBegin(); // 只调用一次；失败 ESP_ERROR_CHECK 会报告错误并终止启动。
// region 为空或完全越界时不提交；paint 在每条扫描带上被调用一次。
void displayPaint(Rect region, Painter paint, void *context=nullptr);
uint64_t displayBytes(); // 像素字节，不含命令和总线协议开销。
void paintPet(Canvas &canvas, int x, int y, int frame, bool sleeping);

// 对比 full-refresh 与 dirty-refresh：相同界面逻辑只改变提交区域。
class PetUi {
public:
    void begin(bool partial=true, bool startInMenu=false, bool submenus=true);
    // 消费导航事件；只有真正的业务动作才返回给上层。
    Event input(Event event);
    void draw(const Pet &pet, int frame, bool connected);
    void message(const char *text); // 文本复制到固定缓冲区，过长内容被截断。
private:
    // page_ 与 oldPage_ 用于决定整屏刷新；current_/previous_ 用于发现属性差异。
    enum class Page { Home, Menu, Settings, Info };
    Page page_=Page::Home, oldPage_=Page::Info;
    Menu root_{7,4,0,0}, settings_{8,4,0,0};
    Menu oldMenu_{};
    Pet current_{}, previous_{};
    int frame_=0, oldFrame_=-1;
    bool connected_=false, oldConnected_=false, partial_=true, submenus_=true;
    bool dirty_=true, footerDirty_=true;
    char message_[28]="READY";
    static void painter(Canvas &c, void *context);
    void scene(Canvas &c);
};
