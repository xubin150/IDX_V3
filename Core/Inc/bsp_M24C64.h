/*
 * bsp_AT24C02.h
 *
 *  Created on: Dec 2, 2025
 *      Author: Administrator
 */

#ifndef SRC_BSP_AT24C02_H_
#define SRC_BSP_AT24C02_H_
#include "main.h"
#include "bsp_IIC.h"

#define ADDR 0xA0

// 1. 写单个字节 (修改为 16 位地址发送)
void M24C64_WriteByte(uint16_t WordAddress, uint8_t Byte);
uint8_t M24C64_ReadByte(uint16_t address);

#endif /* SRC_BSP_AT24C02_H_ */
