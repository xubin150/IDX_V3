/*
 * flash_config.h
 *
 *  Created on: Jun 23, 2026
 *      Author: Administrator
 */

#ifndef INC_FLASH_CONFIG_H_
#define INC_FLASH_CONFIG_H_


#include "main.h"

// STM32G030F6 最后一页的起始地址 (Page 15)
#define CONFIG_FLASH_ADDR 0x08007800
#define CONFIG_FLAG       0x55AA

// 强制 1 字节对齐，并确保总大小是 8 的倍数
// (因为 G0 的 Flash 写入要求必须是 64-bit 双字对齐)
#pragma pack(1)
typedef struct {
    uint16_t head;          // 标识位 0x55AA
    uint8_t  ip[4];         // 本机 IP
    uint8_t  subnet[4];     // 子网掩码
    uint8_t  gateway[4];    // 网关
    uint8_t  target_ip[4];  // 目的 IP
    uint16_t target_port;   // 目的端口
    uint8_t  reserved[6];   // 预留，凑满 24 字节 (3 个 uint64_t)
} NetConfig_t;
#pragma pack()

extern NetConfig_t DeviceConfig;

void Config_Init(void);
void Config_Save(void);


#endif /* INC_FLASH_CONFIG_H_ */
