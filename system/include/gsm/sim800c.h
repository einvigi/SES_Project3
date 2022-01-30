/*
 * sim800c.h
 *
 *  Created on: Jan 18, 2022
 *      Author: mateusz
 */

#ifndef INCLUDE_GSM_SIM800C_H_
#define INCLUDE_GSM_SIM800C_H_

#include "drivers/serial.h"
#include "gsm/sim800_state_t.h"

extern const char * gsm_at_command_sent_last;

extern char gsm_sim800_sim_status[10];
extern char gsm_sim800_registered_network[16];
extern int8_t gsm_sim800_signal_level_dbm;
extern float gsm_sim800_bcch_frequency;
extern char gsm_sim800_cellid[5];
extern char gsm_sim800_lac[5];

inline void replace_non_printable_with_space(char * str) {
	for (int i = 0; *(str + i) != 0 ; i++) {
		char current = *(str + i);

		if (current != 0x00) {
			if (current < 0x21 || current > 0x7A) {
				*(str + i) = ' ';
			}
		}
	}
}

void gsm_sim800_init(gsm_sim800_state_t * state, uint8_t enable_echo);

void gsm_sim800_initialization_pool(srl_context_t * srl_context, gsm_sim800_state_t * state);
uint8_t gsm_sim800_rx_terminating_callback(uint8_t current_data, const uint8_t * const rx_buffer, uint16_t rx_bytes_counter);	// callback used to detect echo
void gsm_sim800_rx_done_event_handler(srl_context_t * srl_context, gsm_sim800_state_t * state);
void gsm_sim800_tx_done_event_handler(srl_context_t * srl_context, gsm_sim800_state_t * state);
uint8_t gsm_sim800_get_waiting_for_command_response(void);

#endif /* INCLUDE_GSM_SIM800C_H_ */