/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// STM32G030 UID 的基地址
#define STM32_UID_BASE 0x1FFF7590
extern unsigned char Phy_Addr[6];
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
	uint8_t uart_rx_buf[64];
	uint8_t rx_idx = 0;
	uint8_t rx_byte;

// 强制 1 字节对齐，确保结构体在内存中紧凑排列，总共 10 字节
	#pragma pack(1)
	typedef struct {
		uint8_t  head1;    // 0x5A
		uint8_t  head2;    // 0xA5
		int16_t  data_i;   // I路数据 (16位ADC原始值)
		int16_t  data_q;   // Q路数据
		uint16_t reserved; // 预留位 (目前填0)
		uint8_t  checksum; // 校验和 (前8个字节的累加和)
		uint8_t  tail;     // 0x0D (帧尾)
	} Packet_t;
	#pragma pack()

	// 实例化数据帧，并赋初始固定值
	Packet_t tx_packet = {
		.head1 = 0x5A,
		.head2 = 0xA5,
		.reserved = 0x0000,
		.tail = 0x0D
	};

	// 定时器触发标志位
	volatile uint8_t adc_ready_flag = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void Process_Uart_Command(void);
void Print_Current_Config(void);
void Uart_Send_String(char *str);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// 串口发送字符串的辅助函数
void Uart_Send_String(char *str)
{
    // 使用阻塞方式发送字符串，直至发送完毕
    HAL_UART_Transmit(&huart2, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

// 打印当前所有网络配置信息的函数
void Print_Current_Config(void)
{
    char tx_buf[256]; // 用于组装字符串的缓冲区
    extern unsigned char Phy_Addr[6]; // 引入MAC地址变量

    Uart_Send_String("\r\n=================================\r\n");
    Uart_Send_String("      Device Network Config      \r\n");
    Uart_Send_String("=================================\r\n");

    // 打印 MAC 地址
    sprintf(tx_buf, "MAC Address  : %02X:%02X:%02X:%02X:%02X:%02X\r\n",
            Phy_Addr[0], Phy_Addr[1], Phy_Addr[2],
            Phy_Addr[3], Phy_Addr[4], Phy_Addr[5]);
    Uart_Send_String(tx_buf);

    // 打印 本机 IP
    sprintf(tx_buf, "Local IP     : %d.%d.%d.%d\r\n",
            DeviceConfig.ip[0], DeviceConfig.ip[1],
            DeviceConfig.ip[2], DeviceConfig.ip[3]);
    Uart_Send_String(tx_buf);

    // 打印 子网掩码
    sprintf(tx_buf, "Subnet Mask  : %d.%d.%d.%d\r\n",
            DeviceConfig.subnet[0], DeviceConfig.subnet[1],
            DeviceConfig.subnet[2], DeviceConfig.subnet[3]);
    Uart_Send_String(tx_buf);

    // 打印 网关
    sprintf(tx_buf, "Gateway      : %d.%d.%d.%d\r\n",
            DeviceConfig.gateway[0], DeviceConfig.gateway[1],
            DeviceConfig.gateway[2], DeviceConfig.gateway[3]);
    Uart_Send_String(tx_buf);

    // 打印 目标 IP
    sprintf(tx_buf, "Target IP    : %d.%d.%d.%d\r\n",
            DeviceConfig.target_ip[0], DeviceConfig.target_ip[1],
            DeviceConfig.target_ip[2], DeviceConfig.target_ip[3]);
    Uart_Send_String(tx_buf);

    // 打印 目标端口
    sprintf(tx_buf, "Target Port  : %d\r\n", DeviceConfig.target_port);
    Uart_Send_String(tx_buf);

    Uart_Send_String("=================================\r\n");
}

// 串口回调函数 (每次收到一个字节进一次)
	void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
	{
		if (huart->Instance == USART2)
		{
			if (rx_byte == '\n' || rx_byte == '\r')
			{
				uart_rx_buf[rx_idx] = '\0'; // 字符串结束符
				if (rx_idx > 0)
				{
					Process_Uart_Command(); // 解析命令
				}
				rx_idx = 0; // 清空接收索引
			}
			else
			{
				if (rx_idx < 63) uart_rx_buf[rx_idx++] = rx_byte;
			}
			// 重新开启接收中断
			HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
		}
	}

	// 串口指令解析执行
	/*串口接收到指令进行网络参数的设置，结尾含\r\n
	 *串口解析示例
	 * SETIP:192.168.1.150   功能：把 STM32 自己的 IP 改成 192.168.1.150
	 * SETDIP:192.168.1.200  功能：把接收数据的电脑/服务器的 IP 改成 192.168.1.200
	 * SETMASK:255.255.255.0 功能：修改子网掩码（通常不需要改，除非跨越大型企业内网）
	 * SETGW:192.168.1.1	 功能：修改网关地址（路由器地址）
	 * SETPORT:8080			 功能：把发往电脑的 UDP 目标端口改成 8080
	 * RESET				 功能：恢复默认参数并重启
	 * GETCONFIG			 功能：回复当前网络配置状态
	 * */
	void Process_Uart_Command(void)
	{
	    int ip1, ip2, ip3, ip4;
	    int port;

	    // 1. 解析修改本机 IP 指令
	    if (sscanf((char *)uart_rx_buf, "SETIP:%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) == 4)
	    {
	        DeviceConfig.ip[0] = ip1; DeviceConfig.ip[1] = ip2;
	        DeviceConfig.ip[2] = ip3; DeviceConfig.ip[3] = ip4;
	        Config_Save();         // 存入 Flash
	        NVIC_SystemReset();    // 立即重启生效
	    }
	    // 2. 解析修改目标上位机 IP 指令 (Destination IP)
	    else if (sscanf((char *)uart_rx_buf, "SETDIP:%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) == 4)
	    {
	        DeviceConfig.target_ip[0] = ip1; DeviceConfig.target_ip[1] = ip2;
	        DeviceConfig.target_ip[2] = ip3; DeviceConfig.target_ip[3] = ip4;
	        Config_Save();
	        NVIC_SystemReset();
	    }
	    // 3. 解析修改子网掩码指令 (Subnet Mask)
	    else if (sscanf((char *)uart_rx_buf, "SETMASK:%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) == 4)
	    {
	        DeviceConfig.subnet[0] = ip1; DeviceConfig.subnet[1] = ip2;
	        DeviceConfig.subnet[2] = ip3; DeviceConfig.subnet[3] = ip4;
	        Config_Save();
	        NVIC_SystemReset();
	    }
	    // 4. 解析修改网关指令 (Gateway)
	    else if (sscanf((char *)uart_rx_buf, "SETGW:%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) == 4)
	    {
	        DeviceConfig.gateway[0] = ip1; DeviceConfig.gateway[1] = ip2;
	        DeviceConfig.gateway[2] = ip3; DeviceConfig.gateway[3] = ip4;
	        Config_Save();
	        NVIC_SystemReset();
	    }
	    // 5. 解析修改目标上位机端口指令
	    else if (sscanf((char *)uart_rx_buf, "SETPORT:%d", &port) == 1)
	    {
	        DeviceConfig.target_port = port;
	        Config_Save();
	        NVIC_SystemReset();
	    }
	    // 6. 恢复出厂设置指令 (重置为代码中写入的默认值)
	    else if (strncmp((char *)uart_rx_buf, "RESET", 5) == 0)
	    {
	        DeviceConfig.head = 0xFFFF; // 破坏标志位
	        Config_Save();              // 存入损坏的标志位
	        NVIC_SystemReset();         // 重启后 Config_Init 发现标志位不对，会自动重新加载默认值
	    }
	    // 7. 新增：解析查询配置指令
		else if (strncmp((char *)uart_rx_buf, "GETCONFIG", 9) == 0)
		{
			Print_Current_Config(); // 调用刚才写的打印函数
		}
		// 8. 容错处理：如果输入了不认识的指令
		else
		{
			Uart_Send_String("ERROR: Unknown Command!\r\n");
		}
	}
/*根据芯片UID，动态生成唯一MAC地址*/
void Generate_MAC_From_UID(void)
{
    // 获取芯片 UID 的前 32 位
    uint32_t uid = *(uint32_t *)STM32_UID_BASE;

    // 前 3 个字节固定 (类似于厂商 OUI)
    Phy_Addr[0] = 0x00;
    Phy_Addr[1] = 0x08;
    Phy_Addr[2] = 0xDC;

    // 后 3 个字节使用 UID 动态生成，保证同网段不冲突
    Phy_Addr[3] = (uid >> 16) & 0xFF;
    Phy_Addr[4] = (uid >> 8) & 0xFF;
    Phy_Addr[5] = uid & 0xFF;
}

/**
  * @brief  计算 8 位累加校验和
  * @param  data: 数据缓冲区的起始指针
  * @param  len:  参与计算的字节长度
  * @retval 8位无符号校验和
  */
uint8_t Calc_Checksum(uint8_t *data, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++)
    {
        sum += data[i];
    }
    return sum;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  // 1. 开启串口接收中断
   HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
   // 2. 初始化网络参数 (从 Flash 读，如果没有则赋默认值并存 Flash)
   Config_Init();
   // 3. 将 Flash 中的参数覆盖到 W5500 的全局变量中 (这些变量在 bsp_w5500.c 定义)
	extern unsigned char IP_Addr[4];
    extern unsigned char Sub_Mask[4];
	extern unsigned char Gateway_IP[4];
	extern unsigned char S0_DIP[4];
	extern unsigned char S0_DPort[2];
	memcpy(IP_Addr, DeviceConfig.ip, 4);
    memcpy(Sub_Mask, DeviceConfig.subnet, 4);
    memcpy(Gateway_IP, DeviceConfig.gateway, 4);
    memcpy(S0_DIP, DeviceConfig.target_ip, 4);

    S0_DPort[0] = DeviceConfig.target_port >> 8;
    S0_DPort[1] = DeviceConfig.target_port & 0xFF;
   // 4. 动态生成独一无二的 MAC 地址
    Generate_MAC_From_UID();
   // 5. 初始化 W5500 硬件
   W5500_Hardware_Reset(); // 硬件复位 W5500
   W5500_Init();           // 初始化 MAC、IP、网关等基础参数
   Socket_Init(0);         // 初始化 Socket 0
   Socket_UDP(0);          // 将 Socket 0 配置为 UDP 模式并打开监听
   // 6. 启动 ADC 采样与定时器
   adcStartup(); // 一开机，先初始化并复位 ADC
   HAL_TIM_Base_Start_IT(&htim3); // 启动 TIM3 (10ms 周期)


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	// 定义状态机用的计时器变量
	 uint16_t led_timer = 0;
	 uint16_t btn_press_timer = 0;
  while (1)
  {
	  if (adc_ready_flag == 1)
	{
		adc_ready_flag = 0;

		// 1. 读取双通道 ADC 数据 (耗时约 6ms)
		tx_packet.data_i = readChannelData(0);
		tx_packet.data_q = readChannelData(1);

		// 2. 计算校验和
		// 结构体总长 10 字节，我们计算前 8 个字节 (从 head1 到 reserved) 的累加和
		// 也可以用 offsetof(Packet_t, checksum) 代替硬编码的 8
		tx_packet.checksum = Calc_Checksum((uint8_t *)&tx_packet, 8);

		// 3. UDP 网络发送逻辑 (自带断线重连保护)
	  // 读取 Socket 0 的状态寄存器，确保它当前处于 UDP 正常工作模式
		  if (Read_W5500_1Byte(Sn_SR) != SOCK_UDP)
		  {
			  // 如果网线被拔掉后重新插上，或者模块发生异常状态，重新打开 UDP 模式
			  Socket_UDP(0);
		  }
		  else
		  {
			  // 状态正常，直接将 10 字节的结构体数据通过 UDP 发送给上位机
			  Write_SOCK_Data_Buffer(0, (uint8_t *)&tx_packet, sizeof(Packet_t));
		  }

		  //  指示灯心跳逻辑 (利用 10ms 周期计数)
		  led_timer++;
		  if (led_timer >= 50) // 50 * 10ms = 500ms (半秒闪烁一次)
		  {
			led_timer = 0;
			// 1. 系统心跳：翻转运行指示灯 (如绿灯)
			HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin); // 翻转 LED 状态

			// 2. 网络诊断：读取 W5500 物理层连接状态 (LINK 位)
		  // PHYCFGR 寄存器的最低位 (bit 0) 是物理连接标志：1 为已连接，0 为断开
		  uint8_t phy_status = Read_W5500_1Byte(PHYCFGR);

		  if ((phy_status & LINK) == 0)
		  {
			  // 异常情况 A：网线被拔出或交换机断电
			  // 点亮异常指示灯 (红灯常亮)
			  HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
		  }
		  else
		  {
			  // 网线物理连接正常，进一步检查 Socket 状态
			  if (Read_W5500_1Byte(Sn_SR) != SOCK_UDP)
			  {
				  // 异常情况 B：网线插着，但协议栈死机或未处于 UDP 模式
				  HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
			  }
			  else
			  {
				  // 一切正常：熄灭异常指示灯
				  HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
			  }
		  }
		  }

		  // 物理按键“长按 3 秒恢复出厂设置”逻辑

		  // 检测按键是否按下 (假设按下是低电平 RESET)
			if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_RESET)
			{
				btn_press_timer++; // 只要按住，每 10ms 加 1

				if (btn_press_timer >= 300) // 300 * 10ms = 3000ms = 3秒
				{
					// 1. 破坏 Flash 里的标志位，强制恢复默认参数
					DeviceConfig.head = 0xFFFF;
					Config_Save();

					// 2. 用 LED 快速闪烁 10 次，给用户“重置成功”的视觉反馈
					for(int i = 0; i < 20; i++)
					{
						HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
						HAL_Delay(50);
					}

					// 3. 立即重启单片机
					NVIC_SystemReset();
				}
			}
			else
			{
				// 如果中途松手了，计时器立刻清零，防止累加误触发
				btn_press_timer = 0;
			}

	}


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {//10ms采样一次
        adc_ready_flag = 1;
    }
}


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
