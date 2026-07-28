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
// 保存当前的 PGA 设定，默认为 ±4.096V
static ADS1115_PGA_e s_current_pga = ADS1115_PGA_4_096V;

// ============================================================================
// 内部辅助函数声明
// ============================================================================
static uint16_t combineBytes(uint8_t upperByte, uint8_t lowerByte);

// ============================================================================
// 核心驱动功能函数
// ============================================================================

/**
 * @brief  设置 ADC 的 PGA 量程
 * @param  pga_mode: 参见 ADS1115_PGA_e 枚举
 */
void ADS1115_SetPGA(ADS1115_PGA_e pga_mode)
{
    if (pga_mode <= ADS1115_PGA_0_256V) {
        s_current_pga = pga_mode;
    }
}
/**
 * @brief  获取当前 ADC 的 PGA 量程
 */
ADS1115_PGA_e ADS1115_GetPGA(void)
{
    return s_current_pga;
}
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

    // 1. 动态组装配置寄存器的值
	// OS(Bit 15) = 1: 开始单次转换 (0x8000)
	// MODE(Bit 8) = 1: 单次转换模式 (0x0100)
	// DR(Bit 7:5) = 110: 475 SPS (0x00C0)
	// COMP(Bit 4:0) = 00011: 禁用比较器 (0x0003)
	// 基础固定配置: 0x8000 | 0x0100 | 0x00C0 | 0x0003 = 0x81C3
    uint16_t base_config = 0x81C3;
    // 注入 PGA (Bit 11:9)
    base_config |= ((uint16_t)s_current_pga << 9);
    if (channel == 0)
    {
    	// AIN0 通道 (MUX = 100 -> Bit 14:12 = 0x4000)
    	configValue = base_config | 0x4000;
    }
    else if (channel == 1)
    {
    	// AIN1 通道 (MUX = 101 -> Bit 14:12 = 0x5000)
    	configValue = base_config | 0x5000;
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
 * @brief  [非阻塞] 启动 ADS1115 特定通道的转换
 * @param  channel: 0 表示 AIN0, 1 表示 AIN1
 * @note   发送完配置指令后立刻返回，绝生死等！
 */
void ADS1115_Start_Conversion(uint8_t channel)
{
    uint8_t regData[2];
    uint16_t configValue = 0;

    // 1. 动态组装配置寄存器的值
    // OS(Bit 15) = 1: 开始单次转换
    // MODE(Bit 8) = 1: 单次转换模式
    // DR(Bit 7:5) = 110: 475 SPS
    // COMP(Bit 4:0) = 00011: 禁用比较器
    uint16_t base_config = 0x81C3;

    // 注入 PGA (Bit 11:9)
    base_config |= ((uint16_t)s_current_pga << 9);

    if (channel == 0)
    {
        // AIN0 通道 (MUX = 100 -> Bit 14:12 = 0x4000)
        configValue = base_config | 0x4000;
    }
    else if (channel == 1)
    {
        // AIN1 通道 (MUX = 101 -> Bit 14:12 = 0x5000)
        configValue = base_config | 0x5000;
    }
    else
    {
        return; // 无效通道，直接退出
    }

    // 2. 写入配置寄存器，触发单次转换
    regData[0] = (uint8_t)(configValue >> 8);
    regData[1] = (uint8_t)(configValue & 0xFF);

    // 发送 I2C 数据 (这里的 sendI2CData 只有十几个字节的通信，耗时约100微秒，不影响系统宏观实时性)
    sendI2CData(CONFIG_ADDRESS, regData, 2);
}

/**
 * @brief  [非阻塞] 读取 ADS1115 转换好的数据
 * @retval 16位有符号电压值
 * @note   调用此函数前，请确保距离上次 Start_Conversion 已过去至少 2.2 毫秒
 */
int16_t ADS1115_Read_Result(void)
{
    uint8_t regRXdata[2] = {0};

    // 直接读取转换寄存器的数据
    // receiveI2CDataNoWrite 不包含转换等待，直接去总线上拉取数据
    if (receiveI2CDataNoWrite(CONVERSION_ADDRESS, regRXdata, 2, false) == 0)
    {
        return (int16_t)combineBytes(regRXdata[0], regRXdata[1]);
    }

    return 0; // 读取失败时的安全默认值
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
