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
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_cmd_parser.h" // 引入指令解析器
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
	uint8_t g_rx_byte = 0; // 定义单字节接收缓存

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
	//非阻塞状态机变量
	static uint8_t adc_state = 0;
	static uint32_t adc_timer_base = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
	SystemCtrl_t g_SysCtrl = {0}; // 真正分配内存并初始化为0

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
  MX_DMA_Init();
  MX_I2C1_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */

  // 开启第一次串口接收中断，指定接收到 g_rx_byte 中，长度为 1
  HAL_UART_Receive_IT(&huart2, &g_rx_byte, 1);

  // 1. 读取 EEPROM，恢复上次保存的参数 (信道、相位等)
  #if ENABLE_MODULE_EEPROM
  Storage_Init_And_Load(&g_SysCtrl);
  #endif


  // 2. 将系统参数打入硬件 (这句会调用 Hardware_Set_TX_Freq 进行首次发令枪点火)
  Hardware_Apply_Params(&g_SysCtrl);

  adcStartup(); // 一开机，先初始化并复位 ADC
  //  记录时间状态机的零点
    adc_timer_base = HAL_GetTick();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
      {
          // --- 获取当前时间差 ---
          uint32_t current_time = HAL_GetTick();
          uint32_t elapsed_time = current_time - adc_timer_base;

          // --- 任务1：10ms 实时严格时序采样流 (非阻塞状态机) ---
          switch (adc_state) {
              case 0: // T = 0ms: 触发 I 通道采集
                  if (elapsed_time >= 10) {
                	  // 改为绝对时基累加，消除时间漂移
                	  adc_timer_base += 10;

                      // 【注意】这里必须改为非阻塞的启动函数，不能死等！
                      ADS1115_Start_Conversion(0); // 发送指令让 ADS1115 采通道0
                      adc_state = 1;
                  }
                  break;

              case 1: // T = 3ms: 读 I 通道，并触发 Q 通道采集
                  if (elapsed_time >= 3) {
                      g_SysCtrl.raw_I = ADS1115_Read_Result(); // 此时一定转换完了，直接读

                      ADS1115_Start_Conversion(1); // 立刻发指令采通道1
                      adc_state = 2;
                  }
                  break;

              case 2: // T = 6ms: 读 Q 通道，执行算法，并发送数据
                  if (elapsed_time >= 6) {
                      g_SysCtrl.raw_Q = ADS1115_Read_Result();

                      // --- 开始流水线数据处理 ---
                      #if ENABLE_MODULE_FILTER //滤波
                      SignalProcess_Filter(&g_SysCtrl);
                      #endif

                      SignalProcess_HandleAutoZero(&g_SysCtrl);//归0

                      #if ENABLE_MODULE_ROTATION //旋转坐标
                      SignalProcess_Rotate(&g_SysCtrl);
                      #endif

                      // --- 打包发送 ---
                      tx_packet.data_i = g_SysCtrl.processed_I;
                      tx_packet.data_q = g_SysCtrl.processed_Q;
                      tx_packet.checksum = Calc_Checksum((uint8_t *)&tx_packet, 8);
                      // 增加 DMA 忙碌状态保护
                      if (huart2.gState == HAL_UART_STATE_READY) {
						HAL_UART_Transmit_DMA(&huart2, (uint8_t *)&tx_packet, sizeof(Packet_t));
					}
                   //   HAL_UART_Transmit_DMA(&huart2, (uint8_t *)&tx_packet, sizeof(Packet_t));

                      // --- LED 状态指示 ---
                      Hardware_Update_LED(&g_SysCtrl);

                      adc_state = 0; // 状态归零，等待下一个 10ms 周期到来
                  }
                  break;
          }

          // --- 任务2：非实时指令解析 (后台任务，在 ADC 转换的空闲时间疯狂执行) ---
          if (CmdParser_HasNewCmd()) {
              CmdParser_Execute(&g_SysCtrl);
          }

          // --- 任务3：非实时掉电保存 (后台任务) ---
          #if ENABLE_MODULE_EEPROM
          if (g_SysCtrl.need_save_eeprom) {
              Storage_Save_Params(&g_SysCtrl);
              g_SysCtrl.need_save_eeprom = false;

            // 2. 【核心修复】由于时间轴已经发生了巨大的断层，必须强制重置状态机
			adc_timer_base = HAL_GetTick(); // 重新对齐零点
			adc_state = 0;                  // 强制回到初始状态
          }
          #endif
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

/**
  * @brief  串口接收完成回调函数 (由 HAL 库的中断处理函数自动调用)
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        // 1. 将接收到的这个字节，喂给我们的状态机解析器
        CmdParser_ReceiveByte(g_rx_byte);

        // 2. 必须再次调用此函数，重新开启接收中断，否则以后再也收不到数据了
        HAL_UART_Receive_IT(&huart2, &g_rx_byte, 1);
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
