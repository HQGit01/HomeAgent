/**
 * @file 17_pocket_pet.cpp
 * @brief 第 17 课：PocketPet 完整应用，整合输入、模型、LCD、NVS 和 BLE。
 *
 * 主循环按职责分段处理时间推进、最多八个输入、手动/自动保存、通知、
 * UI 绘制和诊断日志。dirty 表示内存状态尚未持久化，autoSave 仅在存档
 * 有效或不存在时开启，损坏存档不会被后台静默覆盖。
 */
#include "course.h"
#include "display.h"
#include "radio.h"
#include "nvs.h" // ESP_ERR_NVS_NOT_FOUND 用于区分首次启动与损坏存档。

void lesson17() {
    // 先读存档再启动外围设备，使启动消息能明确报告存档状态。
    ESP_ERROR_CHECK(storageBegin());
    Pet pet;
    const esp_err_t loaded=loadPet(pet);
    // loaded 只描述读取结果；pet 在成功前仍是默认值，避免未初始化访问。
    // 未知/损坏存档保留在 NVS，不自动覆盖；用户明确 SAVE 后才替换。
    bool autoSave=loaded==ESP_OK || loaded==ESP_ERR_NVS_NOT_FOUND;
    // 首次启动允许后台建档；损坏数据则要求用户明确保存，防止静默覆盖证据。
    ESP_LOGI("pet","load=%s",esp_err_to_name(loaded));
    inputsBegin(); displayBegin(); ledBegin(); radioBegin();
    PetUi ui; ui.begin();
    if (!autoSave) ui.message("BAD SAVE - MANUAL SAVE");
    uint32_t last=nowMs(),animated=last,saved=last,published=last,reported=last;
    bool dirty=false,accepted=true; int frame=0;
    // dirty 代表内存模型比 Flash 更新；accepted 只表示最近一次事件处理结果。
    for (;;) {
        uint32_t now=nowMs();
        // 一个循环只取一次当前时刻，保证所有 250 ms/60 s 节拍互相可比较。
        dirty|=pet.advance(uint32_t(now-last)); last=now;
        Event event;
        for (int i=0;i<8 && readEvent(event);++i) {
            Event action=ui.input(event);
            // 页面导航先被 UI 消费，只有返回的业务动作才触碰模型或外设。
            if (action==Event::None) continue;
            if (action==Event::Save) {
                // 手动 SAVE 立即执行校验、编码和 commit，并把结果反馈到页脚。
                const esp_err_t error=savePet(pet); accepted=error==ESP_OK;
                ui.message(accepted?"SAVED":"SAVE FAILED");
                if (accepted) { dirty=false; autoSave=true; saved=now; }
            } else if (action==Event::LedOn || action==Event::LedOff) {
                ledSet(action==Event::LedOn); accepted=true;
            } else {
                accepted=pet.apply(action); dirty|=accepted;
                ui.message(accepted?eventName(action):"ACTION REJECTED");
            }
            ESP_LOGI("pet","action=%s accepted=%d",eventName(action),accepted);
        }
        if (uint32_t(now-animated)>=250) { animated=now; frame^=1; }
        // 自动保存只在允许、确有改动且距离上次尝试满 60 秒时执行。
        if (autoSave && dirty && uint32_t(now-saved)>=60000) {
            const esp_err_t error=savePet(pet);
            saved=now; // 失败时也限制重试频率，防止每轮重写 Flash。
            if (error==ESP_OK) dirty=false;
            else { ui.message("SAVE FAILED"); ESP_LOGE("pet","save: %s",esp_err_to_name(error)); }
        }
        // BLE 快照按 250 ms 合并发布，不把通知当作每个事件的可靠回执。
        if (uint32_t(now-published)>=250) { published=now; radioPublish(pet,accepted); }
        ui.draw(pet,frame,radioConnected());
        if (uint32_t(now-reported)>=10000) {
            reported=now;
            ESP_LOGI("pet","heap=%lu min_heap=%lu queue_dropped=%lu pixel_bytes=%llu",
                (unsigned long)esp_get_free_heap_size(),(unsigned long)esp_get_minimum_free_heap_size(),
                (unsigned long)droppedEvents(),(unsigned long long)displayBytes());
        }
        pauseMs(2);
    }
}
