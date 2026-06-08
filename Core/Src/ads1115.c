/**
 * @file ads1115.c
 * @brief 针对 STM32G0 系列优化的 ADS1115 驱动源文件
 * @note 已升级支持 AIN0 和 AIN1 双通道单端轮询采集，量程±4.096V，速率860 SPS。
 */

#include "hal.h"
#include "ads1115.h"

// ============================================================================
// 静态局部变量
// ============================================================================
/* 内部寄存器映像数组，用于缓存在单片机内存中 */
static uint16_t registerMap[4];

// ============================================================================
// 内部辅助函数声明
// ============================================================================
static uint16_t combineBytes(uint8_t upperByte, uint8_t lowerByte);

// ============================================================================
// 核心驱动功能函数
// ============================================================================

/**
 * @brief  ADS1115 启动初始化序列
 * @note   在使用此函数前，确保单片机的时钟和 I2C 外设已通过 CubeMX 初始化完成。
 */
void adcStartup(void)
{
    // 1. 给硬件上电提供短暂的稳定时间
    delay_ms(50);

    // 2. 初始化单片机本地的寄存器默认映射
    registerMap[0] = 0x0000;          // DATA 寄存器默认
    registerMap[1] = CONFIG_DEFAULT;  // CONFIG 寄存器默认 (0x8583)
    registerMap[2] = 0x8000;          // LO_THRESH 默认
    registerMap[3] = 0x7FFF;          // HI_THRESH 默认

    // 3. 软件复位一下芯片，确保状态干净
    resetDevice();
}

/**
 * @brief  启动指定通道的单次转换并读取 ADC 采样数据
 * @param  channel: 目标通道选择 (0 代表 AIN0-GND, 1 代表 AIN1-GND)
 * @return 16位有符号补码 ADC 原始数据
 */
int16_t readChannelData(uint8_t channel)
{
    uint8_t regData[2];
    uint8_t regRXdata[2] = {0};
    uint16_t configValue = 0;

    // 1. 组装配置寄存器的值
    // OS(1): 开始单次转换
    // PGA(001): ±4.096V 量程
    // MODE(1): 单次转换模式
    // DR(101): 修改为 250 SPS 采样率 (二进制 101)，大幅降低高频噪声！
    // COMP(00011): 禁用比较器
    if (channel == 0)
    {
    	// MUX(100) + PGA(±4.096V) + MODE(单次) + DR(475 SPS)
    	// 二进制: 1 100 001 1  110 000 11 = 0xC3C3
    	configValue = 0xC3C3;
    }
    else if (channel == 1)
    {
    	// 二进制: 1 101 001 1  110 000 11 = 0xD3C3
        configValue = 0xD3C3;
    }
    else
    {
        return 0;
    }

    // 2. 写入配置寄存器，触发单次转换
    regData[0] = (uint8_t)(configValue >> 8);
    regData[1] = (uint8_t)(configValue & 0xFF);

    if (sendI2CData(CONFIG_ADDRESS, regData, 2) != 0)
    {
        return 0;
    }

    // 3. 等待转换完成
    // 在 475 SPS 速率下，单次转换理论耗时 2.1ms。
    // 为了保证芯片数字滤波器稳健建立，这里将延时安全值调整为 3ms。
    // (注意：这也意味着单次 readChannelData 会占用 3ms 阻塞时间)
    delay_ms(3);

    // 4. 读取转换寄存器
    if (receiveI2CDataNoWrite(CONVERSION_ADDRESS, regRXdata, 2, false) == 0)
    {
        return (int16_t)combineBytes(regRXdata[0], regRXdata[1]);
    }

    return 0;
}

/**
 * @brief  写入数据到指定的寄存器 (保留原 API 结构)
 */
bool writeSingleRegister(uint8_t address, uint16_t data)
{
    uint8_t regData[2];
    uint8_t readBackData[2];
    uint16_t checkedValue;

    if (address == 0) return true;

    regData[0] = (uint8_t)(data >> 8);
    regData[1] = (uint8_t)(data & 0xFF);

    if (sendI2CData(address, regData, 2) != 0) return true;
    if (receiveI2CData(address, readBackData, 2) != 0) return true;

    checkedValue = combineBytes(readBackData[0], readBackData[1]);
    registerMap[address] = checkedValue;

    if ((checkedValue & 0x7FFF) != (data & 0x7FFF)) return true;

    return false;
}

/**
 * @brief  纯写入寄存器，不进行任何 I2C 回读校验
 */
bool writeSingleRegisterNoRead(uint8_t address, uint16_t data)
{
    uint8_t regData[2];
    if (address == 0) return false;

    regData[0] = (uint8_t)(data >> 8);
    regData[1] = (uint8_t)(data & 0xFF);

    if (sendI2CRegPointer(address, regData, 2) != 0) return false;

    registerMap[address] = data;
    return true;
}

/**
 * @brief  从指定寄存器读取 16 位原始数据
 */
uint16_t readSingleRegister(uint8_t address)
{
    uint8_t regRXdata[2] = {0};
    uint16_t regValue = 0;

    if (receiveI2CData(address, regRXdata, 2) != 0) return 0;

    regValue = combineBytes(regRXdata[0], regRXdata[1]);
    registerMap[address] = regValue;

    return regValue;
}

/**
 * @brief  软件复位整个 ADS1115 芯片
 */
bool resetDevice(void)
{
    uint8_t resetCmd = 0x06;
    if (sendI2CData(0x00, &resetCmd, 1) != 0) return true;
    registerMap[CONFIG_ADDRESS] = CONFIG_DEFAULT;
    return false;
}

/**
 * @brief  获取本地缓存的最新寄存器值
 */
uint16_t getRegisterValue(uint8_t address)
{
    return registerMap[address];
}

// ============================================================================
// 内部辅助数据处理工具函数
// ============================================================================
static uint16_t combineBytes(uint8_t upperByte, uint8_t lowerByte)
{
    return (((uint16_t)upperByte << 8) | (uint16_t)lowerByte);
}
