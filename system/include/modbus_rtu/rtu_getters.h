/*
 * rtu_getters.h
 *
 *  Created on: 29.09.2020
 *      Author: mateusz
 */

#ifndef INCLUDE_MODBUS_RTU_RTU_GETTERS_H_
#define INCLUDE_MODBUS_RTU_RTU_GETTERS_H_

#include <stdint.h>

int32_t rtu_get_temperature(float* out);
int32_t rtu_get_pressure(float* out);
int32_t rtu_get_wind_direction(uint16_t* out);
int32_t rtu_get_wind_speed(uint16_t* out);
int32_t rtu_get_wind_gusts(uint16_t* out);
int32_t rtu_get_humidity(int8_t* out);

void rtu_get_raw_values_string(char* out, uint16_t out_buffer_ln, uint8_t* generated_string_ln);

#endif /* INCLUDE_MODBUS_RTU_RTU_GETTERS_H_ */
