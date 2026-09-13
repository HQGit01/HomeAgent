/**
 * @file model.h
 * @brief 学习工程共享的事件、菜单、输入解码和宠物数据模型接口。
 *
 * 这些声明保持平台无关，既供 ESP32 固件使用，也供 tests/ 的主机测试
 * 使用。时间参数均为单调毫秒计数；属性统一限制在 0..100。
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

// 此文件不依赖 ESP-IDF 或外设：菜单、输入解码和游戏规则可在主机上测试。
enum class Event : uint8_t {
    // 底层统一使用枚举，避免菜单/蓝牙/按键分别定义相互不兼容的命令。
    None, Up, Down, Select, Back, Feed, Play, Sleep, Wake, LedOn, LedOff, Save
};

// 将外部单字节协议映射为内部事件；None 表示输入不属于协议。
// BLE 和串口共用单字节 ASCII 协议；非法字节返回 None。
Event parseCommand(char c);
const char *eventName(Event event);

struct Menu {
    int count = 0;
    int visible = 4;
    int selected = 0;
    int first = 0; // 可见窗口首项下标，不是屏幕坐标。
    // direction 为 -1 或 +1；空菜单不取模，选择项和窗口保持为 0。
    void move(int direction);
};

// 按钮滤波只处理已经采样的电平，便于用人工时间重现抖动。
class ButtonLogic {
public:
    void begin(bool pressed, uint32_t now);
    // 采样后返回一次性事件；持续按住不会重复发送长按事件。
    Event sample(bool pressed, uint32_t now);
private:
    // raw_ 是最近采样电平，stable_ 是通过 25 ms 滤波后的业务电平。
    bool raw_ = false, stable_ = false, longSent_ = false;
    uint32_t changedAt_ = 0, pressedAt_ = 0;
};

// 两路相位编码器：合法转移累计，反向抖动互相抵消，非法跳变丢弃。
class EncoderLogic {
public:
    void begin(uint8_t ab, int transitions = 4);
    // 返回 -1、0、+1；调用方可反转方向，非法两位跳变被忽略。
    int sample(uint8_t ab);
private:
    // previous_ 保存上一次两位相位，accumulated_ 保存尚未凑满一步的转移量。
    uint8_t previous_ = 0;
    int accumulated_ = 0, threshold_ = 4;
};

struct Pet {
    uint8_t food = 80, happy = 70, energy = 90;
    bool sleeping = false;
    uint32_t remainderMs = 0; // 不存档的不足 10 秒余量，避免每次更新丢失时间。
    // 返回动作是否被接受；睡眠时禁止喂食/玩耍，拒绝时对象不变。
    bool apply(Event event);
    // 按 10 秒离散步进衰减/恢复属性，并返回可见属性是否变化。
    bool advance(uint32_t elapsedMs);
    bool valid() const;
};

// 使用显式字节格式，不直接保存含有填充字节的 C++ 结构体。
constexpr size_t saveSize = 9;
// 编解码接口只操作显式字节数组，避免结构体布局和填充字节不稳定。
void encodePet(const Pet &pet, uint8_t out[saveSize]);
bool decodePet(const uint8_t *data, size_t size, Pet &pet);
