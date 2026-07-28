/*
 * app_hardware.h
 *
 *  Created on: Jul 27, 2026
 *      Author: Administrator
 */

#ifndef INC_APP_HARDWARE_H_
#define INC_APP_HARDWARE_H_

#include "bsp_M24C64.h"

void Hardware_Set_TX_Freq(uint8_t channel);
void Hardware_Set_TX_PWM(uint8_t duty_percentage);
void Hardware_TX_Start(void);

void Hardware_Apply_Params(SystemCtrl_t *ctrl);
void Hardware_Update_LED(SystemCtrl_t *ctrl);
void Hardware_Set_PreGain(uint8_t gain_level);
void Storage_Save_Params(SystemCtrl_t *ctrl);
void Storage_Init_And_Load(SystemCtrl_t *ctrl) ;
#endif /* INC_APP_HARDWARE_H_ */
