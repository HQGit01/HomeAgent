/**
 * @file model.cpp
 * @brief 与硬件无关的输入解码、菜单状态和电子宠物规则实现。
 *
 * 这里刻意不依赖 ESP-IDF，使同一套边界条件可以在 tests/ 中由主机
 * 程序验证。模型只通过返回值报告状态变化，调用方决定何时记录日志、
 * 刷新屏幕或保存到 NVS。
 */
#include "model.h"

Event parseCommand(char c) {
    // BLE/串口协议使用大写单字节命令；未知字符保持为 None，不改变状态。
    // switch 直接覆盖协议字节，避免把大小写转换引入协议歧义。
    switch (c) {
    case 'U': return Event::Up; case 'D': return Event::Down;
    case 'E': return Event::Select; case 'B': return Event::Back;
    case 'F': return Event::Feed; case 'P': return Event::Play;
    case 'S': return Event::Sleep; case 'W': return Event::Wake;
    case '1': return Event::LedOn; case '0': return Event::LedOff;
    case 'V': return Event::Save; default: return Event::None;
    }
}

const char *eventName(Event event) {
    // 枚举值作为数组下标前先做范围检查，避免日志路径越界访问。
    static const char *names[] = {"NONE", "UP", "DOWN", "SELECT", "BACK",
        "FEED", "PLAY", "SLEEP", "WAKE", "LED_ON", "LED_OFF", "SAVE"};
    const size_t i = static_cast<size_t>(event);
    return i < sizeof(names) / sizeof(names[0]) ? names[i] : "INVALID";
}

void Menu::move(int direction) {
    // 选择项在 [0,count) 内环绕；first 是可见窗口的首项并随选择滚动。
    if (count <= 0 || visible <= 0) { selected = first = 0; return; }
    // 非法方向按 0 处理；这样调用方传入其他数值也不会跳过菜单项。
    selected += direction > 0 ? 1 : direction < 0 ? -1 : 0;
    if (selected < 0) selected = count - 1;
    if (selected >= count) selected = 0;
    if (selected < first) first = selected;
    if (selected >= first + visible) first = selected - visible + 1;
}

void ButtonLogic::begin(bool pressed, uint32_t now) {
    // 首次采样直接建立原始/稳定状态，不伪造一次按下或释放事件。
    raw_ = stable_ = pressed; changedAt_ = pressedAt_ = now; longSent_ = false;
}

Event ButtonLogic::sample(bool pressed, uint32_t now) {
    // 先等待原始电平稳定 25 ms，再区分短按释放和 700 ms 长按。
    if (pressed != raw_) { raw_ = pressed; changedAt_ = now; }
    // changedAt_ 只记录最近一次原始变化，稳定计时从该时刻重新开始。
    // 无符号减法可跨一次 32 位毫秒时间戳回绕；不要求时钟从零开始。
    if (stable_ != raw_ && uint32_t(now - changedAt_) >= 25) {
        stable_ = raw_;
        if (stable_) { pressedAt_ = now; longSent_ = false; }
        else if (!longSent_) return Event::Select;
    }
    if (stable_ && !longSent_ && uint32_t(now - pressedAt_) >= 700) {
        // 长按事件在达到阈值的采样轮发送一次，并用 longSent_ 锁住后续轮次。
        longSent_ = true;
        return Event::Back; // 长按释放时不再产生短按。
    }
    return Event::None;
}

void EncoderLogic::begin(uint8_t ab, int transitions) {
    // transitions 表示输出一步所需的合法相位转移数，异常配置回退为四相。
    previous_ = ab & 3; accumulated_ = 0;
    threshold_ = transitions > 0 ? transitions : 4;
}

int EncoderLogic::sample(uint8_t ab) {
    // 下标由旧两位与新两位拼接；合法正向累计，反向抖动抵消。
    static const int8_t delta[16] = {0,1,-1,0, -1,0,0,1, 1,0,0,-1, 0,-1,1,0};
    ab &= 3;
    if ((previous_ ^ ab) == 3) accumulated_ = 0; // 两位同时跳变：漏采样或噪声。
    else accumulated_ += delta[(previous_ << 2) | ab];
    previous_ = ab;
    // 无论本次是否产生一步，都要把新相位作为下一次比较的旧相位。
    if (accumulated_ >= threshold_) { accumulated_ = 0; return 1; }
    if (accumulated_ <= -threshold_) { accumulated_ = 0; return -1; }
    return 0;
}

static uint8_t clamp(int v) { return v < 0 ? 0 : v > 100 ? 100 : uint8_t(v); }

bool Pet::apply(Event e) {
    // 只处理即时动作；睡眠、能量不足或属性已满时拒绝动作且不改状态。
    // 每个分支先检查前置条件，只有成功动作才修改属性并返回 true。
    switch (e) {
    case Event::Feed:
        if (sleeping || food == 100) return false;
        food = clamp(food + 15); return true;
    case Event::Play:
        if (sleeping || energy < 10) return false;
        energy -= 10; happy = clamp(happy + 10); food = clamp(food - 3); return true;
    case Event::Sleep:
        if (sleeping) return false;
        sleeping = true; return true;
    case Event::Wake:
        if (!sleeping) return false;
        sleeping = false; return true;
    default: return false;
    }
}

bool Pet::advance(uint32_t elapsedMs) {
    // 用余量保存不足一个更新周期的时间；64 位相加防止长间隔溢出。
    const uint64_t total = uint64_t(remainderMs) + elapsedMs;
    const uint32_t steps = total / 10000;
    // 10000 毫秒为一个离散规则步；除法取整，余数留给下一次调用。
    remainderMs = total % 10000;
    if (!steps) return false;
    const uint8_t f = food, h = happy, e = energy;
    // 保存旧值用于返回“界面是否需要刷新”，而不是比较时间本身。
    const int n = steps > 100 ? 100 : int(steps);
    // 所有规则都是线性饱和变化，长间隔不必循环补跑几万次。
    food = clamp(food - n); happy = clamp(happy - n);
    energy = clamp(energy + (sleeping ? 3 * n : -n));
    return f != food || h != happy || e != energy;
}

bool Pet::valid() const { return food <= 100 && happy <= 100 && energy <= 100; }

void encodePet(const Pet &p, uint8_t out[saveSize]) {
    // 固定 9 字节布局便于跨编译器/架构读取：头标识、版本、三项属性、
    // 睡眠标志、保留字节和 XOR 校验字节。
    out[0] = 'P'; out[1] = 'T'; out[2] = 1; // 魔数和格式版本。
    out[3] = p.food; out[4] = p.happy; out[5] = p.energy;
    out[6] = p.sleeping ? 1 : 0; out[7] = 0; out[8] = 0;
    // sleeping 规范化为 0/1；字节 7 保留给未来版本，当前必须为零。
    for (size_t i = 0; i < saveSize - 1; ++i) out[8] ^= out[i];
    // XOR 只是教学中的简单损坏检查，不是强校验或加密。
}

bool decodePet(const uint8_t *data, size_t size, Pet &pet) {
    // 先验证长度、格式、校验和及属性范围，再一次性提交 candidate。
    if (!data || size != saveSize) return false;
    uint8_t check = 0;
    // 校验覆盖除校验字节外的全部字段，任何单字节改动都会改变结果。
    for (size_t i = 0; i < size - 1; ++i) check ^= data[i];
    if (data[0] != 'P' || data[1] != 'T' || data[2] != 1 ||
        data[6] > 1 || data[7] != 0 || data[8] != check) return false;
    Pet candidate;
    candidate.food = data[3]; candidate.happy = data[4]; candidate.energy = data[5];
    candidate.sleeping = data[6] != 0;
    if (!candidate.valid()) return false;
    pet = candidate; // 验证全部成功后才替换调用方对象。
    return true;
}
