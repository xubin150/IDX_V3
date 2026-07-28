/*
 * app_signal_process.h
 *
 *  Created on: Jul 16, 2026
 *      Author: Administrator
 */

#ifndef INC_APP_SIGNAL_PROCESS_H_
#define INC_APP_SIGNAL_PROCESS_H_
#include "app_system.h"
#include <stdint.h>
// 清除历史状态 (切换滤波算法时调用)
void SignalProcess_ClearFilterHistory(void);

// 核心滤波执行函数
void SignalProcess_Filter(SystemCtrl_t *ctrl);
void SignalProcess_Rotate(SystemCtrl_t *ctrl);
void SignalProcess_HandleAutoZero(SystemCtrl_t *ctrl);
#endif /* INC_APP_SIGNAL_PROCESS_H_ */
