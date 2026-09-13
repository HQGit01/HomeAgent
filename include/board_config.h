/**
 * @file board_config.h
 * @brief 目标开发板的 GPIO、LCD 时序和屏幕几何配置。
 *
 * 默认值针对经典 ESP32-WROOM 与 240x240 ST7789 教学接线，不保证适用于
 * ESP32-C3/S3 或其他 LCD 模块。所有硬件访问都通过这些常量集中配置，
 * 修改前应核对原理图、电平和背光供电方式。
 */
#pragma once
#include <stdint.h>

// 仅作为经典 ESP32-WROOM、4 MB Flash 的接线草案。
// 请先核对实际板卡原理图；不要将这些 GPIO 编号套用到 C3/S3 上。
namespace board {
// LED、编码器和按键的有效电平/方向会影响输入解码与显示状态。
constexpr int led = 2;              // 改成你已经点亮成功的普通 LED 引脚。
// true 表示写入 1 点亮，false 表示写入 0 点亮，驱动代码只接收逻辑状态。
constexpr bool ledActiveHigh = true;
constexpr int encoderA = 32;        // 编码器 CLK/A；只允许 3.3 V 逻辑输入。
constexpr int encoderB = 33;        // 编码器 DT/B。
constexpr int encoderButton = 27;   // SW，按下接地；没有 SW 时外接按钮。
// 一个完整旋钮卡点需要累计的合法 AB 转移数，决定灵敏度和抗抖程度。
constexpr int encoderTransitions = 4; // 实测每个卡点的合法相位变化数，可改成 2。
constexpr bool encoderReverse = false;

constexpr int lcdSck = 18;
// LCD 使用 SPI 写入；lcdCs=-1 表示模块固定片选，不能再假定共享总线。
constexpr int lcdMosi = 23;         // 屏幕 SDA 在此指 SPI 数据，不是 I2C SDA。
constexpr int lcdCs = 21;           // 模块未引出 CS 时改为 -1；总线不能共享。
constexpr int lcdDc = 22;
constexpr int lcdReset = 19;
constexpr int lcdBacklight = -1;    // 默认不由 GPIO 驱动裸背光，按模块规格供电。
constexpr bool backlightActiveHigh = true;
// width/height 参与 DMA 尺寸和所有 UI 坐标裁剪，必须与面板实际可视区域一致。
constexpr int width = 240;         // 1.54 英寸不能单独证明分辨率，仍需核对。
constexpr int height = 240;
constexpr uint32_t spiHz = 10000000; // 先用 10 MHz 通过颜色测试，再尝试提速。
constexpr uint8_t spiMode = 0;      // SPI mode 0：空闲低电平，上升沿采样。
// MADCTL 位同时影响坐标变换和颜色顺序，display.cpp 会按位读取而非整体猜测。
constexpr uint8_t madctl = 0x00;    // 方向与 RGB/BGR 位。改方向时也要核对偏移。
constexpr int xOffset = 0;
constexpr int yOffset = 0;          // 某些模块/方向需要 80，以四角测试判断。
constexpr bool invert = true;      // 某些 IPS 模块需要反显命令才能显示正确颜色。
}
