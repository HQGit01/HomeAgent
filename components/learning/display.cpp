#include "display.h"
/**
 * @file display.cpp
 * @brief SPI ST7789 驱动、DMA 扫描带画布及 PocketPet 界面实现。
 *
 * 显示模块只分配一条 16 行扫描带的 DMA 缓冲区，绘制请求先裁剪到
 * 240x240 屏幕，再逐带等待传输完成，因此不会在 DMA 仍使用缓冲区时
 * 覆盖像素。PetUi 通过保存上一帧的宠物、菜单、连接和页脚状态，
 * 将刷新范围缩小到实际变化的区域。
 */
#include "course.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_heap_caps.h"
#include "freertos/semphr.h"
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace {
// 每次只准备一条扫描带，降低 DMA 缓冲区的内存占用。
constexpr int stripHeight=16;
// LCD 面板、传输信号量和 DMA 缓冲区都是显示模块的共享资源。
esp_lcd_panel_handle_t panel=nullptr;
SemaphoreHandle_t complete=nullptr;
uint16_t *buffer=nullptr;
// 累计提交给 LCD 的像素字节数，用于观察刷新开销。
uint64_t bytesSent=0;

// DMA 传输完成回调：通知等待中的绘制任务可以复用缓冲区。
bool transferDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *) {
    BaseType_t wake=pdFALSE;
    xSemaphoreGiveFromISR(complete, &wake);
    return wake==pdTRUE; // esp_lcd 根据返回值在退出 ISR 时安排任务切换。
}
}

void displayBegin() {
    // 当前课程界面固定按 240x240 设计，其他分辨率需要同步调整布局。
    static_assert(board::width==240 && board::height==240,
                  "Course UI is 240x240; adapt layout for other resolutions");
    configASSERT(panel==nullptr);
    // 二值信号量用于保证 DMA 尚未完成时不会覆盖共享缓冲区。
    complete=xSemaphoreCreateBinary();
    // 信号量初始为空；只有本次 SPI/DMA 传输完成回调才会释放它。
    // LCD DMA 需要使用内部 RAM 中可 DMA 访问的内存。
    buffer=static_cast<uint16_t *>(heap_caps_malloc(board::width*stripHeight*2,
                                                    MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL));
    // 一个像素占 2 字节，分配的容量正好覆盖“宽 × 一条扫描带”。
    configASSERT(complete && buffer);
    // 配置 SPI 总线；LCD 只写数据，因此不使用 MISO、WP 和 HD。
    spi_bus_config_t bus={};
    bus.sclk_io_num=board::lcdSck; bus.mosi_io_num=board::lcdMosi;
    bus.miso_io_num=-1; bus.quadwp_io_num=-1; bus.quadhd_io_num=-1;
    bus.max_transfer_sz=board::width*stripHeight*2;
    // 限制单次 SPI 事务大小，与上面的扫描带缓冲区保持一致。
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));
    // 配置 SPI LCD IO 层，并注册传输完成回调。
    esp_lcd_panel_io_spi_config_t ioConfig={};
    ioConfig.cs_gpio_num=board::lcdCs; ioConfig.dc_gpio_num=board::lcdDc;
    ioConfig.spi_mode=board::spiMode; ioConfig.pclk_hz=board::spiHz;
    ioConfig.trans_queue_depth=1; // 教学先用一块缓冲，等待完成后复用。
    ioConfig.lcd_cmd_bits=8; ioConfig.lcd_param_bits=8;
    ioConfig.on_color_trans_done=transferDone;
    esp_lcd_panel_io_handle_t io=nullptr;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(esp_lcd_spi_bus_handle_t(SPI2_HOST), &ioConfig, &io));
    // 创建 ST7789 面板对象，并设置 RGB565 数据格式。
    esp_lcd_panel_dev_config_t dev={};
    dev.reset_gpio_num=board::lcdReset;
    dev.rgb_ele_order=(board::madctl & 8) ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB;
    dev.bits_per_pixel=16;
    dev.data_endian=LCD_RGB_DATA_ENDIAN_BIG;
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &dev, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    // madctl 的方向位控制坐标交换、水平镜像和垂直镜像。
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel, (board::madctl & 0x20)!=0));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel, (board::madctl & 0x40)!=0,
                                      (board::madctl & 0x80)!=0));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, board::xOffset, board::yOffset));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, board::invert));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    if (board::lcdBacklight>=0) {
        // -1 表示开发板没有可由软件控制的背光 GPIO。
        gpio_config_t cfg={};
        // mask 的移位操作仅使用非负数，默认 -1 表示没有 GPIO 使能。
        cfg.pin_bit_mask=1ULL<<(board::lcdBacklight>=0?board::lcdBacklight:0);
        cfg.mode=GPIO_MODE_OUTPUT;
        ESP_ERROR_CHECK(gpio_config(&cfg));
        // 配置值描述“亮”的逻辑状态，驱动在这里转换为实际 GPIO 电平。
        ESP_ERROR_CHECK(gpio_set_level(gpio_num_t(board::lcdBacklight), board::backlightActiveHigh));
    }
}

void displayPaint(Rect r, Painter paint, void *context) {
    // API 面向课程内部的小坐标；先裁剪，拒绝空区域。
    const int x0=std::max(0,r.x), y0=std::max(0,r.y);
    const int x1=std::min(board::width,r.x+r.w), y1=std::min(board::height,r.y+r.h);
    if (x0>=x1 || y0>=y1 || !paint) return;
    // r 的右/下边界采用半开区间，避免宽高为负或越界时计算出非法 DMA 区域。
    const int64_t started=esp_timer_get_time();
    // 用微秒记录本次区域提交耗时；它只用于诊断，不参与刷新决策。
    uint32_t bytes=0;
    // 按扫描带分块绘制，避免为整块 240x240 画面申请 DMA 缓冲区。
    for (int y=y0;y<y1;y+=stripHeight) {
        Rect strip{x0,y,x1-x0,std::min(stripHeight,y1-y)};
        Canvas canvas(buffer,strip);
        // 同一画布用绝对坐标绘制，Canvas 内部会把坐标映射到本带的偏移。
        canvas.fill(color::background); // 每块先恢复背景，再按层绘制前景。
        paint(canvas,context);
        const int pixels=strip.w*strip.h;
        for (int i=0;i<pixels;++i) buffer[i]=uint16_t((buffer[i]<<8)|(buffer[i]>>8));
        // CPU 颜色值为低字节在前，ST7789 接收高字节在前，因此逐像素交换。
        // draw_bitmap 的右、下端点不包含；缓冲必须是紧密排列的矩形。
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel,x0,y,x1,y+strip.h,buffer));
        if (xSemaphoreTake(complete,pdMS_TO_TICKS(2000))!=pdTRUE) {
            // 不能超时后继续覆盖 DMA 仍可能使用的内存。
            ESP_LOGE("lcd","DMA completion timeout");
            abort();
        }
        bytes+=pixels*2;
    }
    bytesSent+=bytes;
    ESP_LOGD("lcd","rect=(%d,%d,%d,%d) bytes=%lu us=%lld",x0,y0,x1-x0,y1-y0,
             (unsigned long)bytes,(long long)(esp_timer_get_time()-started));
}

// 返回自启动以来提交给 LCD 的像素数据量，不包含命令和协议开销。
uint64_t displayBytes() { return bytesSent; }

// 用一个颜色填充当前 Canvas 对应的紧密像素缓冲区。
void Canvas::fill(uint16_t c) { std::fill(pixels_,pixels_+area_.w*area_.h,c); }

// 设置单个像素；超出当前扫描带的坐标会被忽略。
void Canvas::pixel(int x,int y,uint16_t c) {
    // 只有落在本扫描带内的像素才计算线性下标，避免写出 DMA 缓冲区。
    if (x>=area_.x && x<area_.x+area_.w && y>=area_.y && y<area_.y+area_.h)
        pixels_[(y-area_.y)*area_.w+x-area_.x]=c;
}

// 绘制半开区间矩形，并将其裁剪到当前 Canvas 区域。
void Canvas::rect(Rect r,uint16_t c) {
    const int x0=std::max(r.x,area_.x), x1=std::min(r.x+r.w,area_.x+area_.w);
    const int y0=std::max(r.y,area_.y), y1=std::min(r.y+r.h,area_.y+area_.h);
    if (x0>=x1 || y0>=y1) return;
    // 每一行都是连续区间，按行填充可跳过矩形外的像素。
    for (int y=y0;y<y1;++y)
        std::fill(pixels_+(y-area_.y)*area_.w+x0-area_.x,
                  pixels_+(y-area_.y)*area_.w+x1-area_.x,c);
}

void Canvas::text(int x,int y,const char *s,uint16_t c,int scale) {
    // 自制 3x5 点阵：每行低三位代表三个像素。先学 ASCII，中文另加字模。
    static const uint8_t glyph[36][5]={
        {2,5,7,5,5},{6,5,6,5,6},{3,4,4,4,3},{6,5,5,5,6},
        {7,4,6,4,7},{7,4,6,4,4},{3,4,5,5,3},{5,5,7,5,5},
        {7,2,2,2,7},{1,1,1,5,2},{5,5,6,5,5},{4,4,4,4,7},
        {5,7,7,5,5},{5,7,7,7,5},{2,5,5,5,2},{6,5,6,4,4},
        {2,5,5,3,1},{6,5,6,5,5},{3,4,2,1,6},{7,2,2,2,2},
        {5,5,5,5,7},{5,5,5,5,2},{5,5,7,7,5},{5,5,2,5,5},
        {5,5,2,2,2},{7,1,2,4,7},
        {7,5,5,5,7},{2,6,2,2,7},{6,1,2,4,7},{6,1,2,1,6},
        {5,5,7,1,1},{7,4,6,1,6},{3,4,6,5,2},{7,1,2,2,2},
        {2,5,2,5,2},{2,5,3,1,6}
    };
    // 空字符串指针或非正缩放比例不产生任何像素，也不会触碰缓冲区。
    if (!s || scale<=0) return;
    // 每个字符使用一个缩放后的像素列作为字符间距。
    // 指针逐字符前进；x 的步长包含 3 列字模和 1 列字符间距。
    for (;*s;++s,x+=4*scale) {
        char ch=*s;
        if (ch>='a' && ch<='z') ch-=32;
        // 字母占 0..25、数字占 26..35，其余符号通过下面的特殊规则处理。
        int index=ch>='A'&&ch<='Z' ? ch-'A' : ch>='0'&&ch<='9' ? ch-'0'+26 : -1;
        // 逐点读取 3×5 字模；位值从高到低对应左到右三列。
        for (int row=0;row<5;++row) for (int col=0;col<3;++col) {
            bool set=index>=0 && (glyph[index][row] & (4>>col));
            if (ch=='-') set=row==2;
            if (ch==':') set=col==1 && (row==1 || row==3);
            if (set) rect({x+col*scale,y+row*scale,scale,scale},c);
        }
    }
}

void paintPet(Canvas &c,int x,int y,int frame,bool asleep) {
    // 用矩形组合像素宠物，不依赖外部图片；只在清醒时上下移动。
    const int bob=asleep?0:(frame%2)*4;
    // 只在清醒状态根据帧号产生 0/4 像素位移，睡眠时保持静止。
    y+=bob;
    // 宠物的身体、耳朵和脚都用矩形组成，便于演示基础图形绘制。
    c.rect({x+12,y+12,56,48},color::yellow);
    c.rect({x+12,y,16,20},color::yellow); c.rect({x+52,y,16,20},color::yellow);
    c.rect({x+20,y+60,12,8},color::yellow); c.rect({x+48,y+60,12,8},color::yellow);
    const int eyeH=asleep?2:8;
    c.rect({x+24,y+28,6,eyeH},color::black); c.rect({x+50,y+28,6,eyeH},color::black);
    c.rect({x+36,y+44,8,4},color::red);
    if (asleep) c.text(x+68,y,"Z",color::white,2);
}

void PetUi::begin(bool partial,bool inMenu,bool submenus) {
    // 初始化刷新模式和起始页面；首次 draw() 会强制完整刷新。
    partial_=partial; submenus_=submenus; page_=inMenu?Page::Menu:Page::Home;
    root_.count=submenus?7:5;
    // 隐藏子菜单时同步缩短根菜单，防止绘制逻辑访问不存在的条目。
    dirty_=true;
}
void PetUi::message(const char *text) {
    // 复制消息内容，避免调用者传入的临时字符串失效。
    // 采用固定数组和 snprintf，确保外部文本不会越界覆盖界面状态。
    snprintf(message_,sizeof(message_),"%s",text?text:"");
    footerDirty_=true;
}

Event PetUi::input(Event e) {
    // Back 按页面层级返回；Home 页面没有更上一层。
    if (e==Event::Back) {
        if (page_==Page::Info) page_=Page::Settings;
        else if (page_==Page::Settings) page_=Page::Menu;
        else page_=Page::Home;
        dirty_=true; return Event::None;
    }
    if (page_==Page::Home) {
        // Home 页面只响应进入菜单，方向键在这里没有意义。
        if (e==Event::Select) { page_=Page::Menu; dirty_=true; return Event::None; }
        return (e==Event::Up || e==Event::Down)?Event::None:e;
    }
    if (page_==Page::Info) {
        // 信息页的 Select 用于返回设置菜单。
        if (e==Event::Select) { page_=Page::Settings; dirty_=true; }
        return Event::None;
    }
    Menu &menu=page_==Page::Settings?settings_:root_;
    // 菜单移动只改变选择状态，不直接触发业务动作。
    if (e==Event::Up || e==Event::Down) { menu.move(e==Event::Up?-1:1); return Event::None; }
    if (e!=Event::Select) return e;
    if (page_==Page::Settings) {
        // 设置菜单最后一项为返回，其余项目进入信息页。
        page_=menu.selected==7?Page::Menu:Page::Info; dirty_=true; return Event::None;
    }
    switch (menu.selected) {
    case 0: page_=Page::Home; dirty_=true; return Event::None;
    case 1: return Event::Feed; case 2: return Event::Play;
    case 3: return Event::Sleep; case 4: return Event::Wake;
    case 5: page_=Page::Settings; dirty_=true; return Event::None;
    case 6: return Event::Save; default: return Event::None;
    }
}

// Painter 需要普通函数指针，因此通过 context 找回 PetUi 对象。
void PetUi::painter(Canvas &c,void *ctx) { static_cast<PetUi *>(ctx)->scene(c); }

// 绘制当前页面；Canvas 会自动裁剪到 displayPaint() 请求的区域。
void PetUi::scene(Canvas &c) {
    static const char *rootLabels[]={"PET","FEED","PLAY","SLEEP","WAKE","SETTINGS","SAVE"};
    static const char *settingsLabels[]={"ABOUT","CONTROLS","STORAGE","DISPLAY","BLE","VERSION","HELP","BACK"};
    c.rect({0,0,240,28},color::panel);
    c.text(10,8,page_==Page::Home?"POCKET PET":page_==Page::Menu?"MENU":"SETTINGS",color::white,2);
    c.text(204,8,connected_?"BLE":"OFF",connected_?color::accent:color::muted,2);
    if (page_==Page::Home) {
        // 主页显示宠物动画和三项状态条。
        paintPet(c,80,48,frame_,current_.sleeping);
        char line[24];
        const int values[]={current_.food,current_.happy,current_.energy};
        // 将三个 uint8_t 属性复制为 int，便于统一进行文本格式化和比例计算。
        const char *labels[]={"FOOD","HAPPY","ENERGY"};
        for (int i=0;i<3;++i) {
            const int y=140+i*23;
            snprintf(line,sizeof(line),"%s %d",labels[i],values[i]);
            c.text(10,y,line,color::white,2);
            c.rect({114,y,112,12},color::panel);
            // 0..100 属性线性映射到 0..110 像素，避免填充越过外框。
            c.rect({115,y+1,values[i]*110/100,10},color::accent);
        }
    } else if (page_==Page::Info) {
        // 信息页根据设置菜单的选择显示对应说明。
        c.text(12,46,settingsLabels[settings_.selected],color::accent,3);
        const char *info[]={"ESP WROOM 32","TURN AND PRESS","SAVE EACH 60S","ST7789 240X240",
                            "GATT COMMANDS","ESP IDF 5 4","LONG PRESS BACK"};
        c.text(12,90,info[settings_.selected],color::white,2);
        c.text(12,142,"PRESS TO RETURN",color::muted,2);
    } else {
        // 菜单页只绘制当前可见窗口内的菜单项。
        const Menu &m=page_==Page::Settings?settings_:root_;
        const char *const *labels=page_==Page::Settings?settingsLabels:rootLabels;
        for (int row=0;row<m.visible && m.first+row<m.count;++row) {
            const int index=m.first+row,y=40+row*34;
            c.rect({8,y,224,30},index==m.selected?color::accent:color::panel);
            c.text(18,y+7,labels[index],index==m.selected?color::black:color::white,3);
        }
        char index[20]; snprintf(index,sizeof(index),"%d OF %d",m.selected+1,m.count);
        c.text(12,186,index,color::muted,2);
    }
    c.rect({0,216,240,24},color::panel);
    c.text(8,224,message_,color::white,2);
}

void PetUi::draw(const Pet &pet,int frame,bool connected) {
    // 保存本次状态，并与上一帧比较以确定需要刷新的区域。
    current_=pet; frame_=frame; connected_=connected;
    const Menu &m=page_==Page::Settings?settings_:root_;
    const bool stats=pet.food!=previous_.food || pet.happy!=previous_.happy || pet.energy!=previous_.energy;
    // 将数值、动画、菜单和连接分别比较，随后只提交与变化来源对应的矩形。
    const bool petChanged=frame!=oldFrame_ || pet.sleeping!=previous_.sleeping;
    const bool menuChanged=m.selected!=oldMenu_.selected || m.first!=oldMenu_.first;
    const bool changed=dirty_ || page_!=oldPage_ || stats || petChanged || menuChanged ||
                       connected!=oldConnected_ || footerDirty_;
    if (!changed) return;
    // 首次绘制、页面切换或关闭局部刷新时，重绘整个屏幕。
    if (!partial_ || dirty_ || page_!=oldPage_) displayPaint({0,0,240,240},painter,this);
    else {
        // 局部刷新只提交发生变化的区域，减少 SPI 传输量。
        if (connected!=oldConnected_) displayPaint({196,0,44,28},painter,this);
        if (page_==Page::Home) {
            if (petChanged) displayPaint({72,40,104,88},painter,this);
            if (stats) displayPaint({0,136,240,72},painter,this);
        } else if ((page_==Page::Menu || page_==Page::Settings) && menuChanged) {
            if (m.first!=oldMenu_.first) displayPaint({0,36,240,140},painter,this);
            else {
                displayPaint({8,40+(oldMenu_.selected-m.first)*34,224,30},painter,this);
                displayPaint({8,40+(m.selected-m.first)*34,224,30},painter,this);
            }
            displayPaint({0,180,240,24},painter,this); // 页码也是变化区域。
        }
        if (footerDirty_) displayPaint({0,216,240,24},painter,this);
    }
    // 绘制完成后更新快照，供下一次 draw() 判断差异。
    // 快照必须在所有区域提交完成后更新，否则下一轮会重复刷新同一区域。
    oldPage_=page_; oldMenu_=m; previous_=pet; oldFrame_=frame; oldConnected_=connected;
    dirty_=footerDirty_=false;
}
