/**
 * @file radio.cpp
 * @brief NimBLE GATT 服务及宠物状态广播实现。
 *
 * 命令特征只接受一个 ASCII 字节并立即转换为事件入队，GATT 回调不执行
 * 宠物规则，避免在协议栈上下文中做较慢工作。状态特征维护一份 20 字节
 * 快照，READ 和 NOTIFY 都读取这份快照；通知是“当前状态”而非可靠应答。
 */
#include "radio.h"
#include "course.h"
#include <atomic>
#include <cstdio>
#include <cstring>
extern "C" {
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
}

namespace {
// NimBLE UUID 宏按小端字节顺序填写。
// 服务：ab120000-1234-5678-1234-56789abcdef0；命令/状态分别为 0001/0002。
const ble_uuid128_t serviceUuid=BLE_UUID128_INIT(0xf0,0xde,0xbc,0x9a,0x78,0x56,0x34,0x12,
                                               0x78,0x56,0x34,0x12,0,0,0x12,0xab);
const ble_uuid128_t commandUuid=BLE_UUID128_INIT(0xf0,0xde,0xbc,0x9a,0x78,0x56,0x34,0x12,
                                               0x78,0x56,0x34,0x12,1,0,0x12,0xab);
const ble_uuid128_t statusUuid=BLE_UUID128_INIT(0xf0,0xde,0xbc,0x9a,0x78,0x56,0x34,0x12,
                                              0x78,0x56,0x34,0x12,2,0,0x12,0xab);
ble_gatt_chr_def characteristics[3]={}; // 最后一项为全零结束标记，必须长期有效。
ble_gatt_svc_def services[2]={};
uint16_t statusHandle=0;
uint8_t addressType=0;
// 原子标志供应用任务读取，避免 GAP/NimBLE 任务更新时出现撕裂状态。
std::atomic<bool> connected{false}, subscribed{false}, synced{false};
portMUX_TYPE snapshotLock=portMUX_INITIALIZER_UNLOCKED;
char snapshot[21]="F080 H070 E090 S0 R1";

void check(int result,const char *operation) {
    // NimBLE 配置错误无法安全恢复，统一记录操作名后终止启动。
    if (result) { ESP_LOGE("ble","%s failed: %d",operation,result); abort(); }
}

int access(uint16_t,uint16_t,ble_gatt_access_ctxt *ctx,void *) {
    // 同一个访问回调按操作类型分流：写入命令，读取状态；其余操作拒绝。
    if (ctx->op==BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // mbuf 可能不连续，而且不是以 NUL 结尾的字符串，按长度复制。
        if (OS_MBUF_PKTLEN(ctx->om)!=1) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        char byte=0;
        if (os_mbuf_copydata(ctx->om,0,1,&byte)) return BLE_ATT_ERR_UNLIKELY;
        // 解析在回调内只做轻量查表；真正动作留给应用任务按顺序消费。
        const Event event=parseCommand(byte);
        if (event==Event::None) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
        return postEvent(event)?0:BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ctx->op==BLE_GATT_ACCESS_OP_READ_CHR) {
        char local[21];
        // 复制到局部缓冲区后再离开临界区，避免 strlen/mbuf 操作占用锁。
        portENTER_CRITICAL(&snapshotLock);
        memcpy(local,snapshot,sizeof(local));
        portEXIT_CRITICAL(&snapshotLock);
        return os_mbuf_append(ctx->om,local,strlen(local))==0?0:BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

int gap(ble_gap_event *event,void *);
void advertise() {
    // 服务 UUID 放入广播包，设备名称放入扫描响应，以控制传统广播长度。
    ble_hs_adv_fields fields={};
    // flags 宣告“可发现且不支持 BR/EDR”，让手机按 BLE 设备方式扫描。
    fields.flags=BLE_HS_ADV_F_DISC_GEN|BLE_HS_ADV_F_BREDR_UNSUP;
    // UUID 与名称分开放，避免超过传统广播 31 字节限制。
    fields.uuids128=const_cast<ble_uuid128_t *>(&serviceUuid);
    fields.num_uuids128=1; fields.uuids128_is_complete=1;
    check(ble_gap_adv_set_fields(&fields),"adv fields");
    ble_hs_adv_fields scan={};
    // 扫描响应与主广播分开发送，名称较长时不会挤占服务 UUID 空间。
    const char *name=ble_svc_gap_device_name();
    scan.name=reinterpret_cast<const uint8_t *>(name);
    scan.name_len=strlen(name); scan.name_is_complete=1;
    check(ble_gap_adv_rsp_set_fields(&scan),"scan response");
    ble_gap_adv_params params={};
    params.conn_mode=BLE_GAP_CONN_MODE_UND;
    params.disc_mode=BLE_GAP_DISC_MODE_GEN;
    check(ble_gap_adv_start(addressType,nullptr,BLE_HS_FOREVER,&params,gap,nullptr),"advertise");
    ESP_LOGI("ble","Advertising: PocketPet");
}

int gap(ble_gap_event *event,void *) {
    // 连接、断开、订阅和广播结束都会更新状态机；断开后自动重新广播。
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        // status=0 才代表连接建立；失败事件仍会走重新广播路径。
        connected=event->connect.status==0; subscribed=false;
        ESP_LOGI("ble","connect status=%d",event->connect.status);
        if (!connected) advertise();
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        connected=false; subscribed=false;
        ESP_LOGI("ble","disconnect reason=%d",event->disconnect.reason);
        advertise(); break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        // 只接受状态特征的订阅，命令特征不会改变通知开关。
        if (event->subscribe.attr_handle==statusHandle) subscribed=event->subscribe.cur_notify;
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE: advertise(); break;
    default: break;
    }
    return 0;
}
void onHostSync() {
    // 主机同步后确定本机地址类型，只有此时才能开始有效广播。
    check(ble_hs_util_ensure_addr(0),"ensure address");
    check(ble_hs_id_infer_auto(0,&addressType),"address type");
    synced=true; // 地址类型已确定，后续广播参数可以安全使用。
    advertise();
}
void reset(int reason) {
    // NimBLE 主机复位期间清除对外状态，避免上层误以为仍可通知。
    synced=false; connected=false; subscribed=false;
    ESP_LOGW("ble","host reset reason=%d",reason);
}
void hostTask(void *) {
    // NimBLE 事件循环必须持续运行；正常情况下直到系统关闭都不返回。
    nimble_port_run(); // 主机事件循环；正常情况下不返回。
    nimble_port_freertos_deinit();
}
}

void radioBegin() {
    // 按命令写入、状态读取/通知的顺序注册静态 GATT 表并启动主机任务。
    ESP_ERROR_CHECK(nimble_port_init());
    ble_svc_gap_init(); ble_svc_gatt_init();
    check(ble_svc_gap_device_name_set("PocketPet"),"set name");
    characteristics[0].uuid=&commandUuid.u;
    // 静态 GATT 表必须在服务运行期间保持有效，因此使用命名空间内存储。
    characteristics[0].access_cb=access;
    characteristics[0].flags=BLE_GATT_CHR_F_WRITE;
    characteristics[1].uuid=&statusUuid.u;
    characteristics[1].access_cb=access;
    characteristics[1].val_handle=&statusHandle;
    characteristics[1].flags=BLE_GATT_CHR_F_READ|BLE_GATT_CHR_F_NOTIFY;
    services[0].type=BLE_GATT_SVC_TYPE_PRIMARY;
    services[0].uuid=&serviceUuid.u;
    services[0].characteristics=characteristics;
    check(ble_gatts_count_cfg(services),"count GATT");
    check(ble_gatts_add_svcs(services),"add GATT");
    ble_hs_cfg.sync_cb=onHostSync; ble_hs_cfg.reset_cb=reset;
    nimble_port_freertos_init(hostTask);
}
bool radioConnected() { return connected.load(); }
void radioPublish(const Pet &pet,bool accepted) {
    // 在临界区替换完整快照，保证读取回调不会看到半条字符串。
    char local[21];
    // 最大 20 字节，即使默认 ATT MTU=23 也能装下一条通知。
    // F/H/E 为 0..100 三位字段，S 表示睡眠，R 表示最近一次动作结果。
    snprintf(local,sizeof(local),"F%03u H%03u E%03u S%d R%d",pet.food,pet.happy,
             pet.energy,pet.sleeping?1:0,accepted?1:0);
    portENTER_CRITICAL(&snapshotLock);
    memcpy(snapshot,local,sizeof(snapshot));
    portEXIT_CRITICAL(&snapshotLock);
    // chr_updated 让 NimBLE 主机执行通知；读取回调也使用同一份加锁快照。
    // 高频状态允许合并：这不是逐命令的可靠应答协议。
    if (synced && connected && subscribed) ble_gatts_chr_updated(statusHandle);
}
