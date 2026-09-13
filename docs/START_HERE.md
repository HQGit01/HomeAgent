# 原生 ESP-IDF 实验指南

已确认：ESP-WROOM-32、1.54 英寸 ST7789 SPI TFT、旋转编码器。工程现在完全采用原生 ESP-IDF，不需要 PlatformIO 或 Arduino。使用本机已有 ESP-IDF 5.4（安装信息为 5.4.0）；使用 C++ 编写模块、直接调用 ESP-IDF 的 C API。C++ 并不等于 Arduino。

尚待硬件核对：屏幕是否 240×240、引脚顺序、背光供电方式、是否引出 CS、模块偏移、LED 的实际 GPIO、旋钮每卡点对应 2 还是 4 次相位变化。不要按杜邦线颜色猜接线。

## 1. 第一次运行

在 ESP-IDF 扩展中打开此目录，执行 “ESP-IDF: Open ESP-IDF Terminal”；也可使用安装器提供的 ESP-IDF 命令行。终端必须能执行 `idf.py --version`。

```text
idf.py --version
idf.py -B build/lesson0 -DCOURSE_LESSON=0 build
idf.py -B build/lesson0 -p COM实际端口 flash monitor
```

把 `COM实际端口` 替换为设备管理器中的实际值，例如 COM5。退出监视器按 Ctrl+]。首次运行只连接已经验证过的 LED，不必接屏幕或旋钮。

`build` 只是编译；`flash` 才会将固件写到板子。本次编写阶段不会替你烧录或擦除设备。分区表假设 4 MB Flash，并且没有 OTA 分区。

屏幕色条课：

```text
idf.py -B build/lesson8 -DCOURSE_LESSON=8 build
idf.py -B build/lesson8 -p COM实际端口 flash monitor
```

最终应用：

```text
idf.py -B build/lesson17 -DCOURSE_LESSON=17 build
idf.py -B build/lesson17 -p COM实际端口 flash monitor
```

每课使用独立构建目录，防止把另一课的固件误烧录。参数使用数字 0～17 或 99，不写 08 等前导零。各构建目录共用根目录生成的 sdkconfig；课程之间不要随意修改 SDK 特性配置。默认目标 esp32，不能改成 esp32s3。

Windows PowerShell 若禁止执行 export.ps1，使用安装器的 ESP-IDF CMD 或 VS Code 的 ESP-IDF Terminal，不必修改系统执行策略。源码和文档使用 UTF-8；sdkconfig.defaults 与 partitions.csv 使用 ASCII 注释以兼容 Windows 的配置解析器。

## 2. 文件阅读顺序

```text
CMakeLists.txt                  原生工程与课程编号
sdkconfig.defaults              NimBLE、1 ms tick、4 MB Flash 等默认配置
partitions.csv                  NVS / PHY / 应用的 Flash 地址布局
main/main.cpp                   app_main 入口，选择本课函数
lessons/00_hello.cpp ...         每课独立入口：先从这里读
include/board_config.h          所有接线与屏幕参数
components/learning/include/    公共模块接口
components/learning/model.cpp   不依赖硬件的按钮/旋钮/菜单/宠物规则
components/learning/inputs.cpp  GPIO 采样、任务与事件队列
components/learning/display.cpp esp_lcd、DMA 等待、画布、菜单和局部刷新
components/learning/radio.cpp   NimBLE GATT、连接恢复和状态快照
components/learning/storage.cpp NVS 存档
tests/                         模型边界测试
```

原生 ESP-IDF 用 CMake 注册组件，不使用 Arduino 的 lib 目录自动发现规则。不要直接双击某个 cpp 文件编译；它依赖工程配置、头文件路径和 ESP-IDF 工具链。

`app_main` 在主任务中运行。C++ 文件使用 `extern "C"` 保留正确入口符号。基础示例允许 `app_main` 返回；循环应用通过 `vTaskDelay` 让出 CPU。

## 3. 接线草案

以下为当前代码默认值，只适用于已经确认的经典 ESP-WROOM-32。核对开发板引出脚和屏幕厂商标识后再接线；GPIO 编号不等于排针序号。

| 设备信号 | ESP32 默认 GPIO | 说明 |
| --- | --- | --- |
| LED | 2 | 改为你已经点亮成功的引脚与有效电平 |
| 屏幕 SCL/SCK/CLK | 18 | SPI 时钟 |
| 屏幕 SDA/DIN/MOSI | 23 | SPI 数据，名称 SDA 不代表 I2C |
| 屏幕 CS | 21 | 未引出 CS 时将 lcdCs 设为 -1，该屏幕独占总线 |
| 屏幕 DC/A0 | 22 | 区分命令与像素/参数数据 |
| 屏幕 RES/RST | 19 | 复位 |
| 编码器 A/CLK | 32 | 模块输出不能超过 3.3 V |
| 编码器 B/DT | 33 | 方向反了先修改 encoderReverse |
| 编码器 SW | 27 | 按下接地，代码启用内部上拉 |
| GND | GND | ESP32、屏幕、旋钮共地 |

供电按模块额定值确认。ESP32 GPIO 使用 3.3 V 逻辑；不要将 5 V 旋钮模块输出直接送入 GPIO。屏幕裸背光不直接由 GPIO 供电，默认 `lcdBacklight=-1`；只有确认模块提供可由 GPIO 驱动的使能接口后，才填写该 GPIO。

ESP-WROOM-32 的 GPIO6～11 用于模块 Flash。GPIO34～39 为输入专用且没有内部上拉/下拉，不能照搬当前输入配置。GPIO0/2/5/12/15 涉及启动配置；LED 默认 GPIO2 仅为了方便迁移你已有示例，接线需以你的板子为准。

屏幕驱动只按 240×240 布局实现，RGB565，一块 240×16×2=7680 字节的 DMA 缓冲。先以 10 MHz 通过色条课，再尝试 20/40 MHz。`madctl` 控制方向和 RGB/BGR，`xOffset/yOffset` 控制模块可见窗口，部分方向需要 yOffset=80，不能仅凭英寸数确定。

## 4. 每课用例、验收与独立练习

每课先阅读对应 cpp，再根据调用进入公共模块。完整参考代码已给出，但独立练习应先自行完成，不要先让 AI 改答案。

| 课号 / 文件 | 本课运行现象 | 必须验收 | 独立练习 |
| --- | --- | --- | --- |
| 0 / 00_hello | 点灯、打印 GPIO | 复位重复出现启动日志 | 解释初始化与电平含义 |
| 1 / 01_timing | 250 ms 切灯，1 s 日志 | 改灯周期不影响日志周期 | 实现短闪两次再停顿 |
| 2 / 02_button | 旋钮按键短按切灯，长按打印 BACK | 30 次短按只切换 30 次，长按释放无 SELECT | 调整消抖与长按阈值 |
| 3 / 03_encoder | 旋转打印方向/位置 | 慢转十格与十次计数一致；反向正确 | 修改每格相位数并记录原始 AB |
| 4 / 04_queue | 输入事件与心跳同时工作 | 快速输入不影响心跳；观察丢弃计数 | 主动制造队列满并解释策略 |
| 5 / 05_ble_connect | 手机发现 PocketPet，连接时 LED 亮 | 断开/重连 10 次 | 区分广播、连接与配对 |
| 6 / 06_ble_led | BLE 写 ASCII 1/0 开关灯，Read/Notify 示例状态 | 空数据、两字节、未知字节均拒绝 | 增加灯状态到协议 |
| 7 / 07_menu_serial | 旋钮/手机控制串口输出的菜单 | 两种输入首尾循环一致 | 加一个菜单项 |
| 8 / 08_screen | RGB 三色条与 TL/TR/BL/BR | 四角完整，颜色正确 | 修正翻转/偏移，解释原因 |
| 9 / 09_drawing | 静态宠物、文字、状态条 | 没有越界、位图位置正确 | 画自己的宠物表情 |
| 10 / 10_menu | 高亮选择、确认、返回，全屏重绘 | 选择与高亮一致 | 改成首尾停止 |
| 11 / 11_submenu | 滚动列表和 SETTINGS 子页 | 返回恢复父级选择和滚动位置 | 增加帮助页 |
| 12 / 12_partial | 同一菜单改用局部刷新，输出字节/耗时 | 无旧高亮/文字；实测字节减少 | 解释滚动时为何重绘列表 |
| 13 / 13_animation | 250 ms 切换宠物动画，按键仍能操作 | 无残影，输入不随动画阻塞 | 增加第三帧或眨眼 |
| 14 / 14_pet | 串口宠物规则，旋转喂食/玩耍，短按睡眠/长按唤醒 | 属性 0～100，睡眠拒绝玩耍 | 增加清洁度 |
| 15 / 15_save | 旋转改属性，短按保存，重启读取 | 保存后恢复相同值 | 改版存档迁移 |
| 16 / 16_ble_pet | 手机 F/P/S/W 控制宠物，订阅属性 | 睡眠时 P 被业务拒绝，R=0 | 区分传输成功与动作接受 |
| 17 / 17_pocket_pet | 完整菜单、动画、BLE、存档 | 见后面的完整应用验收 | 独立扩展一条端到端功能 |

第 5 课复用已注册的 GATT 服务，但只学习连接；收到的命令被消费但不执行业务。第 10～13 课是界面实验，操作结果显示在底部，不修改宠物属性；第 17 课才连接完整规则。第 15 课为方便验证存档，暂停自动时间衰减。

## 5. 菜单与局部刷新

旋转 = 上下，短按 = 确认，长按 700 ms = 返回。HOME 短按进入菜单，SETTINGS 目前是八项信息页，用来练习滚动与返回；它不是亮度等功能的占位开关。

菜单有两个不同的状态：selected 为“选中第几项”，first 为“屏幕第一行显示哪一项”。显示区域只能容纳四项时，移动超出可见区域才改变 first。

本实现一次不滚动的高亮移动会更新旧行、新行和底部页码：

```text
全屏：240 × 240 × 2 = 115200 字节
两行：224 × 30 × 2 × 2 = 26880 字节
页码：240 × 24 × 2 = 11520 字节
合计：38400 字节，为全屏的 1/3
```

实测还会因换页、滚动、提示文字等发生变化。`displayBytes` 只统计像素数据，不包括 SPI 命令。第 12 课打印本次像素字节和完整绘图/发送耗时，不能将它称为纯 DMA 时间。

每个脏矩形拆成最多 16 行的小块，重画背景和场景、将 RGB565 转成高字节在前、提交 `esp_lcd_panel_draw_bitmap`，再等完成信号。驱动的右下端点不包含。传输未完成之前绝不覆盖缓冲；超时会报告错误并中止，避免继续制造花屏。

目前使用单缓冲 + DMA 完成等待。双缓冲、TE 同步和 LVGL 迁移是进阶练习，尚未提供对应实现；不把单缓冲等待称为双缓冲流水线。

## 6. 手机 BLE 操作

使用能读写 GATT 的 BLE 调试客户端，扫描并连接 PocketPet。通用系统蓝牙设置不一定显示此类设备；不需要先配对。教学固件没有开启配对鉴权，供近距离实验。

| 对象 | UUID | 属性 |
| --- | --- | --- |
| 服务 | ab120000-1234-5678-1234-56789abcdef0 | 自定义服务 |
| 命令 | ab120001-1234-5678-1234-56789abcdef0 | Write with response |
| 状态 | ab120002-1234-5678-1234-56789abcdef0 | Read / Notify |

每次只写一个 ASCII 字节，不带换行：U 上、D 下、E 确认、B 返回、F 喂食、P 玩耍、S 睡眠、W 唤醒、1 开灯、0 关灯、V 保存。若客户端使用 HEX 模式，例如 F 要填 46，E 要填 45，不能直接把十六进制 F 当作字符 F。

状态示例：`F080 H070 E090 S0 R1`。F 为饱食度、H 为快乐、E 为精力、S 为是否睡眠、R 为最近处理的动作是否接受。20 字节不需要自行分包。

先 Read，再启用 Notify 订阅，才能观察主动推送。最终应用最多每 250 ms 更新一次状态。Write 成功只说明合法命令进入队列；动作可能因睡眠等业务规则被拒绝。快速命令的状态会合并，R 不是逐条命令 ACK；如需可靠逐条应答，独立练习增加序号和响应队列。

## 7. 存档与完整应用验收

宠物初值：饱食 80、快乐 70、精力 90。每 10 秒饱食/快乐各减 1；清醒精力减 1，睡眠精力加 3。喂食 +15 饱食；玩耍 -10 精力、+10 快乐、-3 饱食。属性限制为 0～100；睡眠不自动结束，可通过 WAKE 唤醒。

每次循环先按真实经过的时间更新模型，再处理事件。动画周期与属性周期独立。首版只计算通电时间，断电期间不会衰减；不足 10 秒的余量不存档。

最终课变更后每 60 秒自动保存，也可菜单 SAVE 或 BLE V 主动保存。未保存变化断电会丢失。NVS API 错误不会触发自动擦除；未知/损坏存档以默认宠物显示，并关闭自动覆盖，只有用户明确 SAVE 才替换。存档格式包含魔数、版本、数值边界检查和简单 XOR；XOR 不是强校验。

完成以下实测记录后才算硬件验收通过：

- 30 次短按准确，长按释放不误选；旋钮双向慢转十格各记十次。
- 每个菜单项均可进入；滚动、首尾循环、父级选择恢复正确。
- 数字 100→9、动画移动、高亮变化无残影；局部刷新字节数符合场景。
- BLE 连断 10 次可恢复；手机断开不影响本地菜单；错误命令不改变宠物。
- SAVE 后复位属性一致；无存档和错误存档按说明处理。
- 连续运行两小时，记录 heap/min_heap/queue_dropped；无持续内存下降、死机或看门狗复位。
- 目标普通输入响应小于 100 ms，必须在你的屏幕/旋钮上测量，构建成功无法证明这一点。

模型自检无需外接屏幕：

```text
idf.py -B build/selftest -DCOURSE_LESSON=99 build
idf.py -B build/selftest -p COM实际端口 flash monitor
```

成功日志为 `All model boundary tests passed.`。主机有 C++ 编译器时，也可编译 tests/host_main.cpp 和 components/learning/model.cpp，运行完全相同的测试用例。

## 8. 排错顺序

编译失败：先看第一条 error，确认 IDF 版本、目标和组件头文件路径。链接失败：检查函数定义是否编入组件。串口无输出：确认 COM、115200 和是否烧录本课。重复启动：查看第一条错误与复位原因，勿只看最后一屏乱码。

屏幕不亮：先核对供电/背光；背光亮但无图：再查共地、DC/CS/RESET、SPI 接线和初始化；颜色错：查 RGB/BGR、反显和字节序；缺边：查分辨率、方向与偏移。SPI 写成功不代表面板存在或接线正确，因为这类屏幕通常没有可验证的应答。

旋钮跳步：先降低旋转速度并打印 AB，再校准 transitions、方向和消抖。手动旋钮课程采用 1 ms 轮询；高速编码器应另学 ESP-IDF PCNT，不靠不断提高任务优先级解决。

## 9. 自主编程练习

每课记录预测、运行日志、关键 API、一个错误和一个独立变体。前四课读注释后合上重写；中间阶段先实现再对照；最终不看答案增加“清洁度→菜单清洁→BLE 命令→存档版本迁移→验证”。允许查 ESP-IDF 文档，目标是自己拆分问题和定位错误。

官方资料应切换到 ESP32 / v5.4，与本机一致：

- [ESP-IDF v5.4 入门](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/get-started/index.html)
- [GPIO](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/gpio.html)
- [LCD](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/peripherals/lcd/index.html)
- [NVS](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/storage/nvs_flash.html)

也可直接阅读本机 ESP-IDF 的 examples/bluetooth/nimble/bleprph 和 examples/peripherals/lcd。课程使用的 API 已对照本机头文件；硬件结果以实际验收为准。
