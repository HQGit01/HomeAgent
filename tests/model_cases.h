/**
 * @file model_cases.h
 * @brief 共享模型边界用例，供固件自测和主机测试共同调用。
 *
 * 用例使用 assert 表达期望，覆盖空菜单、环绕、消抖时间回绕、编码器
 * 非法转移、宠物动作限制、长间隔更新以及每个存档字节的损坏检测。
 */
#pragma once
#include "model.h"
#include <assert.h>
#include <string.h>

// 真正调用固件共用模型，不在测试中重写一套算法。
inline void runModelCases() {
    // 菜单：空菜单保持 0，非空菜单在首尾环绕且可见窗口跟随选择。
    Menu empty; empty.move(-1); assert(empty.selected==0 && empty.first==0);
    // 下面的断言按“输入 -> 状态 -> 期望”顺序组织，便于定位哪条规则被破坏。
    Menu m{8,4,0,0};
    m.move(-1); assert(m.selected==7 && m.first==4);
    m.move(1); assert(m.selected==0 && m.first==0);
    for (int i=0;i<5;++i) m.move(1);
    assert(m.selected==5 && m.first==2);

    // 按键：验证抖动不会触发事件、短按/长按边界以及时间戳回绕。
    ButtonLogic b; b.begin(false,0);
    assert(b.sample(true,10)==Event::None);
    assert(b.sample(false,15)==Event::None); // 抖动，不形成短按。
    assert(b.sample(true,20)==Event::None);
    assert(b.sample(true,45)==Event::None);
    assert(b.sample(false,100)==Event::None);
    assert(b.sample(false,125)==Event::Select);
    assert(b.sample(false,150)==Event::None);
    b.sample(true,200); b.sample(true,225);
    assert(b.sample(true,925)==Event::Back);
    b.sample(false,930);
    assert(b.sample(false,955)==Event::None); // 长按释放不产生短按。
    b.begin(false,0xfffffff0U);
    b.sample(true,0xfffffff5U); b.sample(true,20);
    b.sample(false,50); assert(b.sample(false,75)==Event::Select);

    // 编码器：验证正反向累计、抖动抵消和两位同时跳变的丢弃策略。
    EncoderLogic encoder; encoder.begin(0,4);
    assert(encoder.sample(1)==0); assert(encoder.sample(0)==0); // 抖动抵消。
    assert(encoder.sample(1)==0); assert(encoder.sample(3)==0);
    assert(encoder.sample(2)==0); assert(encoder.sample(0)==1);
    encoder.sample(2); encoder.sample(3); encoder.sample(1);
    assert(encoder.sample(0)==-1);
    encoder.begin(0); encoder.sample(3); assert(encoder.sample(0)==0); // 非法跳变。

    // 宠物：覆盖睡眠限制、属性上下界和分段时间更新的等价性。
    Pet pet; assert(pet.apply(Event::Sleep));
    assert(!pet.apply(Event::Feed) && !pet.apply(Event::Play));
    assert(pet.apply(Event::Wake)); pet.energy=9;
    assert(!pet.apply(Event::Play)); pet.food=99;
    assert(pet.apply(Event::Feed) && pet.food==100);
    assert(!pet.apply(Event::Feed));
    Pet a,batch;
    for (int i=0;i<100;++i) a.advance(1234);
    batch.advance(123400);
    assert(a.food==batch.food && a.happy==batch.happy && a.energy==batch.energy);
    assert(a.remainderMs==batch.remainderMs);
    a.advance(0xffffffffU); assert(a.food==0 && a.happy==0 && a.energy==0);
    a.apply(Event::Sleep); a.advance(0xffffffffU); assert(a.energy==100);

    // 存档：合法快照可恢复，长度/空指针/任意字节损坏和越界属性均拒绝。
    uint8_t data[saveSize]; encodePet(pet,data);
    Pet restored; assert(decodePet(data,sizeof(data),restored));
    assert(restored.food==pet.food && restored.energy==pet.energy);
    assert(!decodePet(data,sizeof(data)-1,restored));
    assert(!decodePet(nullptr,saveSize,restored));
    for (size_t i=0;i<saveSize;++i) {
        // 每轮只翻转一个字节，验证校验覆盖头、版本、属性、保留位和自身前的数据。
        uint8_t corrupt[saveSize]; memcpy(corrupt,data,saveSize); corrupt[i]^=1;
        assert(!decodePet(corrupt,saveSize,restored));
    }
    uint8_t invalid[saveSize]; memcpy(invalid,data,saveSize);
    invalid[3]=101; invalid[8]=0;
    for (size_t i=0;i<saveSize-1;++i) invalid[8]^=invalid[i];
    assert(!decodePet(invalid,saveSize,restored)); // 校验正确但属性越界仍拒绝。
    assert(parseCommand('F')==Event::Feed && parseCommand('\0')==Event::None);
    assert(parseCommand('f')==Event::None); // 协议明确区分大小写。
}
