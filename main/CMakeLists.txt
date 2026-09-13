# 宏只作用于入口组件；每课源码保存在 lessons 目录。
# 显式列出源码：ESP-IDF 会用脚本模式先解析组件依赖，不能用 CONFIGURE_DEPENDS。
idf_component_register(SRCS "main.cpp"
    "../lessons/00_hello.cpp" "../lessons/01_timing.cpp"
    "../lessons/02_button.cpp" "../lessons/03_encoder.cpp"
    "../lessons/04_queue.cpp" "../lessons/05_ble_connect.cpp"
    "../lessons/06_ble_led.cpp" "../lessons/07_menu_serial.cpp"
    "../lessons/08_screen.cpp" "../lessons/09_drawing.cpp"
    "../lessons/10_menu.cpp" "../lessons/11_submenu.cpp"
    "../lessons/12_partial.cpp" "../lessons/13_animation.cpp"
    "../lessons/14_pet.cpp" "../lessons/15_save.cpp"
    "../lessons/16_ble_pet.cpp" "../lessons/17_pocket_pet.cpp"
    "../lessons/99_selftest.cpp"
    INCLUDE_DIRS "." REQUIRES learning)
target_compile_definitions(${COMPONENT_LIB} PRIVATE COURSE_LESSON=${COURSE_LESSON})
