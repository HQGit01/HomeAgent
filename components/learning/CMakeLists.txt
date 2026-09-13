# 应用入口与公共驱动分开，按课程逐步阅读。
idf_component_register(SRCS "model.cpp" "inputs.cpp" "storage.cpp" "display.cpp" "radio.cpp"
    INCLUDE_DIRS "include" "../../include"
    REQUIRES driver esp_lcd esp_timer nvs_flash bt)
