/**
 * @file ads1115.h
 * @brief 针对 STM32G0 系列优化的 ADS1115 驱动头文件
 * @note 已清理 TI 专属宏，删除了不必要的库引用，并声明了最新的双通道采样 API。
 */

#ifndef ADS1115_H_
#define ADS1115_H_

//****************************************************************************
//
// 基础标准库与硬件抽象层头文件
//
//****************************************************************************
#include <stdbool.h>
#include <stdint.h>
#include "hal.h"

//****************************************************************************
//
// 全局变量声明与基础宏定义
//
//****************************************************************************
extern const char *adcRegisterNames[];

/** 设备基础 I2C 7位物理地址 (ADDR引脚接地) */
#define ADS1115_ADDRESS                         0x48

/** 寄存器数量相关常量 */
#define NUM_REGISTERS                           ((uint8_t) 4)
#define MAX_REGISTER_ADDRESS                    ((uint8_t) 3)

// PGA 增益档位枚举 (对应 Config 寄存器 Bit[11:9])
typedef enum {
    ADS1115_PGA_6_144V = 0x00, // 000: 量程 ±6.144V
    ADS1115_PGA_4_096V = 0x01, // 001: 量程 ±4.096V (默认)
    ADS1115_PGA_2_048V = 0x02, // 010: 量程 ±2.048V
    ADS1115_PGA_1_024V = 0x03, // 011: 量程 ±1.024V
    ADS1115_PGA_0_512V = 0x04, // 100: 量程 ±0.512V
    ADS1115_PGA_0_256V = 0x05  // 101: 量程 ±0.256V
} ADS1115_PGA_e;


//**********************************************************************************
//
// 核心驱动应用层功能函数声明
//
//**********************************************************************************
// === 新增的对外接口 ===
void ADS1115_SetPGA(ADS1115_PGA_e pga_mode);
ADS1115_PGA_e ADS1115_GetPGA(void);

/** @brief 初始化并复位 ADC 芯片 */
void adcStartup(void);

/** @brief 启动单次转换并读取指定通道的 16位有符号原始数据 */
int16_t readChannelData(uint8_t channel);

/** @brief 写入 16位数据到指定的寄存器 (带硬件回读校验) */
bool writeSingleRegister(uint8_t address, uint16_t data);

/** @brief 写入 16位数据到指定的寄存器 (无校验，速度快) */
bool writeSingleRegisterNoRead(uint8_t address, uint16_t data);

/** @brief 从指定寄存器回读 16位原始数据 */
uint16_t readSingleRegister(uint8_t address);

/** @brief 软件复位整个 ADS1115 芯片 */
bool resetDevice(void);

/** @brief 获取单片机本地缓存的最新寄存器映像值 */
uint16_t getRegisterValue(uint8_t address);

void ADS1115_Start_Conversion(uint8_t channel);
int16_t ADS1115_Read_Result(void);
//**********************************************************************************
//
// ADS1115 内部寄存器地址及配置宏定义 (Register Definitions)
//
//**********************************************************************************

/* ==========================================
   0x00: 转换寄存器 (CONVERSION REGISTER)
   ========================================== */
#define CONVERSION_ADDRESS                      ((uint16_t) 0x00)
#define CONVERSION_DEFAULT                     ((uint16_t) 0x0000)

/* ==========================================
   0x01: 配置寄存器 (CONFIG REGISTER)
   ========================================== */
#define CONFIG_ADDRESS                          ((uint16_t) 0x01)
#define CONFIG_DEFAULT                          ((uint16_t) 0x8583) // 上电默认值

/* 配置寄存器 各功能字段掩码 */
#define CONFIG_OS_MASK                          ((uint16_t) 0x8000) // 运行状态/触发单次转换
#define CONFIG_MUX_MASK                         ((uint16_t) 0x7000) // 输入通道选择
#define CONFIG_PGA_MASK                         ((uint16_t) 0x0E00) // 可编程增益量程
#define CONFIG_MODE_MASK                        ((uint16_t) 0x0100) // 工作模式选择
#define CONFIG_DR_MASK                          ((uint16_t) 0x00E0) // 数据采样速率
#define CONFIG_COMP_QUE_MASK                    ((uint16_t) 0x0003) // 比较器队列与禁用

/* OS (Bit 15) 字段定义 */
#define CONFIG_OS_CONV_START                    ((uint16_t) 0x8000) // 写入此位启动单次转换

/* MUX (Bit 14:12) 输入通道定义 */
#define CONFIG_MUX_AIN0_GND                     ((uint16_t) 0x4000) // AIN0 单端对地
#define CONFIG_MUX_AIN1_GND                     ((uint16_t) 0x5000) // AIN1 单端对地
#define CONFIG_MUX_AIN2_GND                     ((uint16_t) 0x6000) // AIN2 单端对地
#define CONFIG_MUX_AIN3_GND                     ((uint16_t) 0x7000) // AIN3 单端对地

/* PGA (Bit 11:9) 量程增益定义 */
#define CONFIG_PGA_6p144V                       ((uint16_t) 0x0000) // ±6.144V
#define CONFIG_PGA_4p096V                       ((uint16_t) 0x0200) // ±4.096V
#define CONFIG_PGA_2p048V                       ((uint16_t) 0x0400) // ±2.048V
#define CONFIG_PGA_1p024V                       ((uint16_t) 0x0600) // ±1.024V

/* MODE (Bit 8) 工作模式定义 */
#define CONFIG_MODE_CONT                        ((uint16_t) 0x0000) // 连续转换模式
#define CONFIG_MODE_SS                          ((uint16_t) 0x0100) // 单次转换省电模式

/* DR (Bit 7:5) 数据速率定义 (针对 ADS1115 芯片标准) */
#define CONFIG_DR_8SPS                          ((uint16_t) 0x0000)
#define CONFIG_DR_16SPS                         ((uint16_t) 0x0020)
#define CONFIG_DR_32SPS                         ((uint16_t) 0x0040)
#define CONFIG_DR_64SPS                         ((uint16_t) 0x0060)
#define CONFIG_DR_128SPS                        ((uint16_t) 0x0080)
#define CONFIG_DR_250SPS                        ((uint16_t) 0x00A0) // 当前工程首选：250 SPS
#define CONFIG_DR_475SPS                        ((uint16_t) 0x00C0)
#define CONFIG_DR_860SPS                        ((uint16_t) 0x00E0)

/* COMP_QUE (Bit 1:0) 比较器禁用定义 */
#define CONFIG_COMP_QUE_DISABLE                 ((uint16_t) 0x0003) // 禁用比较器并让 ALERT 引脚保持高阻抗

/* ==========================================
   0x02 & 0x03: 阈值比较寄存器 (通常不用)
   ========================================== */
#define LO_THRESH_ADDRESS                       ((uint16_t) 0x02)
#define LO_THRESH_DEFAULT                       ((uint16_t) 0x8000)

#define HI_THRESH_ADDRESS                       ((uint16_t) 0x03)
#define HI_THRESH_DEFAULT                       ((uint16_t) 0x7FFF)

#endif /* ADS1115_H_ */
