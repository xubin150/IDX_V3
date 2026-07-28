/*
 * app_cmd_parser.h
 *
 *  Created on: Jul 16, 2026
 *      Author: Administrator
 */

#ifndef INC_APP_CMD_PARSER_H_
#define INC_APP_CMD_PARSER_H_

#include <stdint.h>
#include <stdbool.h>
#include "app_system.h" // 引入全局系统状态结构体

// ============================================================================
// 通讯协议定义
// 帧结构: [帧头1] [帧头2] [指令码] [参数/数据] [校验和] [帧尾]
// ============================================================================
#define CMD_HEAD1       0x5A
#define CMD_HEAD2       0xA5
#define CMD_TAIL        0x0D
#define CMD_FRAME_LEN   6      // 假设所有控制指令固定为 6 字节长度

// ============================================================================
// 指令集枚举 (映射硬件需求书中的功能)
// ============================================================================
typedef enum {
    CMD_SET_TX_CHANNEL = 0x01,  // 设置 TX 发射信道 (CH1-CH5)
    CMD_SET_TX_POWER   = 0x02,  // 设置 TX 发射功率 (10%-50%)
    CMD_SET_PRE_GAIN   = 0x03,  // 设置 前端硬件放大倍数 (R_x1-R_x4)
    CMD_SET_ADC_PGA    = 0x04,  // 设置 ADC PGA 量程 (0-5)
    CMD_SET_FILTER     = 0x05,  // 设置 软件滤波算法 (0:无, 1:EMA, 2:Biquad)
    CMD_AUTO_ZERO      = 0x06,  // 触发 坐标旋转/自动归零
    CMD_SET_LED_MODE   = 0x07,   // 设置 LED 工作模式
	CMD_SET_THRESHOLD =0x08   //设置金属阈值档位
} AppCommand_e;

// ============================================================================
// 对外接口
// ============================================================================
void CmdParser_ReceiveByte(uint8_t rx_byte); // 串口接收中断里调用，推入单字节
bool CmdParser_HasNewCmd(void);              // 检查是否有完整有效指令
void CmdParser_Execute(SystemCtrl_t *ctrl);  // 执行指令



#endif /* INC_APP_CMD_PARSER_H_ */
