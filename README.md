# ESP32 从点亮 LED 到口袋宠物

这是面向已有 C/C++、Python 基础、刚开始学习 ESP32 的进阶课程。目标是独立完成一个带旋钮菜单、宠物动画、BLE 控制和掉电存档的应用，并能查文档自行增加功能。

**当前工程完全使用原生 ESP-IDF 5.4，目标为 ESP-WROOM-32。没有 PlatformIO 配置，也不依赖 Arduino。** C++ 仅用于组织模块，硬件接口直接使用 ESP-IDF API。

已提供 18 个独立课程入口、公共模块和中文注释。屏幕暂按 240×240 ST7789 配置；分辨率、偏移、背光和具体接线仍需核对实物。构建验证情况见 [验证记录](docs/VALIDATION.md)，硬件尚未验证。

**先读 [原生 ESP-IDF 实验指南](docs/START_HERE.md)**：其中有构建烧录步骤、接线草案、每课验收、BLE 协议和排错方法。不熟悉 GPIO、任务、DMA 时先看 [概念入门](docs/CONCEPTS.md)。额外器件见 [扩展路线](docs/EXTENSIONS.md)。

## 从哪里开始

1. 在 ESP-IDF Terminal 打开本目录，运行 `idf.py --version`。
2. 打开 [board_config.h](include/board_config.h)，把 LED 引脚改成你已经点亮成功的 GPIO。
3. 阅读 [第 0 课](lessons/00_hello.cpp)，编译并烧录。
4. 每课验收通过后再前进，不需要一次阅读整个驱动文件。

```text
idf.py -B build/lesson0 -DCOURSE_LESSON=0 build
idf.py -B build/lesson0 -p COM实际端口 flash monitor
```

将 COM实际端口替换为板子的实际端口。退出监视器按 Ctrl+]。课程编号取 0～17，模型自检取 99；不写前导零。不同课程使用不同构建目录。

## 学习计划

建议每周 6～8 小时，约 12～16 周。已有编程基础无需重学语言，重点补足外设、时序、任务、资源生命周期和调试。

| 阶段 | 建议时间 | 独立完成的能力 |
| --- | --- | --- |
| GPIO、时间、按键、编码器 | 第 1～3 周 | 能把原始电平转换为可靠输入事件 |
| FreeRTOS 与 BLE | 第 4～5 周 | 能用队列连接回调和业务逻辑，理解 GATT |
| 屏幕基础与菜单 | 第 6～8 周 | 能显示像素、文字，编写滚动和返回逻辑 |
| 局部刷新与动画 | 第 9～10 周 | 能定位变化区域，分析传输量与 DMA 缓冲 |
| 宠物模型、存档、整合 | 第 11～13 周 | 能完成并验证一个有状态应用 |
| 独立扩展与复盘 | 第 14～16 周 | 不看参考实现，自己设计、编写和排错 |

## 18 个进阶用例

| 编号 | 源码 | 新增能力 |
| --- | --- | --- |
| 0 | [启动与点灯](lessons/00_hello.cpp) | app_main、GPIO、日志、复位 |
| 1 | [独立时间调度](lessons/01_timing.cpp) | LED 与日志互不影响 |
| 2 | [按键消抖](lessons/02_button.cpp) | 短按、长按、释放不误触发 |
| 3 | [旋转编码器](lessons/03_encoder.cpp) | AB 相位解码、方向和卡点校准 |
| 4 | [任务与队列](lessons/04_queue.cpp) | 输入与应用解耦、队列满处理 |
| 5 | [BLE 连接](lessons/05_ble_connect.cpp) | 广播、发现、连接、断线恢复 |
| 6 | [BLE 控灯](lessons/06_ble_led.cpp) | Write、Read、Notify 与长度验证 |
| 7 | [串口菜单](lessons/07_menu_serial.cpp) | 旋钮与手机共用菜单事件 |
| 8 | [屏幕色条](lessons/08_screen.cpp) | SPI、ST7789、颜色与四角测试 |
| 9 | [文字与宠物绘图](lessons/09_drawing.cpp) | 点阵字模、图形、状态条 |
| 10 | [高亮菜单](lessons/10_menu.cpp) | 上下选择、确认与返回 |
| 11 | [滚动和子菜单](lessons/11_submenu.cpp) | selected、first、父级状态保留 |
| 12 | [局部刷新](lessons/12_partial.cpp) | 脏矩形、背景恢复、字节/耗时测量 |
| 13 | [宠物动画](lessons/13_animation.cpp) | 独立动画时钟、DMA 完成等待 |
| 14 | [宠物规则](lessons/14_pet.cpp) | 喂食、玩耍、睡眠与属性边界 |
| 15 | [NVS 存档](lessons/15_save.cpp) | 显式序列化、提交、重启恢复 |
| 16 | [BLE 宠物控制](lessons/16_ble_pet.cpp) | 通信与业务规则整合 |
| 17 | [完整口袋宠物](lessons/17_pocket_pet.cpp) | 屏幕、旋钮、BLE、动画、存档 |

第 10～13 课先做界面，第 14 课再学习规则，第 17 课才全部组合。第 16 课调整为 BLE 宠物控制；LVGL、双缓冲和高速 PCNT 编码器作为扩展，不属于当前已实现内容。

## 最终应用怎么玩

旋转选择，短按确认，长按 700 ms 返回。主页显示宠物和饱食/快乐/精力；菜单支持喂食、玩耍、睡眠、唤醒、信息子页和保存。手机 BLE 可以发相同动作，断开后本地操作继续工作。

清醒与睡眠采用不同精力变化规则。改变后的状态每 60 秒自动保存，也可以手动保存；断电期间不计算成长。错误存档不会被自动擦除，详细处理见实验指南。

```text
idf.py -B build/lesson17 -DCOURSE_LESSON=17 build
idf.py -B build/lesson17 -p COM实际端口 flash monitor
```

当前 ASCII 界面由手写绘图模块渲染，宠物用几何图形构成，不需要下载素材。菜单 SETTINGS 是用于学习滚动/返回的信息页，不包含尚未实现的亮度设置。

## 为什么这样拆分

输入任务和 BLE 回调只投递小事件，应用任务统一修改模型。菜单只关心 UP/DOWN/SELECT/BACK，不关心来源。模型不包含 ESP-IDF 头文件，因此可在主机上测试。

显示模块使用 esp_lcd 驱动 ST7789，把脏区域拆成小块绘制。每块等 DMA 完成后再复用内存，避免花屏；移动高亮只刷新旧行、新行和页码。先理解这些机制，再迁移到 LVGL。

代码的中文注释重点说明原因、时间单位、内存归属、回调环境和错误路径。对照代码时，先预测执行结果再运行，避免只复制烧录。

## 如何逐步脱离 AI

- 第 0～4 课：读注释版，合上后重写核心逻辑；只查 API。
- 第 5～9 课：自己画调用顺序、找官方示例，再对照实现。
- 第 10～13 课：先完成变体，AI 只做提示或审查。
- 第 14～17 课：先写需求、规则和验收，独立定位一个错误后再求助。

每课留下“预期—实际—假设—验证—结论”。毕业练习是增加 **清洁度 → 清洁菜单 → BLE 清洁命令 → 存档版本迁移 → 边界测试**，先不看参考代码。独立编程允许查手册，不要求背诵所有 API。

官方文档版本固定到 ESP32 / v5.4：[入门](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/get-started/index.html)、[LCD](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/lcd/index.html)、[NVS](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/storage/nvs_flash.html)。
