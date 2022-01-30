#include "gsm/sim800c_gprs.h"
#include "gsm/sim800c.h"

#include "drivers/serial.h"

#include <stdint.h>
#include <string.h>


const char * START_CONFIG_APN 			= "AT+CSTT=\0";
const char * SHUTDOWN_GPRS 				= "AT+CIPSHUT\r\0";
const char * SHUTDOWN_GRPS_RESPONSE 	= "SHUT OK\0";
const char * ENABLE_EDGE				= "AT+CEGPRS=1,10\r\0";
const char * START_GPRS					= "AT+CIICR\r\0";
const char * GET_IP_ADDRESS				= "AT+CIFSR\r\0";
const char * GET_CONNECTION_STATUS		= "AT+CIPSTATUS\r\0";


static const char * OK = "OK\r\n\0";
static const char * QUOTATION = "\"\0";
static const char * COMMA = ",\0";
static const char * NEWLINE = "\r\0";
static const char * STATE = "STATE\0";

config_data_gsm_t * gsm_sim800_gprs_config_gsm;

int8_t gsm_sim800_gprs_ready = 0;

char gsm_sim800_ip_address[18];

char gsm_sim800_connection_status_str[24];

void sim800_gprs_initialize(srl_context_t * srl_context, gsm_sim800_state_t * state, config_data_gsm_t * config_gsm) {

	if (*state != SIM800_ALIVE) {
		return;
	}

	if (gsm_sim800_get_waiting_for_command_response() == 1) {
		return;
	}

	gsm_sim800_gprs_config_gsm = config_gsm;

	*state = SIM800_INITIALIZING_GPRS;


}

void sim800_gprs_create_apn_config_str(char * buffer, uint16_t buffer_ln) {

	memset(buffer, 0x00, buffer_ln);

	//append prefix
	strcat(buffer, START_CONFIG_APN);

	//append apn
	strcat(buffer, QUOTATION);
	strncat(buffer, gsm_sim800_gprs_config_gsm->apn, 24);
	strcat(buffer, QUOTATION);

	// if username & password was provided
	if (strlen(gsm_sim800_gprs_config_gsm->username) > 0 && strlen(gsm_sim800_gprs_config_gsm->password) > 0) {
		// append comma and quotation mark
		strcat(buffer, COMMA);
		strcat(buffer, QUOTATION);

		// append username
		strncat(buffer, gsm_sim800_gprs_config_gsm->username, 24);

		strcat(buffer, QUOTATION);
		strcat(buffer, COMMA);
		strcat(buffer, QUOTATION);

		// append password
		strncat(buffer, gsm_sim800_gprs_config_gsm->password, 24);

		// append newline
		strcat(buffer, QUOTATION);


	}

	strcat(buffer, NEWLINE);

}

void sim800_gprs_response_callback(srl_context_t * srl_context, gsm_sim800_state_t * state, uint16_t gsm_response_start_idx) {

	int comparision_result = 0;

	if (gsm_at_command_sent_last == SHUTDOWN_GPRS && srl_context->srl_rx_state != SRL_RX_ERROR) {
		comparision_result = strncmp(SHUTDOWN_GRPS_RESPONSE, (const char *)(srl_context->srl_rx_buf_pointer + gsm_response_start_idx), 7);
	}
	else if (gsm_at_command_sent_last == START_GPRS && srl_context->srl_rx_state != SRL_RX_ERROR) {
		comparision_result = strncmp(OK, (const char *)(srl_context->srl_rx_buf_pointer + gsm_response_start_idx), 2);

	}
	else if (gsm_at_command_sent_last == GET_IP_ADDRESS) {
		memset(gsm_sim800_ip_address, 0, 18);

		strncpy(gsm_sim800_ip_address, (const char *)(srl_context->srl_rx_buf_pointer + gsm_response_start_idx), 18);

		replace_non_printable_with_space(gsm_sim800_ip_address);
	}
	else if (gsm_at_command_sent_last == GET_CONNECTION_STATUS ) {
			// TODO

		//for (comparision_result = gsm_response_start_idx; comparision_result > 0 && (strncmp(STATE, (const char *)(srl_context->srl_rx_buf_pointer + gsm_response_start_idx), 5)); comparision_result--);

		/**
		 * AT+CIPSTATUS
OK

STATE: IP STATUS
		 *
		 */

		memset(gsm_sim800_connection_status_str, 0x00, 24);
	}
	else if (srl_context->srl_rx_state == SRL_RX_DONE || srl_context->srl_rx_state == SRL_RX_IDLE){
		comparision_result = strncmp(OK, (const char *)(srl_context->srl_rx_buf_pointer + gsm_response_start_idx), 2);
	}

	if (comparision_result != 0) {
		*state = SIM800_NOT_YET_COMM;
	}
}
