/*
 * flash_config.c
 *
 *  Created on: Jun 23, 2026
 *      Author: Administrator
 */
#include "flash_config.h"
#include <string.h>

NetConfig_t DeviceConfig;

// 从 Flash 加载配置
void Config_Init(void)
{
    NetConfig_t *flash_ptr = (NetConfig_t *)CONFIG_FLASH_ADDR;

    // 检查 Flash 中是否有我们的标识符 0x55AA
    if (flash_ptr->head == CONFIG_FLAG)
    {
        // 存在配置，直接读取到内存
        memcpy(&DeviceConfig, flash_ptr, sizeof(NetConfig_t));
    }
    else
    {
        // 如果是新板子，加载默认参数
        DeviceConfig.head = CONFIG_FLAG;

        uint8_t default_ip[4]      = {192, 168, 1, 111};
        uint8_t default_subnet[4]  = {255, 255, 255, 0};
        uint8_t default_gateway[4] = {192, 168, 1, 1};
        uint8_t default_target_ip[4] = {192, 168, 1, 100};

        memcpy(DeviceConfig.ip, default_ip, 4);
        memcpy(DeviceConfig.subnet, default_subnet, 4);
        memcpy(DeviceConfig.gateway, default_gateway, 4);
        memcpy(DeviceConfig.target_ip, default_target_ip, 4);
        DeviceConfig.target_port = 6000;

        // 第一次开机，自动将默认参数写入 Flash
        Config_Save();
    }
}

// 保存配置到 Flash
void Config_Save(void)
{
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;

    HAL_FLASH_Unlock();

    // 1. 擦除 Page 15
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.Page = 15;
    EraseInitStruct.NbPages = 1;
    HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

    // 2. 将结构体以 64-bit (双字) 格式写入 Flash
    uint64_t *data_ptr = (uint64_t *)&DeviceConfig;
    uint32_t words_to_write = sizeof(NetConfig_t) / 8; // 24 / 8 = 3 次写入

    for (uint32_t i = 0; i < words_to_write; i++)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, CONFIG_FLASH_ADDR + (i * 8), data_ptr[i]);
    }

    HAL_FLASH_Lock();
}

