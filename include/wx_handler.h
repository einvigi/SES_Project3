/*
 * wx_handler.h
 *
 *  Created on: 26.01.2019
 *      Author: mateusz
 */

#ifndef WX_HANDLER_H_
#define WX_HANDLER_H_

#include <stdint.h>
#include "config_data.h"

extern uint32_t wx_last_good_temperature_time;
extern uint32_t wx_last_good_wind_time;

typedef enum wx_pwr_state_t {
	WX_PWR_OFF,
	WX_PWR_ON,
	WX_PWR_UNDER_RESET,
	WX_PWR_DISABLED
}wx_pwr_state_t;

void wx_check_force_i2c_reset(void);
void wx_get_all_measurements(void);
void wx_pool_dht22(void);
void wx_pool_anemometer(void);
void wx_pwr_init(void);
void wx_pwr_periodic_handle(void);
//void wx_pwr_disable_12v_sw(void);
//void wx_pwr_disable_5v_isol(void);
//void wx_pwr_enable_12v_sw(void);
//void wx_pwr_enable_5v_isol(void);


#endif /* WX_HANDLER_H_ */
