/*
 * app_signal_process.c
 *
 *  Created on: Jul 16, 2026
 *      Author: Administrator
 */


#include "app_signal_process.h"

// Auto-Zero 状态缓存
static uint8_t az_running = 0;
static uint8_t az_counter = 0;
static int32_t az_sum_I = 0;
static int32_t az_sum_Q = 0;

// 0~90度 Q15 正弦查表 (91个点，放大 32768 倍)
const int16_t SIN_TAB_Q15[91] = {
    0, 572, 1144, 1715, 2286, 2856, 3425, 3993, 4560, 5126, 5690, 6252, 6813, 7371, 7927,
    8481, 9032, 9580, 10126, 10668, 11207, 11743, 12275, 12803, 13328, 13848, 14364, 14876,
    15383, 15886, 16383, 16876, 17364, 17846, 18323, 18794, 19260, 19720, 20173, 20621, 21062,
    21497, 21925, 22347, 22762, 23170, 23571, 23964, 24351, 24730, 25101, 25465, 25821, 26169,
    26509, 26841, 27165, 27481, 27788, 28087, 28377, 28659, 28932, 29196, 29451, 29697, 29934,
    30163, 30381, 30591, 30791, 30982, 31163, 31335, 31498, 31650, 31794, 31927, 32051, 32165,
    32269, 32364, 32448, 32523, 32587, 32642, 32687, 32722, 32747, 32763, 32767
};

// ============================================================================
// 一阶 EMA (指数移动平均) 低通滤波器参数
// ============================================================================
// 为了完全不消耗 Flash 的浮点库，我们采用纯整数移位算法
// ALPHA_SHIFT = 2 相当于 Alpha = 0.25 (1/4)。
// 截断频率约为: Fs * Alpha / (2 * PI) ≈ 100 * 0.25 / 6.28 ≈ 4 Hz
#define EMA_ALPHA_SHIFT  2
static int32_t s_ema_I = 0;
static int32_t s_ema_Q = 0;
static uint8_t s_ema_first_run = 1; // 第一次运行标志，用于快速收敛

// ============================================================================
// 二阶 Biquad IIR 带通滤波器参数 (由 MATLAB 脚本自动生成)
// ============================================================================
// 把你在 MATLAB 控制台打印出来的宏定义直接粘贴在这里
// 例如 (具体数值以你的 MATLAB 运行结果为准)：（中心5Hz，带宽4Hz，3Hz-7Hz）
#define SHIFT_BITS 15
#define B0  3675
#define B1  0
#define B2  -3675
#define A1  -55777
#define A2  25417

// Biquad 历史状态缓存 (因为是 Q15，输入和输出都可以用 16位整数存储)
typedef struct {
    int16_t x1, x2;
    int16_t y1, y2;
} BiquadState_t;

static BiquadState_t s_bq_I = {0};
static BiquadState_t s_bq_Q = {0};

// ============================================================================
// 内部辅助函数
// ============================================================================
/**
 * @brief 根据角度获取 Q15 正弦值 (支持 0~359度)
 */
static int16_t Math_SinQ15(int16_t angle) {
    angle = (angle % 360 + 360) % 360; // 确保在 0~359 范围内
    if (angle <= 90) return SIN_TAB_Q15[angle];
    if (angle <= 180) return SIN_TAB_Q15[180 - angle];
    if (angle <= 270) return -SIN_TAB_Q15[angle - 180];
    return -SIN_TAB_Q15[360 - angle];
}

/**
 * @brief 根据角度获取 Q15 余弦值 (利用相差 90度特性)
 */
static int16_t Math_CosQ15(int16_t angle) {
    return Math_SinQ15(angle + 90);
}

/**
 * @brief 执行单步 Biquad Q15 定点化滤波运算
 */
static int16_t Process_Biquad(int16_t input, BiquadState_t *state) {
    // 【核心精髓】：中间累加器必须使用 int32_t，因为 16位 * 16位 会达到 32位，防止溢出！
    int32_t acc = (B0 * input) + (B1 * state->x1) + (B2 * state->x2)
                - (A1 * state->y1) - (A2 * state->y2);

    // 缩放还原：将累加结果右移 15 位
    int16_t output = (int16_t)(acc >> SHIFT_BITS);

    // 更新历史状态 (延时线)
    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = output;

    return output;
}
// ============================================================================
// 对外接口函数
// ============================================================================

void SignalProcess_ClearFilterHistory(void) {
    s_ema_first_run = 1;
    s_bq_I.x1 = 0; s_bq_I.x2 = 0; s_bq_I.y1 = 0; s_bq_I.y2 = 0;
    s_bq_Q.x1 = 0; s_bq_Q.x2 = 0; s_bq_Q.y1 = 0; s_bq_Q.y2 = 0;
}

void SignalProcess_Filter(SystemCtrl_t *ctrl) {
	// 0 档: 无滤波，直接透传原数据
    if (ctrl->filter_type == 0) {
        ctrl->processed_I = ctrl->raw_I;
        ctrl->processed_Q = ctrl->raw_Q;
    }// 1 档: EMA 一阶低通滤波 (平滑去毛刺，适合抓取静态/慢速变化的信号)
    else if (ctrl->filter_type == 1) {
        if (s_ema_first_run) {
            s_ema_I = ctrl->raw_I;// 第一次采样的值直接作为基准，避免从 0 缓慢爬升
            s_ema_Q = ctrl->raw_Q;
            s_ema_first_run = 0;
        } else {
        	// 纯整数位移滤波算法: Y = Y + (X - Y) / 4
            s_ema_I += ((int32_t)ctrl->raw_I - s_ema_I) >> EMA_ALPHA_SHIFT;
            s_ema_Q += ((int32_t)ctrl->raw_Q - s_ema_Q) >> EMA_ALPHA_SHIFT;
        }
        ctrl->processed_I = (int16_t)s_ema_I;
        ctrl->processed_Q = (int16_t)s_ema_Q;
    }// 2 档: Biquad 二阶带通滤波 提取特定频段的特征频率
    else if (ctrl->filter_type == 2) {
        // 直接传入 16位的 raw 数据，不再强转 float
        ctrl->processed_I = Process_Biquad(ctrl->raw_I, &s_bq_I);
        ctrl->processed_Q = Process_Biquad(ctrl->raw_Q, &s_bq_Q);
    }
}

/**
 * @brief 坐标系相位旋转
 */
void SignalProcess_Rotate(SystemCtrl_t *ctrl) {
    if (ctrl->phase_offset == 0) return; // 0度不旋转，节省算力

    int16_t cos_val = Math_CosQ15(ctrl->phase_offset);
    int16_t sin_val = Math_SinQ15(ctrl->phase_offset);

    // 中间变量必须强制转为 int32_t，防止 16位乘16位 溢出
    // 旋转公式:
    // I' = I*cos - Q*sin
    // Q' = I*sin + Q*cos
    int32_t temp_I = ((int32_t)ctrl->processed_I * cos_val - (int32_t)ctrl->processed_Q * sin_val) >> 15;
    int32_t temp_Q = ((int32_t)ctrl->processed_I * sin_val + (int32_t)ctrl->processed_Q * cos_val) >> 15;

    ctrl->processed_I = (int16_t)temp_I;
    ctrl->processed_Q = (int16_t)temp_Q;
}
/**
 * @brief 自动相位归零处理机 (在 Filter 之后，Rotate 之前调用)
 */
void SignalProcess_HandleAutoZero(SystemCtrl_t *ctrl) {
    // 1. 收到指令，触发采集
    if (ctrl->req_auto_zero) {
        ctrl->req_auto_zero = false;
        az_running = 1;
        az_counter = 0;
        az_sum_I = 0;
        az_sum_Q = 0;
    }

    // 2. 收集 32 个点进行平均化 (对应 0.32 秒的数据，极度平滑)
    if (az_running) {
        az_sum_I += ctrl->processed_I;
        az_sum_Q += ctrl->processed_Q;
        az_counter++;

        // 收集完毕，开始计算最佳相位
        if (az_counter >= 32) {
            az_running = 0;
            int32_t avg_I = az_sum_I / 32;
            int32_t avg_Q = az_sum_Q / 32;

            int32_t max_I = -2147483647; // 找寻最大值的初始锚点
            int16_t best_angle = 0;

            // 黑科技：不用 atan2，直接扫表找最大投影角度。
            // 目标是让旋转后的 I' 最大（这就意味着向量被完全拍平到了 X 轴上）
            for (int16_t ang = 0; ang < 360; ang++) {
                int16_t c = Math_CosQ15(ang);
                int16_t s = Math_SinQ15(ang);
                int32_t rot_I = (avg_I * c - avg_Q * s) >> 15;

                if (rot_I > max_I) {
                    max_I = rot_I;
                    best_angle = ang;
                }
            }

            // 设定最新算出的相位角，并触发 EEPROM 保存
            ctrl->phase_offset = best_angle;
            // 2. 记录皮重 (旋转后的背景基线)
            ctrl->baseline_I = max_I;
            ctrl->need_save_eeprom = true;
        }
    }
}
