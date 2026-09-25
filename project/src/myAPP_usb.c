#include "myAPP_usb.h"
#include "ltx.h"
#include "ltx_app.h"
#include "ltx_log.h"
#include "myAPP_display.h"
#include "GC9A01.h"

#define PIC_PACK_LENGTH         61      // 单个包有效字节数
#define PIC_PACK_NUMS           59*32   // 一张图片传输次数

#define SLEEP_TICKS_COUNT       60*1000 // 60 秒 无上位机消息则休眠

void task_func_usb_get(void *param);
void alarm_cb_sleep(void *param);

void _usb_get_up(void);

struct ltx_Task_stu task_usb_get;
struct ltx_Alarm_stu alarm_sleep = {
    .flag = 0,
    .tick_count_down = SLEEP_TICKS_COUNT,
    .callback_alarm = alarm_cb_sleep,

    .next = NULL,
};

uint8_t flag_is_sleeping = 0;
extern struct gc9a01_stu myLCD;

extern uint16_t picture_buffer[240*240];
uint32_t pic_buf_pack_counter[59]; // 240*240*2/61/32

// 重置丢包统计
void pack_counter_reset(void){
    for(uint8_t i = 0; i < 59; i ++){
        pic_buf_pack_counter[i] = 0;
    }
}

// 收包计入
void pack_count_in(uint16_t pack_index){
    pic_buf_pack_counter[pack_index/32] |= (1U << pack_index%32);
}

// 查找第一个丢的包
uint16_t pack_get_first_lost_index(void){
    for(uint8_t i = 0; i < 59; i ++){
        if(pic_buf_pack_counter[i] != 0xFFFFFFFF){
            for(uint8_t j = 0; j < 32; j ++){
                if(!(pic_buf_pack_counter[i] & (1U << j))){
                    return i*32 + j;
                }
            }
        }
    }

    return PIC_PACK_NUMS;
}

// 获取丢包数
uint16_t pack_get_lost_counter(void){
    uint16_t lost_counter = 0;
    // 修复：循环变量 i 原来未初始化（未定义行为，导致首次传输误报丢包）
    for(uint8_t i = 0; i < 59; i ++){
        // 修复：直接统计缺失的位。原实现统计的是不完整字中"已收到"的位，
        // 整字 32 包全丢时该字为 0、贡献 0，会漏判为无丢包。
        uint32_t missing = ~pic_buf_pack_counter[i];
        while(missing){
            lost_counter += missing & 1U;
            missing >>= 1;
        }
    }

    return lost_counter;
}

int myAPP_usb_init(struct ltx_App_stu *app){
    // 创建数据接收轮询任务
    ltx_Task_set_name(&task_usb_get, "usb_get");
    ltx_Task_set_period(&task_usb_get, 1, 1);
    ltx_Task_set_callback(&task_usb_get, task_func_usb_get);
    ltx_Task_init(&task_usb_get, app);

    pack_counter_reset();

    ltx_Alarm_set_count(&alarm_sleep, SLEEP_TICKS_COUNT);
    ltx_Alarm_add(&alarm_sleep);

    return 0;
}

int myAPP_usb_pause(struct ltx_App_stu *app){

    return 0;
}

int myAPP_usb_resume(struct ltx_App_stu *app){

    return 0;
}

int myAPP_usb_destroy(struct ltx_App_stu *app){

    // free...

    return 0;
}

struct ltx_App_stu app_usb = {
    .is_initialized = 0,
    .status = ltx_App_status_pause,
    .name = "usb",

    .init = myAPP_usb_init,
    .pause = myAPP_usb_pause,
    .resume = myAPP_usb_resume,
    .destroy = myAPP_usb_destroy,

    .task_list = NULL,
    
    .next = NULL,
};


#include "usb_conf.h"
#include "wk_system.h"

#include "usbd_int.h"
#include "cdc_class.h"
#include "cdc_desc.h"

extern usbd_core_type usb_core_dev;
extern cdc_struct_type cdc_struct;

uint8_t usb_tx_buf0[64] = {0, 0, 0};
uint8_t *usbd_rx_buffer0 = cdc_struct.g_rx_buff;
uint8_t usbd_rx_buffer1[64] = {0, 0, 0};
uint8_t *usb_rx_buffers[] = {cdc_struct.g_rx_buff, usbd_rx_buffer1};
uint8_t usb_rx_buffer_now = 0;
uint8_t *usb_rx_buffer_p_now = cdc_struct.g_rx_buff;

// 待重发命令缓存（单槽位）：TX 忙时命令先存这里，下个任务周期重试，
// 避免 /a、/k 等关键回包被静默丢弃导致上位机超时误判
static uint8_t usb_tx_pending_buf[8];
static uint8_t usb_tx_pending_len = 0;

// 发送命令回包；TX 忙时存入待重发缓存（新命令覆盖旧命令，/q0 /q1 自身有周期重发，无需走这里）
static void usb_cmd_send(const uint8_t *cmd, uint8_t len){
    if(cdc_struct.g_tx_completed){
        cdc_struct.g_tx_completed = 0;
        for(uint8_t i = 0; i < len; i ++){
            usb_tx_buf0[i] = cmd[i];
        }
        usbd_ept_send(&usb_core_dev, USBD_CDC_BULK_IN_EPT, usb_tx_buf0, len);
        usb_tx_pending_len = 0;
    }else {
        for(uint8_t i = 0; i < len; i ++){
            usb_tx_pending_buf[i] = cmd[i];
        }
        usb_tx_pending_len = len;
    }
}

// 在任务周期开头调用：重试上一周期因 TX 忙未能发出的命令
static void usb_cmd_pending_retry(void){
    if(usb_tx_pending_len && cdc_struct.g_tx_completed){
        cdc_struct.g_tx_completed = 0;
        for(uint8_t i = 0; i < usb_tx_pending_len; i ++){
            usb_tx_buf0[i] = usb_tx_pending_buf[i];
        }
        usbd_ept_send(&usb_core_dev, USBD_CDC_BULK_IN_EPT, usb_tx_buf0, usb_tx_pending_len);
        usb_tx_pending_len = 0;
    }
}
// uint8_t *flag_usb_rx_cplt = &cdc_struct.g_rx_completed;
// 在 usb 中断回调里直接置 1 usb 接收任务运行标志位，主循环一旦空闲立即运行读取任务而无需 1ms 后轮询
uint8_t *flag_usb_task_run = &task_usb_get.topic.flag;

void task_func_usb_get(void *param){
    static uint8_t flag_in_rx_pic = 0;
    // 播放状态跟踪（v2 协议：/1 /0 幂等设态的状态判据）
    // 初值 0 = 暂停，与设备上电默认状态（摇臂抬起、不旋转）一致；
    // topic_player_pause/resume 仅由 /0 /1 发布，手动拨臂经上位机中转最终也回到这两个命令，因此该变量与现实一致
    static uint8_t usb_player_playing = 0;
    uint16_t pack_lost_counter;

    uint16_t length;
    // uint8_t buffer_index;
    uint16_t pic_update_index;
    uint16_t first_lost_pack_index;
    // static uint32_t counter_get_times = 0;

    // 优先重试因 TX 忙未发出的命令回包
    usb_cmd_pending_retry();

    if(cdc_struct.g_rx_completed){
        cdc_struct.g_rx_completed = 0;
        length = cdc_struct.g_rxlen;

        // buffer_index = usb_rx_buffer_now;
        usb_rx_buffer_p_now = usb_rx_buffers[usb_rx_buffer_now];

        // 切换 buffer 立即接收下一包，感觉可以放中断里，多搞几个 buffer
        usb_rx_buffer_now = (usb_rx_buffer_now+1)%2;
        usbd_ept_recv(&usb_core_dev, USBD_CDC_BULK_OUT_EPT, usb_rx_buffers[usb_rx_buffer_now], USBD_CDC_OUT_MAXPACKET_SIZE);

        // LOG_FMT("B%d, %d\n", buffer_index, length);
        // 处理接收到的数据
        // switch(usb_rx_buffers[buffer_index][0]){
        switch(usb_rx_buffer_p_now[0]){
            case '/': // 上位机命令包头
                // LOG_FMT("Get cmd:%s\n", usb_rx_buffers[buffer_index]);
                // switch(usb_rx_buffers[buffer_index][1]){
                switch(usb_rx_buffer_p_now[1]){
                    case '0': // 暂停播放（幂等设态：仅状态翻转时驱动摇臂脚本，重复 /0 无副作用）
                        if(usb_player_playing){
                            usb_player_playing = 0;
                            ltx_Topic_publish(&topic_player_pause);
                        }
                        disp_pic_rotate(0);
                        _usb_get_up();

                        break;

                    case '1': // 继续播放（幂等设态：仅状态翻转时驱动摇臂脚本；
                              // disp_pic_rotate(1) 显示侧幂等，重复调用用于封面传输后恢复转动）
                        if(!usb_player_playing){
                            usb_player_playing = 1;
                            ltx_Topic_publish(&topic_player_resume);
                        }
                        disp_pic_rotate(1);
                        _usb_get_up();

                        break;

                    case 'v': // 协议版本查询：v2 = 支持幂等设态播放命令（/1 /0 不再翻转摇臂）
                        usb_cmd_send((const uint8_t *)"/v2\n", 5);
                        _usb_get_up();

                        break;
                        
                    case 't': // 上位机准备发送图片
                        // 暂停图片旋转
                        disp_pic_rotate(0);
                        disp_angle = 0;

                        // 切换为准备接收图片状态
                        flag_in_rx_pic = 1;

                        // 重置丢包计数
                        pack_counter_reset();

                        // 发送准备 ok 命令（TX 忙时自动转入待重发缓存）
                        usb_cmd_send((const uint8_t *)"/k\n", 4);

                        break;

                    case 'h': // 心跳：仅重置休眠计时，不改变播放状态机与显示
                        _usb_get_up();

                        break;
                        
                    case 'o': // 传输结束
                        // 判断是否有丢包
                        pack_lost_counter = pack_get_lost_counter();
                        if(!pack_lost_counter){ // 无丢包
                            // 播放图片下落动画
                            disp_pic_down();
                            flag_in_rx_pic = 0;

                            // 发送接收完毕消息（TX 忙时自动转入待重发缓存，不再静默丢弃）
                            usb_cmd_send((const uint8_t *)"/a\n", 4);

                            break;
                        }
                        // 有丢包
                        first_lost_pack_index = pack_get_first_lost_index();
                        uint8_t cmd_retry[6] = {'/', 0, (uint8_t)(first_lost_pack_index>>8),
                                                (uint8_t)(first_lost_pack_index & 0xFF), '\n', '\0'};
                        if(pack_lost_counter < 10){ // 丢包数量较小，挨个请求
                            cmd_retry[1] = 'r';
                        }else { // 丢包数量较大，要求从第一个丢包处开始重发图片
                            cmd_retry[1] = 'x';
                        }
                        // TX 忙时自动转入待重发缓存
                        usb_cmd_send(cmd_retry, 6);

                        break;

                    default:

                        break;
                }
                break;

            case '#': // 图片数据包头
                if(!flag_in_rx_pic){ // 不处于准备接收图片状态，不响应
                    break;
                }
                // 计算索引
                // pic_update_index = ((uint16_t)usb_rx_buffers[buffer_index][1] << 8) | usb_rx_buffers[buffer_index][2];
                pic_update_index = ((uint16_t)usb_rx_buffer_p_now[1] << 8) | usb_rx_buffer_p_now[2];
                // 修复：原来用 >，放行 index==1888 导致越界写 29 字节；合法序号范围是 0~1887
                if(pic_update_index >= PIC_PACK_NUMS){
                    break;
                }
                // 复制数据
                for(uint8_t i = 0; i < 61; i ++){
                    ((uint8_t *)picture_buffer)[pic_update_index * 61 + i] = usb_rx_buffer_p_now[i + 3];
                }

                // 统计包
                pack_count_in(pic_update_index);

                // 收图也算上位机活动，重置休眠计时，避免长传输期间倒计时归零休眠
                _usb_get_up();

                // LOG_FMT("Get pic:%d\n", pic_update_index);
                break;

            default: // 未知格式，不处理

                break;
        }
        /*
        usb_rx_buffers[buffer_index][0] = 0;
        usb_rx_buffers[buffer_index][1] = 0;
        usb_rx_buffers[buffer_index][2] = 0;
        */
        usb_rx_buffer_p_now[0] = 0;
        usb_rx_buffer_p_now[1] = 0;
        usb_rx_buffer_p_now[2] = 0;

        // counter_get_times ++;
        // LOG_FMT("U %d, %d\n", counter_get_times, length);
    }
}

// 休眠闹钟
void alarm_cb_sleep(void *param){
    flag_is_sleeping = 1;

    // 关闭图片旋转
    disp_pic_rotate(0);
    // 关闭背光
    myLCD.set_backlight(0);
}

// 唤醒下位机
void _usb_get_up(void){
    ltx_Alarm_set_count(&alarm_sleep, SLEEP_TICKS_COUNT);
    if(flag_is_sleeping){
        ltx_Alarm_add(&alarm_sleep);
        myLCD.set_backlight(100);
    }
    flag_is_sleeping = 0;
}


void send_str_2_usb(uint8_t *str, uint8_t len){
    if(flag_is_sleeping){
        return ;
    }

    if(cdc_struct.g_tx_completed){
        cdc_struct.g_tx_completed = 0;
        uint8_t i;
        for(i = 0; i < len; i ++){
            usb_tx_buf0[i] = str[i];
        }
        usb_tx_buf0[i] = '\0';
        usbd_ept_send(&usb_core_dev, USBD_CDC_BULK_IN_EPT, usb_tx_buf0, i);
    }else { // 正在忙于发送其他数据
        // 
    }
}
