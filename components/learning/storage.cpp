/**
 * @file storage.cpp
 * @brief 使用 ESP-IDF NVS 保存和恢复经过校验的宠物快照。
 *
 * NVS 中只保存 model.cpp 定义的固定字节格式，不直接持久化包含
 * 对齐填充和运行时余量的 C++ 对象。读失败或数据损坏时不覆盖调用者
 * 当前对象；写入只有在 commit 成功后才算完成。
 */
#include "course.h"
#include "nvs_flash.h"
#include "nvs.h"

esp_err_t storageBegin() {
    // 初始化失败原样返回，让上层决定是否报警；不会擅自擦除用户数据。
    // 不沿用某些入门示例“初始化失败就擦除”的策略。
    // NVS 分区由 ESP-IDF 管理；这里只初始化，不在库层决定擦除策略。
    return nvs_flash_init();
}

esp_err_t loadPet(Pet &pet) {
    // 句柄的生命周期限制在本次读取内，任何错误路径都在返回前关闭句柄。
    nvs_handle_t handle;
    esp_err_t error = nvs_open("pocket_pet", NVS_READONLY, &handle);
    if (error != ESP_OK) return error;
    uint8_t bytes[saveSize];
    size_t length = sizeof(bytes);
    // 入参表示缓冲区容量，成功返回时会被 NVS 改写为实际 blob 长度。
    error = nvs_get_blob(handle, "state", bytes, &length);
    nvs_close(handle);
    if (error != ESP_OK) return error;
    return decodePet(bytes, length, pet) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

esp_err_t savePet(const Pet &pet) {
    // 先拒绝非法模型，再执行 set_blob/commit，避免把无效状态写入 Flash。
    if (!pet.valid()) return ESP_ERR_INVALID_ARG;
    uint8_t bytes[saveSize];
    encodePet(pet, bytes);
    // 先编码到栈缓冲区，只有完整固定格式准备好后才接触 NVS。
    nvs_handle_t handle;
    esp_err_t error = nvs_open("pocket_pet", NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_set_blob(handle, "state", bytes, sizeof(bytes));
    // set_blob 只修改 RAM 中的 NVS 工作区，后面的 commit 才使其持久化。
    if (error == ESP_OK) error = nvs_commit(handle); // 成功提交后才向界面报告已保存。
    nvs_close(handle);
    return error;
}
