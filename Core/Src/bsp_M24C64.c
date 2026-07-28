/*
 * bsp_AT24C02.c
 *
 *  Created on: Dec 2, 2025
 *      Author: Administrator
 */
#include <bsp_M24C64.h>


void M24C64_WriteByte(uint16_t WordAddress, uint8_t Byte)
{
    I2C_Start();
    Send_Byte(ADDR);
    I2C_WaitAck();

    // 先发送高 8 位地址
	Send_Byte((uint8_t)(WordAddress >> 8));
	I2C_WaitAck();
	// 再发送低 8 位地址
	Send_Byte((uint8_t)(WordAddress & 0xFF));
	I2C_WaitAck();

    Send_Byte(Byte);
    I2C_WaitAck();

    I2C_Stop();
    HAL_Delay(5);

}
uint8_t M24C64_ReadByte(uint16_t address)
{
	uint8_t data ;

	    I2C_Start();
	    Send_Byte(ADDR);
	    I2C_WaitAck();

	    // 先发送高 8 位地址
	    Send_Byte((uint8_t)(address >> 8));
	    I2C_WaitAck();

	    // 再发送低 8 位地址
	    Send_Byte((uint8_t)(address & 0xFF));
	    I2C_WaitAck();

	    I2C_Start();
	    Send_Byte(ADDR | 0x01); // 切换为读模式[cite: 1]
	    I2C_WaitAck();
	    data = I2C_ReceiveByte();

	    I2C_send_ac(1); // 接收完毕发送 NACK[cite: 1, 2]
	    I2C_Stop();
	    return data ;

}

