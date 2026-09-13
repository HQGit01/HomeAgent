/**
 * @file 15_save.cpp
 * @brief 第 15 课：读取 NVS 宠物快照，并用按键触发保存。
 *
 * 启动时 loadPet() 失败只记录错误；Down/Up 分别演示喂食/玩耍，
 * Select 执行带校验和的固定格式写入，复位后可检查持久化结果。
 */
#include "course.h"

void lesson15() {
    // 读取发生在事件循环前，因而日志能区分首次启动与已有存档。
    ESP_ERROR_CHECK(storageBegin()); inputsBegin();
    Pet pet;
    // 若读取失败，Pet 保持默认安全值；loadPet 不会用坏数据覆盖它。
    const esp_err_t loaded=loadPet(pet);
    ESP_LOGI("L15","load=%s food=%u happy=%u energy=%u",esp_err_to_name(loaded),pet.food,pet.happy,pet.energy);
    ESP_LOGI("L15","DOWN feed; UP play; short press SAVE; reset to verify.");
    for (;;) {
        Event event;
        // 同一队列既承载动作又承载 SAVE，分支决定是否写入 Flash。
        for (int i=0;i<8 && readEvent(event);++i) {
            if (event==Event::Select) {
                // 保存接口内部会重新校验并在 commit 成功后返回 ESP_OK。
                const esp_err_t result=savePet(pet);
                ESP_LOGI("L15","save=%s",esp_err_to_name(result));
            } else {
                pet.apply(event==Event::Down?Event::Feed:event==Event::Up?Event::Play:Event::None);
                ESP_LOGI("L15","food=%u happy=%u energy=%u",pet.food,pet.happy,pet.energy);
            }
        }
        pauseMs(5);
    }
}
