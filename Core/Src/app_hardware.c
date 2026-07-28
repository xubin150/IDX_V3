#include "tim.h"
#include "dma.h" // 包含 DMA 头文件
#include <stdlib.h> // 为了使用 abs() 求绝对值
#include "app_hardware.h"

#define EEPROM_BASE_ADDR   0x0000
#define EEPROM_MAGIC_WORD  0xAA55  // 用于校验 EEPROM 数据是否有效

// 引入在 tim.c 中定义的 DMA 句柄
extern DMA_HandleTypeDef hdma_tim3_up;
// 引入全局状态，以便获取当前的占空比设定 (需确保 app_hardware.h 包含了 app_system.h)
extern SystemCtrl_t g_SysCtrl;
// =================================================================
// 核心：预先定义 0, 90, 180, 270 度的 GPIO 状态 (写入 BSRR)
// BSRR 寄存器：低 16 位写 1 是拉高 (SET)，高 16 位写 1 是拉低 (RESET)
// PA0 (CON_X) -> SET 位是 bit 0，RESET 位是 bit 16
// PA1 (CON_Y) -> SET 位是 bit 1，RESET 位是 bit 17
// =================================================================
const uint32_t CON_XY_STATES[4] = {
    // 数组排列顺序从 90度 开始，因为 TIM3 启动 1/4 周期后触发第一次 DMA
    // 90度时:  CON_X 高, CON_Y 高
    (1 << 0) | (1 << 1),

    // 180度时: CON_X 低, CON_Y 高
    (1 << 16)| (1 << 1),

    // 270度时: CON_X 低, CON_Y 低
    (1 << 16)| (1 << 17),

    // 0/360度: CON_X 高, CON_Y 低
    (1 << 0) | (1 << 17)
};

// 定义 5 个抗干扰信道的频率，必须保证 (TIM1_ARR+1) = 4 * (TIM3_ARR+1)
typedef struct {
    uint32_t tim1_arr;
    uint32_t tim3_arr;
} ChannelConfig_t;

const ChannelConfig_t TX_CHANNELS[5] = {
    {939, 234},  // CH1: 68.08 kHz
    {927, 231},  // CH2: 68.96 kHz
    {911, 227},  // CH3: 70.17 kHz (默认)
    {899, 224},  // CH4: 71.11 kHz
    {887, 221}   // CH5: 72.07 kHz
};

static uint8_t s_current_ch_index = 2; // 默认 CH3

/**
 * @brief 动态设置 TX 频率并重建锁相环
 */
void Hardware_Set_TX_Freq(uint8_t channel) {
    if (channel < 1 || channel > 5) return;
    s_current_ch_index = channel - 1;

    uint32_t arr1 = TX_CHANNELS[s_current_ch_index].tim1_arr;
    uint32_t arr3 = TX_CHANNELS[s_current_ch_index].tim3_arr;

    // 1. 关停所有设备
    __HAL_TIM_DISABLE(&htim1);
    __HAL_TIM_DISABLE(&htim3);
    HAL_DMA_Abort(&hdma_tim3_up); // 停止 DMA

    // 2. 更新两者的周期
    __HAL_TIM_SET_AUTORELOAD(&htim1, arr1);
    __HAL_TIM_SET_AUTORELOAD(&htim3, arr3);

    // 【修复新增】同步更新一次 CCR，保证切换频率时功率不发生突变
	uint8_t current_duty = g_SysCtrl.tx_pwm_duty == 0 ? 50 : g_SysCtrl.tx_pwm_duty; // 默认 50%
	uint32_t new_ccr = (arr1 + 1) * current_duty / 100;
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, new_ccr);

    // 3. 强制重置计数值，准备同步起跑
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    __HAL_TIM_SET_COUNTER(&htim3, 0);

    // 4. 手动设置 0度 时的初始状态: CON_X=1, CON_Y=0
    GPIOA->BSRR = (1 << 0) | (1 << 17);

    // 5. 启动 DMA 循环搬运
    //    源地址: 状态数组的首地址
    //    目的地址: GPIOA->BSRR 寄存器的硬件地址
    HAL_DMA_Start(&hdma_tim3_up, (uint32_t)CON_XY_STATES, (uint32_t)&GPIOA->BSRR, 4);

    // 6. 允许 TIM3 产生 DMA 请求
    __HAL_TIM_ENABLE_DMA(&htim3, TIM_DMA_UPDATE);


  /*  // 7. 发令枪响：双双启动 ！
    __HAL_TIM_ENABLE(&htim3);
    __HAL_TIM_ENABLE(&htim1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1); // 启动 PA8 的 TX*/
    // 7. 发令枪响：纯硬件同步启动！
	// 注意：因为 TIM3 处于 Trigger Mode，它现在是“上膛”状态，不会自己跑。
	// 先把 TIM1 的 PWM 通道连通到 PA8 管脚 (此时还没有波形，因为 TIM1 没启动)
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

	// 最后一步：启动 TIM1！
	// 就在 TIM1 启动的同一个纳秒，硬件 TRGO 信号会瞬间击发 TIM3，完美同步！
//	__HAL_TIM_ENABLE(&htim1);
}

/**
 * @brief 设置 TX 功率
 */
void Hardware_Set_TX_PWM(uint8_t duty_percentage) {
    if (duty_percentage < 10) duty_percentage = 10;
    if (duty_percentage > 50) duty_percentage = 50;

    uint32_t current_arr = TX_CHANNELS[s_current_ch_index].tim1_arr;
    uint32_t new_ccr = (current_arr + 1) * duty_percentage / 100;

    // 只改变 PA8 的占空比，完全不影响 DMA 生成的 0~180度方波
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, new_ccr);
}

void Hardware_TX_Start(void) {
//    Hardware_Set_TX_Freq(3);
}


// 占位函数：未来用来配置 PGA 和 频率
void Hardware_Apply_Params(SystemCtrl_t *ctrl) {
// 假设未初始化的结构体默认值为 0，给个合理的默认启动值
	if (ctrl->tx_channel < 1 || ctrl->tx_channel > 5) ctrl->tx_channel = 3;
	if (ctrl->tx_pwm_duty < 10 || ctrl->tx_pwm_duty > 50) ctrl->tx_pwm_duty = 50;

	// 1. 开机配置前置放大器
	   Hardware_Set_PreGain(ctrl->pre_gain);
	// 2. 开机点火,启动频率和 PWM
	Hardware_Set_TX_Freq(ctrl->tx_channel);
	// 开机默认 LED 处于 0 号心跳模式
	if (ctrl->led_mode > 3) ctrl->led_mode = 0;

	// 如果开机时档位越界 (比如第一次烧录EEPROM全为0xFF)，默认给第 5 档
	    if (ctrl->threshold_level > 9) {
	        ctrl->threshold_level = 5;
	    }
    // TODO
}

void Hardware_Update_LED(SystemCtrl_t *ctrl) {
    // 记录上一次心跳翻转的时间
    static uint32_t last_heartbeat_tick = 0;
    uint32_t current_tick = HAL_GetTick();

// 档位索引:                     0,    1,    2,    3,    4,    5,    6,   7,   8,   9
	// 数值越小，只要一点点偏差就会报警 (即灵敏度越高)
	const uint16_t THRESHOLD_TABLE[10] = {5000, 4000, 3000, 2500, 2000, 1500, 1000, 800, 500, 200};

    switch (ctrl->led_mode) {
        case 0: // 模式 0：心跳指示 (默认) - 每 500ms 翻转一次，证明单片机活着
            if (current_tick - last_heartbeat_tick >= 500) {
                last_heartbeat_tick = current_tick;
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_12);
            }
            break;
        case 1: // 目标检测指示 (仅关注 I 轴能量衰减)
                {
                    // 算差值
                    int32_t delta_I = abs(ctrl->processed_I - ctrl->baseline_I);

                    // 查表获取当前的真实阈值
                    uint16_t active_threshold = THRESHOLD_TABLE[ctrl->threshold_level];

                    // 判断是否报警
                    if (delta_I > active_threshold) {
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);   // 亮
                    } else {
                        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET); // 灭
                    }
                    break;
                }
            break;

        case 2: // 模式 2：常灭 (强制关闭)
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
            break;

        case 3: // 模式 3：常亮 (强制打开)
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
            break;

        default: // 防止串口收到异常参数导致状态机跑飞
            ctrl->led_mode = 0;
            break;
    }
}

/**
 * @brief  设置前端放大器增益档位 (使用单片选通逻辑)
 * @param  gain_level: 1 档 ~ 4 档
 */
void Hardware_Set_PreGain(uint8_t gain_level) {
    // 边界安全防护
    if (gain_level < 1 || gain_level > 4) {
        return;
    }

    // 1. 定义要清除的位掩码 (把 PA4~PA7 全部关断，写入 BSRR 的高 16 位)
    // GPIO_PIN_4 是 (1<<4)，左移 16 位即为 Reset 动作
    uint32_t reset_mask = (GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7) << 16;

    // 2. 定义要置位的位掩码 (写入 BSRR 的低 16 位)
    uint32_t set_mask = 0;
    switch (gain_level) {
        case 1: set_mask = GPIO_PIN_4; break; // 导通 R_x1
        case 2: set_mask = GPIO_PIN_5; break; // 导通 R_x2
        case 3: set_mask = GPIO_PIN_6; break; // 导通 R_x3
        case 4: set_mask = GPIO_PIN_7; break; // 导通 R_x4
    }

    // 3. 一步到位：执行原子级并发写入
    // 硬件会先执行 Reset 清除，再执行 Set 置位，完全无缝切换
    GPIOA->BSRR = reset_mask | set_mask;
}
/**
 * @brief 将关键参数保存至 EEPROM
 */
void Storage_Save_Params(SystemCtrl_t *ctrl) {
    uint16_t addr = EEPROM_BASE_ADDR;

    // 1. 写入魔法字 (标记这是一份合法数据)
    M24C64_WriteByte(addr++, (EEPROM_MAGIC_WORD >> 8) & 0xFF);
    M24C64_WriteByte(addr++, EEPROM_MAGIC_WORD & 0xFF);

    // 2. 写入单字节参数
    M24C64_WriteByte(addr++, ctrl->tx_channel);
    M24C64_WriteByte(addr++, ctrl->tx_pwm_duty);
    M24C64_WriteByte(addr++, ctrl->pre_gain);
    M24C64_WriteByte(addr++, ctrl->threshold_level);
    M24C64_WriteByte(addr++, ctrl->led_mode);

    // 3. 写入 16位 参数 (相位角)
    M24C64_WriteByte(addr++, (ctrl->phase_offset >> 8) & 0xFF);
    M24C64_WriteByte(addr++, ctrl->phase_offset & 0xFF);

    // 4. 写入 32位 参数 (皮重基线)
    M24C64_WriteByte(addr++, (ctrl->baseline_I >> 24) & 0xFF);
    M24C64_WriteByte(addr++, (ctrl->baseline_I >> 16) & 0xFF);
    M24C64_WriteByte(addr++, (ctrl->baseline_I >> 8) & 0xFF);
    M24C64_WriteByte(addr++, ctrl->baseline_I & 0xFF);

    // 标记已保存
    ctrl->need_save_eeprom = false;
}

/**
 * @brief 开机初始化时从 EEPROM 加载参数
 */
void Storage_Init_And_Load(SystemCtrl_t *ctrl) {
    uint16_t addr = EEPROM_BASE_ADDR;

    // 读取魔法字
    uint8_t magic_h = M24C64_ReadByte(addr++);
    uint8_t magic_l = M24C64_ReadByte(addr++);
    uint16_t magic = (magic_h << 8) | magic_l;

    if (magic == EEPROM_MAGIC_WORD) {
        // 如果校验通过，依次加载保存的参数 (顺序必须与 Save 一致)
        ctrl->tx_channel      = M24C64_ReadByte(addr++);
        ctrl->tx_pwm_duty     = M24C64_ReadByte(addr++);
        ctrl->pre_gain        = M24C64_ReadByte(addr++);
        ctrl->threshold_level = M24C64_ReadByte(addr++);
        ctrl->led_mode        = M24C64_ReadByte(addr++);

        uint8_t ph_h = M24C64_ReadByte(addr++);
        uint8_t ph_l = M24C64_ReadByte(addr++);
        ctrl->phase_offset = (int16_t)((ph_h << 8) | ph_l);

        uint8_t bl_3 = M24C64_ReadByte(addr++);
        uint8_t bl_2 = M24C64_ReadByte(addr++);
        uint8_t bl_1 = M24C64_ReadByte(addr++);
        uint8_t bl_0 = M24C64_ReadByte(addr++);
        ctrl->baseline_I = (int32_t)((bl_3 << 24) | (bl_2 << 16) | (bl_1 << 8) | bl_0);

    } else {
        // 【首次开机】或 EEPROM 损坏：赋予安全默认值
        ctrl->tx_channel = 3;
        ctrl->tx_pwm_duty = 50;
        ctrl->pre_gain = 1;
        ctrl->threshold_level = 5;
        ctrl->led_mode = 0;
        ctrl->phase_offset = 0;
        ctrl->baseline_I = 0;

        // 立即执行一次保存，打上魔法字标签
        Storage_Save_Params(ctrl);
    }
}
