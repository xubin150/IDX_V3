/**
 * @file hal.c
 * @brief 针对 STM32G0 系列的硬件抽象层 (Hardware Abstraction Layer)
 * @note 此文件已从 TI 驱动重构为 STM32 HAL 库驱动，适配 ADS1115。
 */

#include "hal.h"
#include "main.h" // 包含 STM32 HAL 库和 GPIO 引脚的宏定义
#include "i2c.h"  // 包含 CubeMX 自动生成的 I2C 句柄 (hi2c1)

// ============================================================================
// 外部变量声明与宏定义
// ============================================================================

/* 引入 CubeMX 生成的 I2C 句柄，默认通常为 hi2c1 */
extern I2C_HandleTypeDef hi2c1;

/* * ADS1115 的 I2C 设备地址 (ADDR 接地时)。
 * STM32 HAL 库要求包含读写位的 8 位地址，因此 0x48 需要左移 1 位
 */
#ifndef ADS1115_I2C_ADDRESS
#define ADS1115_I2C_ADDRESS    (0x48 << 1)
#endif

// 用于标记 ALERT/RDY 引脚是否触发外部中断的标志位
static volatile bool flag_nALERT_INTERRUPT = false;

// ============================================================================
// 延时函数 (Timing functions)
// ============================================================================

/**
 * @brief 提供毫秒级延时
 * @param delay_time_ms 延时的毫秒数
 */
void delay_ms(const uint32_t delay_time_ms)
{
    HAL_Delay(delay_time_ms); // 直接调用 STM32 HAL 的毫秒延时
}

/**
 * @brief 提供微秒级延时
 * @param delay_time_us 延时的微秒数
 * @note STM32 HAL 没有现成的 us 延时，这里使用基于系统主频的粗略软件循环替代。
 */
void delay_us(const uint32_t delay_time_us)
{
    // 假设系统时钟为 64MHz，简单的空指令循环，可根据实际情况微调
    uint32_t delay = delay_time_us * (SystemCoreClock / 1000000 / 4);
    while (delay--)
    {
        __NOP();
    }
}

// ============================================================================
// I2C 通信函数 (I2C Communication functions)
// ============================================================================

/**
 * @brief  向 ADS1115 的指定寄存器写入数据
 * @param  reg_address: 寄存器指针地址 (例如 0x01 代表配置寄存器)
 * @param  arrayIndex:  要写入的数据数组指针
 * @param  length:      要写入的数据字节数 (通常为 2)
 * @retval 0 成功; -1 失败
 */
int8_t sendI2CData(uint8_t reg_address, uint8_t *arrayIndex, uint8_t length)
{
    /* 使用 HAL_I2C_Mem_Write 可以一次性发送 寄存器地址 + 数据 */
    if (HAL_I2C_Mem_Write(&hi2c1, ADS1115_I2C_ADDRESS, reg_address, I2C_MEMADD_SIZE_8BIT, arrayIndex, length, 100) == HAL_OK)
    {
        return 0;
    }
    return -1;
}

/**
 * @brief  从 ADS1115 的指定寄存器读取数据
 * @param  reg_address: 寄存器指针地址 (例如 0x00 代表转换寄存器)
 * @param  arrayIndex:  用于存放读取数据的数组指针
 * @param  length:      要读取的数据字节数 (通常为 2)
 * @retval 0 成功; -1 失败
 */
int8_t receiveI2CData(uint8_t reg_address, uint8_t *arrayIndex, uint8_t length)
{
    /* 使用 HAL_I2C_Mem_Read 可以一次性发送 寄存器地址 并读取 数据 */
    if (HAL_I2C_Mem_Read(&hi2c1, ADS1115_I2C_ADDRESS, reg_address, I2C_MEMADD_SIZE_8BIT, arrayIndex, length, 100) == HAL_OK)
    {
        return 0;
    }
    return -1;
}

/**
 * @brief  仅向 ADS1115 发送寄存器指针（不带数据，用于后续的纯读取操作）
 * @param  reg_address: 寄存器指针地址
 * @param  arrayIndex:  兼容原 API 预留，此处不用
 * @param  length:      兼容原 API 预留，此处不用
 * @retval 0 成功; -1 失败
 */
int8_t sendI2CRegPointer(uint8_t reg_address, uint8_t *arrayIndex, uint8_t length)
{
    if (HAL_I2C_Master_Transmit(&hi2c1, ADS1115_I2C_ADDRESS, &reg_address, 1, 100) == HAL_OK)
    {
        return 0;
    }
    return -1;
}

/**
 * @brief  直接读取数据（可选择是否先发送寄存器地址）
 * @param  reg_address: 寄存器指针地址
 * @param  arrayIndex:  用于存放读取数据的数组指针
 * @param  length:      要读取的数据字节数
 * @param  noWrite:     true = 直接读取，不写寄存器地址; false = 先写地址再读
 * @retval 0 成功; -1 失败
 */
int8_t receiveI2CDataNoWrite(uint8_t reg_address, uint8_t *arrayIndex, uint8_t length, bool noWrite)
{
    if (noWrite)
    {
        // 如果不需要写地址，直接接收数据 (当前 ADS1115 指针停留在哪就读哪)
        if (HAL_I2C_Master_Receive(&hi2c1, ADS1115_I2C_ADDRESS, arrayIndex, length, 100) == HAL_OK)
        {
            return 0;
        }
    }
    else
    {
        // 如果需要写地址，复用 receiveI2CData
        return receiveI2CData(reg_address, arrayIndex, length);
    }
    return -1;
}

// ============================================================================
// 初始化及中断函数 (由于 STM32CubeMX 已处理，以下作精简处理)
// ============================================================================

/**
 * @brief 初始化外设
 * @note  在 STM32 中，CubeMX 生成的 MX_I2C1_Init() 和 MX_GPIO_Init() 已经完成了这些工作，此处留空。
 */
void InitADC(void) { }
void InitGPIO(void) { }
void InitI2C(void) { }

/**
 * @brief  获取 ALERT 中断标志位状态
 */
bool getALERTinterruptStatus(void) {
   return flag_nALERT_INTERRUPT;
}

/**
 * @brief  设置/清除 ALERT 中断标志位
 */
void setALERTinterruptStatus(const bool value) {
    flag_nALERT_INTERRUPT = value;
}

/**
 * @brief EXTI 外部中断回调函数 (由 HAL_GPIO_EXTI_Callback 调用)
 * @note 如果你不使用 ADS1115 的 ALERT 引脚中断，这个函数可以忽略。
 */
void GPIO_ALERT_IRQHandler(uint_least8_t index) {
    flag_nALERT_INTERRUPT = true;
}

/**
 * @brief 启用或禁用中断
 * @note 依赖 STM32 的 EXTI，如果不使用外部中断引脚，此函数可留空。
 */
void enableALERTinterrupt(const bool intEnable) {
    // 留空：如需通过代码控制中断开启/关闭，可通过 HAL_NVIC_EnableIRQ / DisableIRQ 实现
}

/**
 * @brief 阻塞等待 ALERT 引脚触发
 */
bool waitForALERTinterrupt(const uint32_t timeout_ms) {
    // 留空：由于你配置的是 860SPS 连续采样，通常直接按照时间周期（例如 delay 2ms）去读取即可，无需阻塞等待此引脚。
    return true;
}
