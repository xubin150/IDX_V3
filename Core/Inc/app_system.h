/*
 * app_system.h
 *
 *  Created on: Jul 16, 2026
 *      Author: Administrator
 */

#ifndef INC_APP_SYSTEM_H_
#define INC_APP_SYSTEM_H_


#include <stdint.h>
#include <stdbool.h>

// 模块裁剪宏开关 (0为关闭，1为开启。关闭后编译器自动剔除该模块代码)
#define ENABLE_MODULE_FILTER      1  // 滤波模块
#define ENABLE_MODULE_ROTATION    1  // 坐标旋转模块
#define ENABLE_MODULE_EEPROM      1  // 掉电保存模块

// 系统工作参数集合
typedef struct {
    // 1. 硬件控制参数
    uint8_t  tx_channel;      // 当前信道 (1-5)
    uint8_t  tx_pwm_duty;     // 发射功率 (10-50)
    uint8_t  pre_gain;        // 前置放大档位 (1-4)
    uint8_t adc_pga;   		  //ADC PGA 增益设置
    uint8_t	 led_mode;		//LED功能模式
    uint8_t threshold_level; // 【新增】灵敏度档位 (0~9，数字越大越灵敏)
    // 2. 算法参数
    uint8_t  filter_type;     // 0:无, 1:EMA, 2:Biquad
    int16_t  phase_offset;    // 系统相位误差角 (经过一键校准后得到)

    // 3. 实时数据流 (ADC -> 滤波 -> 旋转 -> 串口)
    int16_t  raw_I;
    int16_t  raw_Q;
    int16_t  processed_I;
    int16_t  processed_Q;
    int16_t baseline_I;

    // 4. 状态标志位
    bool     need_save_eeprom; // 参数修改后，置位此标志要求保存
    bool     target_detected;  // 目标检测标志 (用于LED指示)
    bool 	req_auto_zero; //一键归0
} SystemCtrl_t;

extern SystemCtrl_t g_SysCtrl; // 全局唯一实例声明


#endif /* INC_APP_SYSTEM_H_ */
