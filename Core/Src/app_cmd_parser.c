/*
 * app_cmd_parser.c
 *
 *  Created on: Jul 16, 2026
 *      Author: Administrator
 */


#include "app_cmd_parser.h"
#include "ads1115.h"
// #include "app_hardware.h" // 你的硬件控制抽象层，用于操作定时器、GPIO等
// #include "app_signal_process.h" // 用于清除滤波器历史状态

// ============================================================================
// 静态私有变量 (解析器状态机)
// ============================================================================
static uint8_t  s_rx_buffer[CMD_FRAME_LEN];
static uint8_t  s_rx_index = 0;
static bool     s_cmd_ready = false;

// ============================================================================
// 内部辅助函数
// ============================================================================
/**
 * @brief 计算校验和 (除去帧头、校验位和帧尾)
 */
static uint8_t Calc_RxChecksum(uint8_t *data, uint16_t len) {
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

// ============================================================================
// 对外接口实现
// ============================================================================

/**
 * @brief  接收单字节状态机 (建议在 USART 接收中断或 DMA 空闲中断中调用)
 * @param  rx_byte: 接收到的一个字节
 */
void CmdParser_ReceiveByte(uint8_t rx_byte) {
    if (s_cmd_ready) return; // 上一条还没处理完，丢弃新数据

    switch (s_rx_index) {
        case 0:
            if (rx_byte == CMD_HEAD1) s_rx_buffer[s_rx_index++] = rx_byte;
            break;
        case 1:
            if (rx_byte == CMD_HEAD2) s_rx_buffer[s_rx_index++] = rx_byte;
            else s_rx_index = 0; // 头不对，状态机复位
            break;
        default:
            s_rx_buffer[s_rx_index++] = rx_byte;
            if (s_rx_index >= CMD_FRAME_LEN) {
                // 收到一帧，检查帧尾和校验和
                uint8_t calc_sum = Calc_RxChecksum(&s_rx_buffer[2], 2); // 仅校验 CMD 和 DATA
                if (s_rx_buffer[CMD_FRAME_LEN - 1] == CMD_TAIL && s_rx_buffer[4] == calc_sum) {
                    s_cmd_ready = true; // 校验通过，标记指令就绪
                }
                s_rx_index = 0; // 准备接下一帧
            }
            break;
    }
}

/**
 * @brief  轮询检查标志位
 */
bool CmdParser_HasNewCmd(void) {
    return s_cmd_ready;
}

/**
 * @brief  执行指令的核心逻辑
 * @param  ctrl: 全局系统状态指针
 */
void CmdParser_Execute(SystemCtrl_t *ctrl) {
    if (!s_cmd_ready) return;

    uint8_t cmd_id = s_rx_buffer[2];
    uint8_t cmd_data = s_rx_buffer[3];

    switch (cmd_id) {

        // ----------------------------------------------------
        // 模块一：TX 发射频率 (信道) 设置 ok
        // ----------------------------------------------------
        case CMD_SET_TX_CHANNEL:
            if (cmd_data >= 1 && cmd_data <= 5) {
                ctrl->tx_channel = cmd_data;
                 Hardware_Set_TX_Freq(ctrl->tx_channel); // 联动底层修改 TIM ARR
                ctrl->need_save_eeprom = true;
            }
            break;

        // ----------------------------------------------------
        // 模块三：TX 发射功率 (PWM 占空比) 设置 ok
        // ----------------------------------------------------
        case CMD_SET_TX_POWER:
            if (cmd_data >= 10 && cmd_data <= 50) {
                ctrl->tx_pwm_duty = cmd_data;
                 Hardware_Set_TX_PWM(ctrl->tx_pwm_duty); // 联动底层修改 TIM CCR
                ctrl->need_save_eeprom = true;
            }
            break;

        // ----------------------------------------------------
        // 模块四：前端放大器增益设置 (PA4~PA7 控制) ok
        // ----------------------------------------------------
        case CMD_SET_PRE_GAIN:
            if (cmd_data >= 1 && cmd_data <= 4) {
                ctrl->pre_gain = cmd_data;
                 Hardware_Set_PreGain(ctrl->pre_gain); // 联动底层修改 GPIO 状态
                ctrl->need_save_eeprom = true;
            }
            break;

        // ----------------------------------------------------
        // 模块二：ADC PGA 增益设置  -> ok
        // ----------------------------------------------------
        case CMD_SET_ADC_PGA:
            if (cmd_data <= 5) { // ADS1115_PGA_0_256V 为 5
                ADS1115_SetPGA((ADS1115_PGA_e)cmd_data); // 更新底层硬件状态
                ctrl->adc_pga = cmd_data;                // 更新系统状态
                ctrl->need_save_eeprom = true;           // 标记保存
            }
            break;

        // ----------------------------------------------------
        // 模块五：输出数据数字滤波设置 ok
        // ----------------------------------------------------
        case CMD_SET_FILTER:
            #if ENABLE_MODULE_FILTER
            if (cmd_data <= 2) {
                ctrl->filter_type = cmd_data;
                SignalProcess_ClearFilterHistory(); // 切换算法时，必须清空历史缓存防跳变！
                ctrl->need_save_eeprom = true;
            }
            #endif
            break;

        // ----------------------------------------------------
        // 模块六：执行自动归零 / 旋转坐标 ok
        // ----------------------------------------------------
        case CMD_AUTO_ZERO:
            #if ENABLE_MODULE_ROTATION
            // 收到指令后，不能立刻用当前唯一的采样点去算角度（容易受噪声干扰）。
            // 而是设置一个请求标志，让主循环里的滤波程序在接下来的一段时间内进行平滑计算。
            ctrl->req_auto_zero = true;
            #endif
            break;

        // ----------------------------------------------------
        // 模块七：LED 工作模式指示
        // ----------------------------------------------------
        case CMD_SET_LED_MODE:
            ctrl->led_mode = cmd_data;
            // ctrl->need_save_eeprom = true; // LED模式一般不需要掉电保存，视具体需求而定
            break;

// ----------------------------------------------------
		// 模块八：设置目标报警灵敏度 (档位: 0~9)
		// ----------------------------------------------------
		case CMD_SET_THRESHOLD: // (替换成你实际的指令宏)
			// 1. 安全防护：限制单字节指令最大为 9 档
			if (cmd_data > 9) {
				cmd_data = 9;
			}

			// 2. 将档位写入大脑
			ctrl->threshold_level = cmd_data;

			// 3. 档位属于核心配置，需要存入 EEPROM
			ctrl->need_save_eeprom = true;
			break;
        default:
            // 收到未知指令，直接忽略
            break;
    }

    // 处理完毕，清空标志位，允许接收下一条指令
    s_cmd_ready = false;
}
