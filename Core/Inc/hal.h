/**
 * @file hal.h
 * @brief 针对 STM32G0 系列的硬件抽象层头文件 (Hardware Abstraction Layer)
 * @note 此文件已从 TI 驱动重构为 STM32 HAL 库兼容版本。
 */

#ifndef HAL_H_
#define HAL_H_

//****************************************************************************
//
// 标准库与 STM32 基础头文件
//
//****************************************************************************
#include <stdbool.h>
#include <stdint.h>

/* 引入 STM32 HAL 库及 CubeMX 生成的引脚定义 */
#include "main.h"

/* 引入应用层头文件 */
#include "ads1115.h"

//****************************************************************************
//
// 基础逻辑宏定义
//
//****************************************************************************
/** 逻辑高电平别名 */
#ifndef HIGH
#define HIGH                ((bool) true)
#endif

/** 逻辑低电平别名 */
#ifndef LOW
#define LOW                 ((bool) false)
#endif

//*****************************************************************************
//
// 硬件抽象层函数声明 (供 ads1115.c 调用)
//
//*****************************************************************************

/* 初始化与时钟延时函数 */
void InitADC(void);
void delay_ms(const uint32_t delay_time_ms);
void delay_us(const uint32_t delay_time_us);

/* I2C 发送与接收函数 */
int8_t sendI2CData(uint8_t address, uint8_t *arrayIndex, uint8_t length);
int8_t sendI2CRegPointer(uint8_t address, uint8_t *arrayIndex, uint8_t length);
int8_t receiveI2CData(uint8_t address, uint8_t *arrayIndex, uint8_t length);
int8_t receiveI2CDataNoWrite(uint8_t address, uint8_t *arrayIndex, uint8_t length, bool noWrite);

/* ALERT/RDY 中断相关函数（如不使用中断引脚，底层已留空） */
bool getALERTinterruptStatus(void);
bool waitForALERTinterrupt(const uint32_t timeout_ms);
void setALERTinterruptStatus(const bool value);
void enableALERTinterrupt(const bool intEnable);
void GPIO_ALERT_IRQHandler(uint_least8_t index);

#endif /* HAL_H_ */
