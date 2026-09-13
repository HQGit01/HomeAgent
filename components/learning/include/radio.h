/**
 * @file radio.h
 * @brief PocketPet BLE GATT 服务的最小业务接口。
 *
 * 调用者只需初始化服务、查询连接状态并发布最新模型快照；命令会由
 * radio.cpp 转换后进入共享输入队列，不在 BLE 回调中直接修改 Pet。
 */
#pragma once
#include "model.h"

// 调用顺序：storageBegin -> inputsBegin -> radioBegin。
// GATT 回调只验证/入队；唯一应用任务执行宠物规则和绘图。
void radioBegin(); // 初始化 NimBLE、注册服务并启动主机事件任务。
bool radioConnected(); // 返回最近一次 GAP 连接状态；读取的是原子快照而非阻塞查询。
// accepted 只编码最近动作结果，pet 内容则是发布时刻的完整属性快照。
void radioPublish(const Pet &pet, bool accepted=true); // 更新 Read/Notify 快照。
